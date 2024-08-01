//
// Created by zj on 7/31/2024.
//

#pragma once
#include "tools.h"
#include "common.h"

namespace RS_Mem {
struct RS_Load_Entry {
    Bit<1>            busy;
    Bit<3>            op; // func3
    Bit<32>           Vj; // rs1, position
    Bit<ROB_SIZE_LOG> Qj;
    Bit<ROB_SIZE_LOG> Ql; // last store operation
    Bit<ROB_SIZE_LOG> dest;
    Bit<12>           offset;
};

struct RS_Store_Entry {
    Bit<1>            busy;
    Bit<3>            op; // func3
    Bit<32>           Vj; // rs1, position
    Bit<32>           Vk; // rs2, value
    Bit<ROB_SIZE_LOG> Qj;
    Bit<ROB_SIZE_LOG> Qk;
    Bit<ROB_SIZE_LOG> Ql; // last store operation
    Bit<ROB_SIZE_LOG> Qm; // last branch operation
    Bit<ROB_SIZE_LOG> dest;
    Bit<12>           offset;
};

struct Load_Operation_Input {
    Wire<1>            enabled;
    Wire<3>            op; // func3
    Wire<32>           Vj; // rs1, position
    Wire<ROB_SIZE_LOG> Qj;
    // Wire<ROB_SIZE_LOG> Ql; // calculated in RS
    Wire<ROB_SIZE_LOG> dest;
    Wire<12>           offset;
};

struct Store_Operation_Input {
    Wire<1>            enabled;
    Wire<3>            op; // func3
    Wire<32>           Vj; // rs1, position
    Wire<32>           Vk; // rs2, value
    Wire<ROB_SIZE_LOG> Qj;
    Wire<ROB_SIZE_LOG> Qk;
    // Wire<ROB_SIZE_LOG> Ql; // calculated in RS
    Wire<ROB_SIZE_LOG> Qm;
    Wire<ROB_SIZE_LOG> dest;
    Wire<12>           offset;
};

struct RS_Input {
    Load_Operation_Input  load_input; // At most one of the 2 inputs can be enabled
    Store_Operation_Input store_input;
    CDB_Input             cdb_input_alu;
    CDB_Input             cdb_input_mem;
    Commit_Info           rob_commit; // From ROB, used to update Qm
    Wire<1>               recv;
    // From memory, whether the instruction is received. The RS keeps sending the same instruction until it is received
    Wire<1> flush_input; // a flush signal is received on the first cycle, serving as RST
};

struct RS_To_Mem {
    Register<1>            typ; // 0 for load, 1 for store
    Register<3>            op;
    Register<32>           Vj;
    Register<32>           Vk; // 0 for load instruction
    Register<12>           offset;
    Register<ROB_SIZE_LOG> dest; // 0 means disabled
};

struct RS_Output {
    Register<32> load_vacancy;  // could have been `bool is_full`, but that requires combinational logic
    Register<32> store_vacancy; // could have been `bool is_full`, but that requires combinational logic
    RS_To_Mem    to_mem;
};

struct Reservation_Station final : dark::Module<RS_Input, RS_Output> {
    void work() {
        // Handle flush signal first
        if (flush_input == 1) {
            flush();
            return;
        }

        // Add new operation if one is provided
        if (load_input.enabled) {
            add_operation(load_input);
        } else if (store_input.enabled) {
            add_operation(store_input);
        }

        // The last insruction is already received, so no need to re-send.
        if (recv) {
            dark::debug::assert(last_issue_status == 1, "RS_Mem: last instruction is not issued but recv is high");
            if (last_issue_typ == 1) /* store */ {
                // the last instruction is issued sucessfully, so the depency is resolved
                update_last_store_id();
                update_store_dependency();
            }
            clear_last_sent();
        }

        // Update the reservation station with new inputs from the CDB
        update_cdb(cdb_input_alu);
        update_cdb(cdb_input_mem);

        // Update the reservation station with new inputs from the ROB
        update_branch_dependency(rob_commit);

        // Issue operations to the Memory
        // Priority: last operation > load > store
        issue_operation();

        // Update the vacancy count
        write_vacancy();
    }

    void add_operation(const Load_Operation_Input& operation_input) {
        // Look for an available slot in the load reservation station
        for (auto& entry : rs_load) {
            if (!to_unsigned(entry.busy)) {
                // Found an empty slot
                entry.busy   = 1;
                entry.op     = operation_input.op;
                entry.Vj     = operation_input.Vj;
                entry.Qj     = operation_input.Qj;
                entry.Ql     = last_store_id;
                entry.dest   = operation_input.dest;
                entry.offset = operation_input.offset;
                break;
            }
        }
    }

    void add_operation(const Store_Operation_Input& operation_input) {
        // Look for an available slot in the store reservation station
        for (auto& entry : rs_store) {
            if (!to_unsigned(entry.busy)) {
                // Found an empty slot
                entry.busy   = 1;
                entry.op     = operation_input.op;
                entry.Vj     = operation_input.Vj;
                entry.Vk     = operation_input.Vk;
                entry.Qj     = operation_input.Qj;
                entry.Qk     = operation_input.Qk;
                entry.Ql     = last_store_id;
                entry.Qm     = operation_input.Qm;
                entry.dest   = operation_input.dest;
                entry.offset = operation_input.offset;
                break;
            }
        }
        // Update the last store id
        last_store_id = operation_input.dest;
    }

    void update_cdb(const CDB_Input& cdb_input) {
        if (cdb_input.rob_id == 0) return;
        for (auto& entry : rs_load) {
            if (to_unsigned(entry.busy)) {
                if (entry.Qj == cdb_input.rob_id) {
                    entry.Vj = cdb_input.value;
                    entry.Qj = 0;
                }
            }
        }
        for (auto& entry : rs_store) {
            if (to_unsigned(entry.busy)) {
                if (entry.Qj == cdb_input.rob_id) {
                    entry.Vj = cdb_input.value;
                    entry.Qj = 0;
                }
                if (entry.Qk == cdb_input.rob_id) {
                    entry.Vk = cdb_input.value;
                    entry.Qk = 0;
                }
            }
        }
    }

    void flush() {
        for (auto& entry : rs_load) {
            entry.busy   = 0;
            entry.op     = 0;
            entry.Vj     = 0;
            entry.Qj     = 0;
            entry.Ql     = 0;
            entry.dest   = 0;
            entry.offset = 0;
        }

        for (auto& entry : rs_store) {
            entry.busy   = 0;
            entry.op     = 0;
            entry.Vj     = 0;
            entry.Vk     = 0;
            entry.Qj     = 0;
            entry.Qk     = 0;
            entry.Ql     = 0;
            entry.Qm     = 0;
            entry.dest   = 0;
            entry.offset = 0;
        }

        to_mem.typ <= 0;
        to_mem.op <= 0;
        to_mem.Vj <= 0;
        to_mem.Vk <= 0;
        to_mem.dest <= 0;
        to_mem.offset <= 0;

        last_store_id     = 0;
        last_issue_status = 0;
        last_issue_typ    = 0;
        last_issue_rs_id  = 0;
        load_vacancy <= RS_SIZE;
        store_vacancy <= RS_SIZE;
    }

    void issue_operation() {
        if (last_issue_status == 1) {
            // Resend last operation
            if (last_issue_typ == 1) {
                // Store
                issue_store_entry(
                    rs_store[to_unsigned(last_issue_rs_id)]);
            } else {
                // Load
                issue_load_entry(
                    rs_load[to_unsigned(last_issue_rs_id)]);
            }
        } else {
            // Issue load instructions first
            for (auto& entry : rs_load) {
                if (try_issue_entry(entry)) {
                    return;
                }
            }

            if (can_store()) {
                for (auto& entry : rs_store) {
                    if (try_issue_entry(entry)) {
                        return;
                    }
                }
            }

            // Don't issue
            to_mem.typ <= 0;
            to_mem.op <= 0;
            to_mem.Vj <= 0;
            to_mem.Vk <= 0;
            to_mem.offset <= 0;
            to_mem.dest <= 0;
        }
    }

    bool can_store() {
        for (auto &entry : rs_load) {
            if (to_unsigned(entry.busy) && to_unsigned(entry.Ql) == 0) {
                // There is a load instruction whose store depenency has been resolved
                // this instruction must be issued before any store instruction
                return false;
            }
        }
        return true;
    }

    bool try_issue_entry(RS_Load_Entry& entry) {
        if (to_unsigned(entry.busy) && to_unsigned(entry.Qj) == 0 && to_unsigned(entry.Ql) == 0) {
            issue_load_entry(entry);
            return true;
        }
        return false;
    }

    bool try_issue_entry(RS_Store_Entry& entry) {
        if (to_unsigned(entry.busy) && to_unsigned(entry.Qj) == 0 && to_unsigned(entry.Qk) == 0
            && to_unsigned(entry.Ql) == 0 && to_unsigned(entry.Qm) == 0) {
            issue_store_entry(entry);
            return true;
        }
        return false;
    }

    void issue_store_entry(RS_Store_Entry& entry) {
        to_mem.typ <= 1;
        to_mem.op <= entry.op;
        to_mem.Vj <= entry.Vj;
        to_mem.Vk <= entry.Vk;
        to_mem.offset <= entry.offset;
        to_mem.dest <= entry.dest;

        last_issue_status = 1;
        last_issue_typ    = 1;                        // store
        last_issue_rs_id  = rs_store.data() - &entry; // Calculate index
    }

    void issue_load_entry(RS_Load_Entry& entry) {
        to_mem.typ <= 0;
        to_mem.op <= entry.op;
        to_mem.Vj <= entry.Vj;
        to_mem.Vk <= 0;
        to_mem.offset <= entry.offset;
        to_mem.dest <= entry.dest;

        last_issue_status = 1;
        last_issue_typ    = 0;                       // load
        last_issue_rs_id  = rs_load.data() - &entry; // Calculate index
    }

    void clear_last_sent() {
        last_issue_status = 0;
    }

    /// Called when a store instruction is received by the memory, i.e. issued sucessfully
    void update_last_store_id() {
        if (last_store_id == to_mem.dest) {
            last_store_id = 0;
        }
    }

    /// Called when a store instruction is received by the memory, i.e. issued sucessfully
    void update_store_dependency() {
        for (auto& entry : rs_load) {
            if (entry.Ql == to_mem.dest) {
                entry.Ql = 0;
            }
        }
        for (auto& entry : rs_store) {
            if (entry.Ql == to_mem.dest) {
                entry.Ql = 0;
            }
        }
    }

    void update_branch_dependency(const Commit_Info& commit_info) {
        if (commit_info.rob_id == 0) return;

        for (auto& entry : rs_store) {
            if (entry.Qm == commit_info.rob_id) {
                entry.Qm = 0;
            }
        }
    }

    void write_vacancy() {
        unsigned load_vacancy_count = 0;
        for (const auto& entry : rs_load) {
            if (!to_unsigned(entry.busy)) {
                load_vacancy_count++;
            }
        }

        unsigned store_vacancy_count = 0;
        for (const auto& entry : rs_store) {
            if (!to_unsigned(entry.busy)) {
                store_vacancy_count++;
            }
        }

        load_vacancy <= load_vacancy_count;
        store_vacancy <= store_vacancy_count;
    }

private:
    std::array<RS_Load_Entry, RS_SIZE>  rs_load;
    std::array<RS_Store_Entry, RS_SIZE> rs_store;
    Bit<ROB_SIZE_LOG>                   last_store_id; // the ROB id of the latest store instruction, used to update Ql
    Bit<1>                              last_issue_status; // 0 for not issued, 1 for issued
    Bit<1>                              last_issue_typ; // 0 for load, 1 for store
    Bit<RS_SIZE_LOG>                    last_issue_rs_id; // the RS id of the latest issued instruction, used to re-send
};

struct Mem_Operation_Input {
    Wire<1>            typ; // 0 for load, 1 for store
    Wire<3>            op;
    Wire<32>           rs1;
    Wire<32>           rs2; // 0 for load instruction
    Wire<12>           offset;
    Wire<ROB_SIZE_LOG> dest;
};

struct Mem_Input {
    Mem_Operation_Input operation_input;
    Wire<1>             flush_input; // a flush signal is received on the first cycle, serving as RST
};

struct Mem_Output {
    CDB_Output  cdb_output;
    Register<1> recv;
    // whether the instruction is received. The RS keeps sending the same instruction until it is received
};

struct MemoryUnit final : dark::Module<Mem_Input, Mem_Output> {
    explicit MemoryUnit(Memory* memory) : memory(memory), state(0) {}

    void work() {
        if (flush_input == 1) {
            flush();
            return;
        }

        if (state == 0) {
            // Idle state
            if (operation_input.dest != 0) {
                execute_operation(operation_input);
                state++;
                recv <= 1; // Operation received
            } else {
                recv <= 0; // No operation
            }
            cdb_output.rob_id <= 0;
            cdb_output.value <= 0;
        } else if (state == MEMORY_LATENCY) {
            output_result();
            state = 0; // Back to idle
        } else {
            state++;
            recv <= 0; // Still in execution delay
            cdb_output.rob_id <= 0;
            cdb_output.value <= 0;
        }
    }

private:
    Memory*           memory;
    unsigned int      state;  // 0 for idle, 1, 2, ... MEM_LATENCY for busy. Specially, reset the state if flushed
    Bit<ROB_SIZE_LOG> rob_id; // cached for delayed output
    Bit<32>           value;  // cached for delayed output

    void flush() {
        state  = 0;
        rob_id = 0;
        value  = 0;
        recv <= 0;
        cdb_output.rob_id <= 0;
        cdb_output.value <= 0;
    }

    void execute_operation(const Mem_Operation_Input& input) {
        rob_id = input.dest;
        if (input.typ == 0) {
            // Load
            value = load_data(input);
        } else {
            // Store
            value = 0;
            store_data(input);
        }
    }

    Bit<32> load_data(const Mem_Operation_Input& input) {
        unsigned address = to_unsigned(input.rs1 + to_signed(input.offset));
        switch (to_unsigned(input.op)) {
        case 0b000: // LB
            return sign_extend<8, 32>(memory->get_byte(address));
        case 0b001: // LH
            return sign_extend<16, 32>(memory->get_half(address));
        case 0b010: // LW
            return memory->get_word(address);
        case 0b100: // LBU
            return zero_extend<8, 32>(memory->get_byte(address));
        case 0b101: // LHU
            return zero_extend<16, 32>(memory->get_half(address));
        default:
            dark::debug::unreachable();
        }
        return 0;
    }

    void store_data(const Mem_Operation_Input& input) {
        unsigned address = to_unsigned(input.rs1 + to_signed(input.offset));
        switch (to_unsigned(input.op)) {
        case 0b000: // SB
            memory->get_byte(address) = to_unsigned(input.rs2);
            break;
        case 0b001: // SH
            memory->get_half(address) = to_unsigned(input.rs2);
            break;
        case 0b010: // SW
            memory->get_word(address) = to_unsigned(input.rs2);
            break;
        default:
            dark::debug::unreachable();
        }
    }

    void output_result() {
        cdb_output.rob_id <= rob_id;
        cdb_output.value <= value;
        recv <= 0; // Operation complete
    }
};
} // namespace RS_Mem
