//
// Created by zj on 7/31/2024.
//

#pragma once
#include "tools.h"
#include "common.h"

namespace RS_BCU {
struct RS_Entry {
    Bit<1>            busy;
    Bit<3>            op; // func3
    Bit<32>           Vj;
    Bit<32>           Vk;
    Bit<ROB_SIZE_LOG> Qj;
    Bit<ROB_SIZE_LOG> Qk;
    Bit<ROB_SIZE_LOG> dest;
    Bit<32>           pc_fallthrough;
    Bit<32>           pc_target;
};

struct Operation_Input {
    Wire<1>            enabled;
    Wire<3>            op; // func3
    Wire<32>           Vj;
    Wire<32>           Vk;
    Wire<ROB_SIZE_LOG> Qj;
    Wire<ROB_SIZE_LOG> Qk;
    Wire<ROB_SIZE_LOG> dest;
    Wire<32>           pc_fallthrough;
    Wire<32>           pc_target;
};

struct RS_Input {
    Operation_Input operation_input;
    CDB_Input       cdb_input_alu;
    CDB_Input       cdb_input_mem;
    Wire<1>         flush_input; // a flush signal is received on the first cycle, serving as RST
};

struct RS_To_BCU {
    Register<3>            op;
    Register<32>           Vj;
    Register<32>           Vk;
    Register<ROB_SIZE_LOG> dest; // 0 means disabled
    Register<32>           pc_fallthrough;
    Register<32>           pc_target;
};

struct RS_Output {
    Register<32> vacancy; // could have been `bool is_full`, but that requires combinational logic
    RS_To_BCU    to_bcu;
};

struct Reservation_Station final : dark::Module<RS_Input, RS_Output> {
    void work() {
        // Handle flush signal first
        if (flush_input == 1) {
            flush();
            return;
        }

        // Add new operation if one is provided
        if (operation_input.enabled) {
            add_operation(operation_input);
        }

        // Update the reservation station with new inputs from the CDB
        update_cdb(cdb_input_alu);
        update_cdb(cdb_input_mem);

        // Issue operations to the BCU
        issue_operation();

        // Update the vacancy count
        write_vacancy();
    }

    void add_operation(const Operation_Input& operation_input) {
        // Look for an available slot in the reservation station
        for (auto& entry : rs) {
            if (!to_unsigned(entry.busy)) {
                // Found an empty slot
                entry.busy           = 1;
                entry.op             = operation_input.op;
                entry.Vj             = operation_input.Vj;
                entry.Vk             = operation_input.Vk;
                entry.Qj             = operation_input.Qj;
                entry.Qk             = operation_input.Qk;
                entry.dest           = operation_input.dest;
                entry.pc_fallthrough = operation_input.pc_fallthrough;
                entry.pc_target      = operation_input.pc_target;
                break; // Exit loop once we've added the operation
            }
        }
    }

    void update_cdb(const CDB_Input& cdb_input) {
        if (cdb_input.rob_id == 0) return;
        // Iterate through each entry in the reservation station
        for (auto& entry : rs) {
            // Check if the entry is busy
            if (to_unsigned(entry.busy)) {
                // Check if the entry is waiting for the result that is broadcast on the CDB
                if (entry.Qj == cdb_input.rob_id) {
                    // Update the entry with the new value
                    entry.Vj = cdb_input.value;
                    entry.Qj = 0; // Qj is now available
                }
                if (entry.Qk == cdb_input.rob_id) {
                    // Update the entry with the new value
                    entry.Vk = cdb_input.value;
                    entry.Qk = 0; // Qk is now available
                }
            }
        }
    }

    void flush() {
        for (auto& entry : rs) {
            entry.busy           = 0;
            entry.op             = 0;
            entry.Vj             = 0;
            entry.Vk             = 0;
            entry.Qj             = 0;
            entry.Qk             = 0;
            entry.dest           = 0;
            entry.pc_fallthrough = 0;
            entry.pc_target      = 0;
        }
        vacancy <= RS_SIZE;
        to_bcu.op <= 0;
        to_bcu.Vj <= 0;
        to_bcu.Vk <= 0;
        to_bcu.dest <= 0;
        to_bcu.pc_fallthrough <= 0;
        to_bcu.pc_target <= 0;
    }

    void issue_operation() {
        bool found_operation = false;

        // Iterate through each entry in the reservation station
        for (auto& entry : rs) {
            // Check if the entry is busy and ready to be issued
            if (to_unsigned(entry.busy) && to_unsigned(entry.Qj) == 0 && to_unsigned(entry.Qk) == 0) {
                // Issue the operation to the BCU
                to_bcu.op <= entry.op;
                to_bcu.Vj <= entry.Vj;
                to_bcu.Vk <= entry.Vk;
                to_bcu.dest <= entry.dest;
                to_bcu.pc_fallthrough <= entry.pc_fallthrough;
                to_bcu.pc_target <= entry.pc_target;
                found_operation = true;

                // Mark the entry as no longer busy
                entry.busy = 0;

                break; // Only one operation can be issued in a cycle
            }
        }
        if (!found_operation) {
            to_bcu.op <= 0;
            to_bcu.Vj <= 0;
            to_bcu.Vk <= 0;
            to_bcu.dest <= 0;
            to_bcu.pc_fallthrough <= 0;
            to_bcu.pc_target <= 0;
        }
    }

    void write_vacancy() {
        // Count the number of vacant entries in the reservation station
        unsigned vacancy_count = 0;
        for (const auto& entry : rs) {
            if (!to_unsigned(entry.busy)) {
                vacancy_count++;
            }
        }
        vacancy <= vacancy_count;
    }

private:
    std::array<RS_Entry, RS_SIZE> rs;
};

struct BCU_Input {
    Wire<3>            op;
    Wire<32>           rs1;
    Wire<32>           rs2;
    Wire<ROB_SIZE_LOG> dest;
    Wire<32>           pc_fallthrough;
    Wire<32>           pc_target;
};

/**
 * BCU_Output is not sent to CDB, but to ROB only.
 */
struct BCU_Output {
    Register<ROB_SIZE_LOG> rob_id; // 0 means invalid
    Register<1>            taken;
    Register<32>           value;
};

struct BCU final : dark::Module<BCU_Input, BCU_Output> {
    void work() {
        if (dest == 0) {
            rob_id <= 0;
            taken <= 0;
            value <= 0;
            return;
        }
        unsigned opcode      = to_unsigned(op); // func3
        bool     take_branch = false;

        switch (opcode) {
        case 0b000: // BEQ
            take_branch = (rs1 == rs2);
            break;
        case 0b001: // BNE
            take_branch = (rs1 != rs2);
            break;
        case 0b100: // BLT
            take_branch = (to_signed(rs1) < to_signed(rs2));
            break;
        case 0b101: // BGE
            take_branch = (to_signed(rs1) >= to_signed(rs2));
            break;
        case 0b110: // BLTU
            take_branch = (to_unsigned(rs1) < to_unsigned(rs2));
            break;
        case 0b111: // BGEU
            take_branch = (to_unsigned(rs1) >= to_unsigned(rs2));
            break;
        default:
            dark::debug::unreachable();
        }

        taken <= take_branch;
        value <= (take_branch ? pc_target : pc_fallthrough);
        rob_id <= dest;
    }
};
} // namespace RS_BCU
