//
// Created by zj on 7/31/2024.
//

#pragma once

#include "tools.h"
#include "common.h"

namespace decoder {
struct Input_From_Fetcher {
    Wire<32> instruction;
    Wire<32> program_counter;
    Wire<1>  predicted_branch_taken;
};

struct Input_From_Regfile {
    std::array<Wire<ROB_SIZE_LOG>, 32> rob_id;
    std::array<Wire<32>, 32>           data;
};

struct Input_From_ROB {
    std::array<Wire<32>, ROB_SIZE> value;
    std::array<Wire<1>, ROB_SIZE>  ready;
};

struct Decoder_Input {
    Input_From_Regfile from_regfile;
    Input_From_ROB     from_rob;
    Input_From_Fetcher from_fetcher;
    CDB_Input          cdb_input_alu;
    CDB_Input          cdb_input_mem;
    Wire<1>            rs_alu_full;
    Wire<1>            rs_bcu_full;
    Wire<1>            rs_mem_load_full;
    Wire<1>            rs_mem_store_full;
    Wire<1>            rob_full;
    Wire<ROB_SIZE_LOG> rob_id;
    Commit_Info        commit_info;
    Wire<1>            flush_input;
};

struct Output_To_Fetcher {
    Register<1>  enabled;
    Register<32> pc;

    void write_disable(bool valid = true);
};

struct Output_To_ROB {
    Register<1>  enabled;
    Register<2>  op;          // 00 for jalr, 01 for branch, 10 for others, 11 for special halt instruction
    Register<1>  value_ready; // 1 for value acquired, 0 otherwise
    Register<32> value;       // for jalr, the jump address; for branch and others, the value to write to the register
    Register<32> alt_value;   // for jalr, pc + 4; for branch, pc of the branch; for others, unused
    Register<5>  dest;        // the register to store the value
    Register<1>  predicted_branch_taken;

    void write_disable(bool valid = true);
};

struct Output_To_RS_ALU {
    Register<1>            enabled;
    Register<4>            op; // func7 bit 5 and func3
    Register<32>           Vj;
    Register<32>           Vk;
    Register<ROB_SIZE_LOG> Qj;
    Register<ROB_SIZE_LOG> Qk;
    Register<ROB_SIZE_LOG> dest;

    void write_disable(bool valid = true);
};

struct Output_To_RS_BCU {
    Register<1>            enabled;
    Register<3>            op; // func3
    Register<32>           Vj;
    Register<32>           Vk;
    Register<ROB_SIZE_LOG> Qj;
    Register<ROB_SIZE_LOG> Qk;
    Register<ROB_SIZE_LOG> dest;
    Register<32>           pc_fallthrough;
    Register<32>           pc_target;

    void write_disable(bool valid = true);
};

struct Output_To_RS_Mem_Load {
    Register<1>            enabled;
    Register<3>            op; // func3
    Register<32>           Vj; // rs1, position
    Register<ROB_SIZE_LOG> Qj;
    // Register<ROB_SIZE_LOG> Ql; // calculated in RS
    Register<ROB_SIZE_LOG> dest;
    Register<12>           offset;

    void write_disable(bool valid = true);
};

struct Output_To_RS_Mem_Store {
    Register<1>            enabled;
    Register<3>            op; // func3
    Register<32>           Vj; // rs1, position
    Register<32>           Vk; // rs2, value
    Register<ROB_SIZE_LOG> Qj;
    Register<ROB_SIZE_LOG> Qk;
    // Register<ROB_SIZE_LOG> Ql; // calculated in RS
    Register<ROB_SIZE_LOG> Qm; // last branch id
    Register<ROB_SIZE_LOG> dest;
    Register<12>           offset;

    void write_disable(bool valid = true);
};

struct Output_To_RegFile {
    Register<1>            enabled;
    Register<5>            reg_id;
    Register<ROB_SIZE_LOG> rob_id;

    void write_disable(bool valid = true);
};

struct Decoder_Output {
    Output_To_Fetcher      to_fetcher;
    Output_To_ROB          to_rob;
    Output_To_RS_ALU       to_rs_alu;
    Output_To_RS_BCU       to_rs_bcu;
    Output_To_RS_Mem_Load  to_rs_mem_load;
    Output_To_RS_Mem_Store to_rs_mem_store;
    Output_To_RegFile      to_reg_file;
};


struct Decoder final : dark::Module<Decoder_Input, Decoder_Output> {
    void work() {
        // Instructions to consider: U lui, U auipc, J jal, J jalr, B beq, B bne, B blt, B bge, B bltu, B bgeu, I lb, I lh, I lw, I lbu, I lhu, S sb, S sh, S sw, I addi, I slti, I sltiu, I xori, I ori, I andi, I slli, I srli, I srai, R add, R sub, R sll, R slt, R sltu, R xor, R srl, R sra, R or, R and

        // if flush_input is received, reset and go to state skip 1 cycle, return

        // if state is `skip 1 cycle`, do nothing, go to state `try to issue`, return

        // if state is `wait for jalr`, stay in that state and return unless commit_info.rob_id == last_branch_id.
        // if so, go to state `try to issue`, return

        // unsigned int instruction; = from_fetcher.instruction if in state `try to issue`, last_insturction if in state `issue previous`

        // try to issue the instruction. if failed (something is full), go to state `issue previous`
        // special cases :
        // lui: write to rob only, with `value_ready` set to true
        // auipc: convert to an add instruction
        // jal: write to rob only, with `value_ready` set to true
        // ret: special case of jalr, if x1 is ready, convert it to a jal
        // jalr: write an add instruction to rs_alu, go to state `wait for jalr`

        // remember that every output register must be written exactly once, so call write_disable if nothing to write
        if (flush_input == 1) {
            flush();
            return;
        }

        // Update last_branch_id using commit_info
        if (commit_info.rob_id == last_branch_id) {
            last_branch_id = 0; // No uncommitted branch
        }

        if (state == 0) {
            state = 1;
            disable_all_outputs();
            return;
        } else if (state == 3) {
            if (last_branch_id == 0) {
                state = 1;
            }
            disable_all_outputs();
            return;
        }

        Bit<32> instruction = to_unsigned((state == 1) ? from_fetcher.instruction : last_instruction);
        Bit<32> program_counter = to_unsigned((state == 1) ? from_fetcher.program_counter : last_program_counter);
        Bit<1>  predicted_branch_taken = to_unsigned((state == 1) ? from_fetcher.predicted_branch_taken : last_predicted_branch_taken);

        issue_instruction(instruction, program_counter, predicted_branch_taken);
        last_instruction = instruction;
        last_program_counter = program_counter;
        last_predicted_branch_taken = predicted_branch_taken;
    }

    struct Query_Register_Result {
        Bit<32>           V;
        Bit<ROB_SIZE_LOG> Q;
    };

    Query_Register_Result query_register(unsigned int reg) {
        Bit<ROB_SIZE_LOG> Q = from_regfile.rob_id[reg];

        if (Q == 0) return {from_regfile.data[reg], 0};

        // Check the CDB first
        if (cdb_input_alu.rob_id == Q) {
            return {cdb_input_alu.value, 0};
        }
        if (cdb_input_mem.rob_id == Q) {
            return {cdb_input_mem.value, 0};
        }

        // Check the ROB
        if (to_unsigned(from_rob.ready[to_unsigned(Q)])) {
            return {from_rob.value[to_unsigned(Q)], 0};
        }

        // Otherwise, return the current data and ROB ID from the regfile
        return {0, Q};
    }

    void disable_all_outputs() {
        to_rob.write_disable();
        to_rs_alu.write_disable();
        to_rs_bcu.write_disable();
        to_rs_mem_load.write_disable();
        to_rs_mem_store.write_disable();
        to_fetcher.write_disable();
    }

    void flush() {
        disable_all_outputs();
        state          = 1;
        last_branch_id = 0;
        last_instruction = 0;
        last_program_counter = 0;
        last_predicted_branch_taken = 0;
    }

    void issue_instruction(Bit<32> instruction, Bit<32> program_counter, Bit<1> predicted_branch_taken) {
        // set flags that records whether an output has been written
        // call to_something.write_disable(!flag) in the end
        // Ensure all outputs are correctly marked disabled if not written
        // Initialize all flags to false
        bool rs_alu_written       = false;
        bool rs_bcu_written       = false;
        bool rs_mem_load_written  = false;
        bool rs_mem_store_written = false;
        bool rob_written          = false;
        bool fetcher_written      = false;
        bool reg_file_written     = false;

        unsigned int opcode = to_unsigned(instruction.range<6, 0>());
        Bit<3>       func3  = instruction.range<14, 12>();
        Bit<7>       func7  = instruction.range<31, 25>();
        Bit<5>       rs1    = instruction.range<19, 15>();
        Bit<5>       rs2    = instruction.range<24, 20>();
        Bit<5>       rd     = instruction.range<11, 7>();
        Bit<12>      imm_i  = instruction.range<31, 20>();
        Bit<12>      imm_s  = Bit{instruction.range<31, 25>(), instruction.range<11, 7>()};
        Bit<13>      imm_b  = {
            instruction.range<31, 31>(), instruction.range<7, 7>(), instruction.range<30, 25>(),
            instruction.range<11, 8>(), Bit<1>(0)
        };
        Bit<20> imm_u = instruction.range<31, 12>();
        Bit<21> imm_j = {
            instruction.range<31, 31>(), instruction.range<19, 12>(), instruction.range<20, 20>(),
            instruction.range<30, 21>(), Bit<1>(0)
        };

        auto rs1_result = query_register(to_unsigned(rs1));
        auto rs2_result = query_register(to_unsigned(rs2));

        if (rob_full == 1) {
            issue_failure(program_counter);
            return;
        }
        switch (opcode) {
        case 0b0110111: { // LUI
            Bit<2> op;
            if (instruction == 0x0ff00513) {
                // Special halt instruction to stop the simulator
                op = 3; // type 'halt'
            } else {
                op = 2; // type 'others'
            }

            // Set output to ROB
            to_rob.enabled <= 1;
            to_rob.op <= op;
            to_rob.value_ready <= 1; // value ready
            to_rob.value <= to_unsigned(imm_u) << 12;
            to_rob.alt_value <= 0;
            to_rob.dest <= rd;
            to_rob.predicted_branch_taken <= 0;
            rob_written = true;

            // Reserve ROB entry for this instruction
            to_reg_file.enabled <= 1;
            to_reg_file.reg_id <= rd;
            to_reg_file.rob_id <= rob_id;
            reg_file_written = true;

            break;
        }
        case 0b0010111: { // AUIPC
            if (rs_alu_full == 1) {
                // ALU reservation station is full
                issue_failure(program_counter);
                return;
            }

            // Set output to ROB
            to_rob.enabled <= 1;
            to_rob.op <= 2;          // type 'others'
            to_rob.value_ready <= 0; // value not ready until ALU computes it
            to_rob.value <= 0;       // temporary
            to_rob.alt_value <= 0;
            to_rob.dest <= rd;
            to_rob.predicted_branch_taken <= 0;
            rob_written = true;

            // Reserve ROB entry for this instruction
            to_reg_file.enabled <= 1;
            to_reg_file.reg_id <= rd;
            to_reg_file.rob_id <= rob_id;
            reg_file_written = true;

            // Set output to RS_ALU
            to_rs_alu.enabled <= 1;
            to_rs_alu.op <= 0x000; // Since ALU will perform addition
            to_rs_alu.Vj <= program_counter;
            to_rs_alu.Vk <= to_unsigned(imm_u) << 12;
            to_rs_alu.Qj <= 0;
            to_rs_alu.Qk <= 0;
            to_rs_alu.dest <= rob_id;
            rs_alu_written = true;

            break;
        }
        case 0b1101111: { // JAL
            // Calculate the jump address using sign-extended immediate
            Bit<32> jump_address = program_counter + to_signed(imm_j);

            // Set output to ROB
            to_rob.enabled <= 1;
            to_rob.op <= 2;          // type 'others'
            to_rob.value_ready <= 1; // value ready
            // The jump address isn't written to the ROB, as it's not needed
            to_rob.value <= program_counter + 4;
            to_rob.alt_value <= 0;
            to_rob.dest <= rd;
            to_rob.predicted_branch_taken <= 0; // Not a branch prediction
            rob_written = true;

            // Reserve ROB entry for this instruction
            to_reg_file.enabled <= 1;
            to_reg_file.reg_id <= rd;
            to_reg_file.rob_id <= rob_id;
            reg_file_written = true;

            // Update the program counter with the jump address in the fetcher
            to_fetcher.enabled <= 1;
            to_fetcher.pc <= jump_address;
            fetcher_written = true;

            state = 0; // Skip 1 cycle

            break;
        }
        case 0b1100111: { // JALR
            if (rs1 == 1 && imm_i == 0 && rd == 0) {
                // special case: RET
                // check if x1 is ready, convert it to JAL
                if (rs1_result.Q == 0) {
                    // x1 is ready, treat it as JAL to the address held in x1
                    Bit<32> return_address = rs1_result.V;

                    // Set output to ROB
                    to_rob.enabled <= 1;
                    to_rob.op <= 2;          // type 'others'
                    to_rob.value_ready <= 1; // value ready
                    to_rob.value <= program_counter + 4;
                    to_rob.alt_value <= 0;
                    to_rob.dest <= 0; // unused
                    to_rob.predicted_branch_taken <= 0;
                    rob_written = true;

                    // Update the program counter with the return address
                    to_fetcher.enabled <= 1;
                    to_fetcher.pc <= return_address;
                    fetcher_written = true;

                    state = 0; // Skip 1 cycle
                    break;
                }
            }

            // Regular JALR
            if (rs_alu_full == 1) {
                // ALU reservation station is full
                issue_failure(program_counter);
                return;
            }

            // Set output to ROB
            to_rob.enabled <= 1;
            to_rob.op <= 0;          // type 'jalr'
            to_rob.value_ready <= 0; // value not ready until ALU computes it
            to_rob.value <= 0;       // temporary
            to_rob.alt_value <= program_counter + 4;
            to_rob.dest <= rd;
            to_rob.predicted_branch_taken <= 0;
            rob_written = true;

            // Reserve ROB entry for this instruction
            to_reg_file.enabled <= 1;
            to_reg_file.reg_id <= rd;
            to_reg_file.rob_id <= rob_id;
            reg_file_written = true;

            // Set output to RS_ALU
            to_rs_alu.enabled <= 1;
            to_rs_alu.op <= 0x000; // Since ALU will perform addition for address calculation
            to_rs_alu.Vj <= rs1_result.V;
            to_rs_alu.Vk <= to_signed(imm_i);
            to_rs_alu.Qj <= rs1_result.Q;
            to_rs_alu.Qk <= 0;
            to_rs_alu.dest <= rob_id;
            rs_alu_written = true;

            state          = 3;      // Wait for jalr to complete
            last_branch_id = rob_id; // When the ROB commits this instruction, it will send its rob_id to the decoder

            break;
        }
        case 0b1100011: { // Branch Instructions: BEQ, BNE, BLT, BGE, BLTU, BGEU
            if (rs_bcu_full == 1) {
                // BCU reservation station is full
                issue_failure(program_counter);
                return;
            }

            // Calculate branch target address
            Bit<32> target_address            = program_counter + to_signed(imm_b);
            Bit<32> predicted_program_counter = to_unsigned(predicted_branch_taken)
                                                    ? target_address
                                                    : program_counter + 4;

            // Set output to ROB (registration of branch instruction)
            to_rob.enabled <= 1;
            to_rob.op <= 1;                                   // type 'branch'
            to_rob.value_ready <= 0;                          // value not ready until branch is resolved
            to_rob.value <= 0;                                // temporary
            to_rob.alt_value <= program_counter; // Current pc
            to_rob.dest <= 0;                                 // No register to write to
            to_rob.predicted_branch_taken <= predicted_branch_taken;
            rob_written = true;

            // Prepare to set the fetcher to the predicted next instruction
            to_fetcher.enabled <= 1;
            to_fetcher.pc <= predicted_program_counter;
            fetcher_written = true;

            // Set output to RS_BCU
            to_rs_bcu.enabled <= 1;
            to_rs_bcu.op <= func3;
            to_rs_bcu.Vj <= rs1_result.V;
            to_rs_bcu.Vk <= rs2_result.V;
            to_rs_bcu.Qj <= rs1_result.Q;
            to_rs_bcu.Qk <= rs2_result.Q;
            to_rs_bcu.dest <= rob_id;
            to_rs_bcu.pc_fallthrough <= program_counter + 4;
            to_rs_bcu.pc_target <= target_address;
            rs_bcu_written = true;

            state          = 0;      // Skip 1 cycle
            last_branch_id = rob_id; // Updates the last_branch_id

            break;
        }
        case 0b0000011: { // Load Instructions: LB, LH, LW, LBU, LHU
            if (rs_mem_load_full == 1) {
                // Load reservation station is full
                issue_failure(program_counter);
                return;
            }

            // Set output to ROB (registration of load instruction)
            to_rob.enabled <= 1;
            to_rob.op <= 2;          // type 'others'
            to_rob.value_ready <= 0; // value not ready until loaded from memory
            to_rob.value <= 0;       // temporary
            to_rob.alt_value <= 0;   // unused
            to_rob.dest <= rd;
            to_rob.predicted_branch_taken <= 0; // Not a branch
            rob_written = true;

            // Reserve ROB entry for this instruction
            to_reg_file.enabled <= 1;
            to_reg_file.reg_id <= rd;
            to_reg_file.rob_id <= rob_id;
            reg_file_written = true;

            // Set output to RS_Mem_Load
            to_rs_mem_load.enabled <= 1;
            to_rs_mem_load.op <= func3;
            to_rs_mem_load.Vj <= rs1_result.V;
            to_rs_mem_load.Qj <= rs1_result.Q;
            to_rs_mem_load.dest <= rob_id;
            to_rs_mem_load.offset <= imm_i;
            rs_mem_load_written = true;

            break;
        }

        case 0b0100011: { // Store Instructions: SB, SH, SW
            if (rs_mem_store_full == 1) {
                // Store reservation station is full
                issue_failure(program_counter);
                return;
            }

            // Set output to ROB (registration of store instruction)
            to_rob.enabled <= 1;
            to_rob.op <= 2;                     // type 'others'
            to_rob.value_ready <= 0;            // value not ready until written to memory
            to_rob.value <= 0;                  // temporary
            to_rob.alt_value <= 0;              // unused
            to_rob.dest <= 0;                   // No destination register for store
            to_rob.predicted_branch_taken <= 0; // Not a branch prediction
            rob_written = true;

            // Set output to RS_Mem_Store
            to_rs_mem_store.enabled <= 1;
            to_rs_mem_store.op <= func3;
            to_rs_mem_store.Vj <= rs1_result.V;
            to_rs_mem_store.Vk <= rs2_result.V;
            to_rs_mem_store.Qj <= rs1_result.Q;
            to_rs_mem_store.Qk <= rs2_result.Q;
            // Note: Qm for last branch id to resolve branch misprediction
            to_rs_mem_store.Qm <= last_branch_id;
            to_rs_mem_store.dest <= rob_id;
            to_rs_mem_store.offset <= imm_s;
            rs_mem_store_written = true;

            break;
        }
        case 0b0010011: { // I-type ALU Instructions: ADDI, SLTI, SLTIU, XORI, ORI, ANDI, SLLI, SRLI, SRAI
            if (rs_alu_full == 1) {
                // ALU reservation station is full
                issue_failure(program_counter);
                return;
            }

            // Set output to ROB (registration of ALU I-type instruction)
            to_rob.enabled <= 1;
            to_rob.op <= 2;          // type 'others'
            to_rob.value_ready <= 0; // value not ready until ALU computes it
            to_rob.value <= 0;       // temporary
            to_rob.alt_value <= 0;   // unused
            to_rob.dest <= rd;
            to_rob.predicted_branch_taken <= 0; // Not a branch
            rob_written = true;

            // Reserve ROB entry for this instruction
            to_reg_file.enabled <= 1;
            to_reg_file.reg_id <= rd;
            to_reg_file.rob_id <= rob_id;
            reg_file_written = true;

            // Set output to RS_ALU
            to_rs_alu.enabled <= 1;
            if (func3 == 0b001 || func3 == 0b101) {                      // SLLI, SRLI, SRAI
                to_rs_alu.op <= Bit{instruction.range<30, 30>(), func3}; // func7 bit 5 and func3 define ALU operation
            } else {                                                     // ADDI, SLTI, SLTIU, XORI, ORI, ANDI
                to_rs_alu.op <= Bit{Bit<1>{0}, func3};
            }
            to_rs_alu.Vj <= rs1_result.V;
            to_rs_alu.Vk <= ((func3 == 0b001 || func3 == 0b101)
                                 ? to_unsigned(instruction.range<24, 20>())
                                 : to_signed(imm_i));
            to_rs_alu.Qj <= rs1_result.Q;
            to_rs_alu.Qk <= 0;
            to_rs_alu.dest <= rob_id;
            rs_alu_written = true;

            break;
        }
        case 0b0110011: { // R-type ALU Instructions: ADD, SUB, SLL, SLT, SLTU, XOR, SRL, SRA, OR, AND
            if (rs_alu_full == 1) {
                // ALU reservation station is full
                issue_failure(program_counter);
                return;
            }

            // Set output to ROB (registration of ALU R-type instruction)
            to_rob.enabled <= 1;
            to_rob.op <= 2;          // type 'others'
            to_rob.value_ready <= 0; // value not ready until ALU computes it
            to_rob.value <= 0;       // temporary
            to_rob.alt_value <= 0;   // unused
            to_rob.dest <= rd;
            to_rob.predicted_branch_taken <= 0; // Not a branch
            rob_written = true;

            // Reserve ROB entry for this instruction
            to_reg_file.enabled <= 1;
            to_reg_file.reg_id <= rd;
            to_reg_file.rob_id <= rob_id;
            reg_file_written = true;

            // Set output to RS_ALU
            to_rs_alu.enabled <= 1;
            to_rs_alu.op <= Bit{func7[5], func3}; // func7 bit 5 and func3 define ALU operation
            to_rs_alu.Vj <= rs1_result.V;
            to_rs_alu.Vk <= rs2_result.V;
            to_rs_alu.Qj <= rs1_result.Q;
            to_rs_alu.Qk <= rs2_result.Q;
            to_rs_alu.dest <= rob_id;
            rs_alu_written = true;

            break;
        }

        default:
            dark::debug::unreachable();
        }


        to_rs_alu.write_disable(!rs_alu_written);
        to_rs_bcu.write_disable(!rs_bcu_written);
        to_rs_mem_load.write_disable(!rs_mem_load_written);
        to_rs_mem_store.write_disable(!rs_mem_store_written);
        to_rob.write_disable(!rob_written);
        to_fetcher.write_disable(!fetcher_written);
        to_reg_file.write_disable(!reg_file_written);
    }

    void issue_failure(Bit<32> program_counter) {
        state = 2; // Try to issue previous instruction

        to_fetcher.enabled <= 1;
        to_fetcher.pc <= program_counter + 4;
        // so that once the previous instruction is issued, the decoder will receive the next instruction in the next cycle

        to_rob.write_disable();
        to_rs_alu.write_disable();
        to_rs_bcu.write_disable();
        to_rs_mem_load.write_disable();
        to_rs_mem_store.write_disable();
    }

private:
    unsigned int state = 0; // 0 for `skip 1 cycle`, 1 for `try to issue`, 2 for `issue previous`, 3 for `wait for jalr`
    // TODO: use enum class for state
    Bit<ROB_SIZE_LOG> last_branch_id; // the rob_id of the last uncommitted branch instruction, used in RS_Mem_Store
    Bit<32>           last_instruction;
    Bit<32>           last_program_counter;
    Bit<1>            last_predicted_branch_taken;
};

inline void Output_To_Fetcher::write_disable(bool valid) {
    if (valid) {
        enabled <= 0;
        pc <= 0;
    }
}

inline void Output_To_ROB::write_disable(bool valid) {
    if (valid) {
        enabled <= 0;
        op <= 3; // 11 unused value to reflect disable
        value_ready <= 0;
        value <= 0;
        alt_value <= 0;
        dest <= 0;
        predicted_branch_taken <= 0;
    }
}

inline void Output_To_RS_ALU::write_disable(bool valid) {
    if (valid) {
        enabled <= 0;
        op <= 0;
        Vj <= 0;
        Vk <= 0;
        Qj <= 0;
        Qk <= 0;
        dest <= 0;
    }
}

inline void Output_To_RS_BCU::write_disable(bool valid) {
    if (valid) {
        enabled <= 0;
        op <= 0;
        Vj <= 0;
        Vk <= 0;
        Qj <= 0;
        Qk <= 0;
        dest <= 0;
        pc_fallthrough <= 0;
        pc_target <= 0;
    }
}

inline void Output_To_RS_Mem_Load::write_disable(bool valid) {
    if (valid) {
        enabled <= 0;
        op <= 0;
        Vj <= 0;
        Qj <= 0;
        dest <= 0;
        offset <= 0;
    }
}

inline void Output_To_RS_Mem_Store::write_disable(bool valid) {
    if (valid) {
        enabled <= 0;
        op <= 0;
        Vj <= 0;
        Vk <= 0;
        Qj <= 0;
        Qk <= 0;
        Qm <= 0;
        dest <= 0;
        offset <= 0;
    }
}

inline void Output_To_RegFile::write_disable(bool valid) {
    if (valid) {
        enabled <= 0;
        reg_id <= 0;
        rob_id <= 0;
    }
}
} // namespace decoder
