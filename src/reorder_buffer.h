//
// Created by zj on 7/31/2024.
//

#pragma once

#include "common.h"
#include "tools.h"

namespace rob {
struct ROB_Entry {
    Bit<1>  busy;
    Bit<2>  op;          // 00 for jalr, 01 for branch, 10 for others, 11 unused
    Bit<1>  value_ready; // 1 for value acquired, 0 otherwise
    Bit<32> value;       // for jalr, the jump address; for branch and others, the value to write to the register
    Bit<32> alt_value;   // for jalr, pc + 4; for branch, pc of the branch; for others, unused
    Bit<5>  dest;        // the register to store the value
    Bit<1>  branch_taken;
    Bit<1>  pred_branch_taken;
};

struct Operation_Input {
    Wire<1>  enabled;
    Wire<2>  op;        // 00 for jalr, 01 for branch, 10 for others, 11 unused
    Wire<1>  status;    // 1 for value acquired, 0 otherwise
    Wire<32> value;     // for jalr, the jump address; for branch and others, the value to write to the register
    Wire<32> alt_value; // for jalr, pc + 4; for branch, pc of the branch; for others, unused
    Wire<5>  dest;      // the register to store the value
    Wire<1>  branch_taken;
    Wire<1>  predicted_branch_taken;
};

struct Input_From_BCU {
    Wire<ROB_SIZE_LOG> rob_id; // 0 means invalid
    Wire<1>            taken;
    Wire<32>           value;
};

struct ROB_Input {
    Operation_Input operation_input;
    CDB_Input       cdb_input_alu;
    CDB_Input       cdb_input_mem;
    Input_From_BCU  bcu_input;
};

struct Output_To_RegFile {
    Register<1>            enabled;
    Register<5>            reg_id;
    Register<32>           data;
    Register<ROB_SIZE_LOG> rob_id;
};

struct Commit_Output {
    Register<ROB_SIZE_LOG> reg_id; // 0 if nothing is committed
};

struct Output_To_Fetcher {
    Register<1>  pc_enabled;
    Register<32> pc;

    Register<32> branch_pc;
    Register<1>  branch_taken;
    Register<1>  branch_record_enabled;
};

struct ROB_Output {
    Output_To_RegFile to_reg_file;
    Commit_Output     commit_output; // to RS and Decoder
    Output_To_Fetcher to_fetcher;
    Register<32>      vacancy;      // could have been `bool is_full`, but that requires combinational logic
    Register<1>       flush_output; // to all
};

struct ROB final : dark::Module<ROB_Input, ROB_Output> {
    void work() {
        static bool is_first_run = true;
        if (is_first_run) {
            flush(0x0, 0x0, false, false);
            return;
        }

        if (operation_input.enabled) {
            add_operation(operation_input);
        }

        // Update the reservation station with new inputs from the CDB
        update_cdb(cdb_input_alu);
        update_cdb(cdb_input_mem);

        // Update the reservation station with new inputs from the BCU
        update_bcu(bcu_input);

        if (rob[to_unsigned(head)].busy == 1 && rob[to_unsigned(head)].value_ready == 1) {
            commit();
        } else {
            to_reg_file.enabled <= 0;
            to_reg_file.reg_id <= 0;
            to_reg_file.data <= 0;
            to_reg_file.rob_id <= 0;

            commit_output.reg_id <= 0;

            to_fetcher.pc_enabled <= 0;
            to_fetcher.pc <= 0;
            to_fetcher.branch_pc <= 0;
            to_fetcher.branch_taken <= 0;
            to_fetcher.branch_record_enabled <= 0;

            flush_output <= 0;

            write_vacancy();
        }
    }

    void flush(Bit<32> new_pc, Bit<32> branch_pc, bool branch_taken, bool write_branch_record) {
        to_reg_file.enabled <= 0;
        to_reg_file.reg_id <= 0;
        to_reg_file.data <= 0;
        to_reg_file.rob_id <= 0;

        commit_output.reg_id <= 0;

        to_fetcher.pc_enabled <= 1;
        to_fetcher.pc <= new_pc;
        to_fetcher.branch_pc <= branch_pc;
        to_fetcher.branch_taken <= branch_taken;
        to_fetcher.branch_record_enabled <= write_branch_record;

        flush_output <= 1;

        for (auto& entry : rob) {
            entry.busy              = 0;
            entry.op                = 0;
            entry.value_ready       = 0;
            entry.value             = 0;
            entry.alt_value         = 0;
            entry.dest              = 0;
            entry.branch_taken      = 0;
            entry.pred_branch_taken = 0;
        }
        head = 1;
        tail = 0;

        vacancy <= ROB_SIZE - 1; // the pos 0 of rob is unused;
    }

    void add_operation(const Operation_Input& op_input) {
        auto& entry             = rob[next_tail(to_unsigned(tail))];
        dark::debug::assert(entry.busy == 0, "ROB: got instruction when buffer is full!");
        entry.busy              = 1;
        entry.op                = op_input.op;
        entry.value_ready       = op_input.status;
        entry.value             = op_input.value;
        entry.alt_value         = op_input.alt_value;
        entry.dest              = op_input.dest;
        entry.branch_taken      = op_input.branch_taken;
        entry.pred_branch_taken = op_input.predicted_branch_taken;
        tail                    = next_tail(to_unsigned(tail));
    }

    void update_cdb(const CDB_Input& cdb_input) {
        if (cdb_input.rob_id == 0) return;
        for (auto& entry : rob) {
            if (entry.busy == 1 && entry.value_ready == 0) {
                if (to_unsigned(cdb_input.rob_id) == &entry - &rob[0]) {
                    entry.value = cdb_input.value;
                    entry.value_ready = 1;
                }
            }
        }
    }

    void update_bcu(const Input_From_BCU& bcu_input) {
        if (bcu_input.rob_id == 0) return;
        auto& entry = rob[to_unsigned(bcu_input.rob_id)];
        if (entry.busy == 1 && entry.value_ready == 0 && entry.op == 0b01) { // branch operation
            if (to_unsigned(bcu_input.rob_id) == &entry - &rob[0]) {
                entry.value = bcu_input.value;
                entry.value_ready = 1;
                entry.branch_taken = bcu_input.taken;
            }
        }
    }

    void commit() {
        // TODO: handle the special halt instruction
        auto& entry = rob[to_unsigned(head)];

        // Handle different operation types
        if (entry.op == 0b00) { // jalr operation
            to_fetcher.pc_enabled <= 1;
            to_fetcher.pc <= entry.value;
            to_fetcher.branch_pc <= 0;
            to_fetcher.branch_taken <= 0;
            to_fetcher.branch_record_enabled <= 0;

            commit_output.reg_id <= head; // inform the decoder to go to `skip next instruction` state

            // Write the return address to the destination register
            to_reg_file.enabled <= 1;
            to_reg_file.reg_id <= entry.dest;
            to_reg_file.data <= entry.alt_value;
            to_reg_file.rob_id <= head;

            flush_output <= 0;
        } else if (entry.op == 0b01) { // branch operation
            if (entry.branch_taken != entry.pred_branch_taken) { // Mis-predicted branch
                flush(entry.value, entry.alt_value, to_unsigned(entry.branch_taken), true);
                return ;
            } else { // Correctly predicted branch
                to_fetcher.pc_enabled <= 0;
                to_fetcher.pc <= 0;
                to_fetcher.branch_pc <= entry.value;
                to_fetcher.branch_taken <= entry.branch_taken;
                to_fetcher.branch_record_enabled <= 1;

                commit_output.reg_id <= head;

                to_reg_file.enabled <= 0;
                to_reg_file.reg_id <= 0;
                to_reg_file.data <= 0;
                to_reg_file.rob_id <= 0;

                flush_output <= 0;
            }
        } else { // other operations
            to_reg_file.enabled <= 1;
            to_reg_file.reg_id <= entry.dest;
            to_reg_file.data <= entry.value;
            to_reg_file.rob_id <= head;

            commit_output.reg_id <= head;

            to_fetcher.pc_enabled <= 0;
            to_fetcher.pc <= 0;
            to_fetcher.branch_pc <= 0;
            to_fetcher.branch_taken <= 0;
            to_fetcher.branch_record_enabled <= 0;

            flush_output <= 0;
        }

        // Update the head pointer and mark the entry as not busy
        entry.busy = 0;
        entry.op = 0;
        entry.value_ready = 0;
        entry.value = 0;
        entry.alt_value = 0;
        entry.dest = 0;
        entry.branch_taken = 0;
        entry.pred_branch_taken = 0;
        head = next_tail(to_unsigned(head));

        write_vacancy();
    }

    void write_vacancy() {
        unsigned vacancy_count = 0;
        for (const auto& entry : rob) {
            if (!to_unsigned(entry.busy)) {
                vacancy_count++;
            }
        }
        vacancy <= vacancy_count - 1; // account for the unused entry at position 0
    }

    static unsigned int next_tail(unsigned int tail) {
        return (tail == ROB_SIZE - 1) ? 1 : tail + 1;
    }

private:
    std::array<ROB_Entry, ROB_SIZE> rob; // the pos 0 of rob is unused!
    Bit<ROB_SIZE_LOG>               head;
    Bit<ROB_SIZE_LOG>               tail;
};
} // namespace rob
