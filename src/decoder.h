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

struct Decoder_Input {
    Input_From_Fetcher from_fetcher;
    CDB_Input          cdb_input_alu;
    CDB_Input          cdb_input_mem;
    Wire<1>            rs_alu_full;
    Wire<1>            rs_bcu_full;
    Wire<1>            rs_mem_full;
    Wire<1>            rob_full;
    Commit_Info        commit_info;
    Wire<1>            flush_input;
};

struct Output_To_Fetcher {
    Register<1>  enabled;
    Register<32> pc;

    void write_disable();
};

struct Output_To_ROB {
    Register<1>  enabled;
    Register<2>  op;        // 00 for jalr, 01 for branch, 10 for others, 11 unused
    Register<1>  status;    // 1 for value acquired, 0 otherwise
    Register<32> value;     // for jalr, the jump address; for branch and others, the value to write to the register
    Register<32> alt_value; // for jalr, pc + 4; for branch, pc of the branch; for others, unused
    Register<5>  dest;      // the register to store the value
    Register<1>  branch_taken;
    Register<1>  predicted_branch_taken;

    void write_disable();
};

struct Output_To_RS_ALU {
    Register<1>            enabled;
    Register<4>            op; // the bit 30 of func7, and func3
    Register<32>           Vj;
    Register<32>           Vk;
    Register<ROB_SIZE_LOG> Qj;
    Register<ROB_SIZE_LOG> Qk;
    Register<ROB_SIZE_LOG> dest;

    void write_disable();
};
struct Output_To_RS_BCU {
    Register<3>            op; // func3
    Register<32>           Vj;
    Register<32>           Vk;
    Register<ROB_SIZE_LOG> Qj;
    Register<ROB_SIZE_LOG> Qk;
    Register<ROB_SIZE_LOG> dest;
    Register<32>           pc_fallthrough;
    Register<32>           pc_target;

    void write_disable();
};
struct Output_To_RS_Mem_Load {
    Register<1>            enabled;
    Register<3>            op; // func3
    Register<32>           Vj; // rs1, position
    Register<ROB_SIZE_LOG> Qj;
    // Register<ROB_SIZE_LOG> Ql; // calculated in RS
    Register<ROB_SIZE_LOG> dest;
    Register<12>           offset;

    void write_disable();

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

    void write_disable();
};

struct Output_To_RegFile {
    Register<1>            enabled;
    Register<ROB_SIZE_LOG> rob_id;

    void write_disable();
};

struct Decoder_Output {
    Output_To_Fetcher to_fetcher;
    Output_To_RS_ALU to_rs_alu;
    Output_To_RS_BCU to_rs_bcu;
    Output_To_RS_Mem_Load to_rs_mem_load;
    Output_To_RS_Mem_Store to_rs_mem_store;
};


struct Decoder final : dark::Module<Decoder_Input, Decoder_Output> {
    void work() {
        // Instructions to consider: U lui, U auipc, J jal, J jalr, B beq, B bne, B blt, B bge, B bltu, B bgeu, I lb, I lh, I lw, I lbu, I lhu, S sb, S sh, S sw, I addi, I slti, I sltiu, I xori, I ori, I andi, I slli, I srli, I srai, R add, R sub, R sll, R slt, R sltu, R xor, R srl, R sra, R or, R and

        // if flush_input is received, reset and go to state skip 1 cycle, return

        // if state is `skip 1 cycle`, do nothing, go to state `try to issue`, return

        // if state is `wait for jalr`, stay in that state and return unless commit_info.rob_id == last_branch_id.
        // if so, go to state `try to issue`, return

        unsigned int instruction; // = from_fetcher.instruction if in state `try to issue`, last_insturction if in state `issue previous`

        // try to issue the instruction. if failed (something is full), go to state `issue previous`
        // special cases :
        // lui: write to rob only, with `value_ready` set to true
        // auipc: convert to an add instruction
        // jal: write to rob only, with `value_ready` set to true
        // ret: special case of jalr, if x1 is ready, convert it to a jal
        // jalr: write an add instruction to rs_alu, go to state `wait for jalr`

        // remember that every output register must be written exactly once, so call write_disable if nothing to write
    }

    struct Query_Register_Result {
        Bit<32> V;
        Bit<32> Q;
    };
    Query_Register_Result query_register(Bit<3> reg) {
        return {};
    }

    void flush() {}
private:
    unsigned int state = 0; // 0 for `skip 1 cycle`, 1 for `try to issue`, 2 for `issue previous`, 3 for `wait for jalr`
    Bit<ROB_SIZE_LOG> last_branch_id; // used in RS_Mem_Store
    Bit<32> last_instruction;
};

} // namespace decoder
