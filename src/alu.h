//
// Created by zj on 7/29/2024.
//

#pragma once
#include "tools.h"

// RISC-V
enum class AluOpcode : max_size_t {
	ADD,
	SUB,
	SLL,
	SRL,
	SRA,
	AND,
	OR,
	XOR,
	SLT,
	SLTU,
	SGE,
	SGEU,
	SEQ,
	SNEQ
};

// Normally, only wire can be used in the input.
struct AluInput {
	Wire<8> opcode;
	Wire<1> issue;
	Wire<32> rs1;
	Wire<32> rs2;
};

struct AluOutput {
	Register<32> out;
	Register<1> done;
};

struct AluModule : dark::Module<AluInput, AluOutput> {
	void work() override {
		if (issue) {
			switch (static_cast<AluOpcode>(static_cast<unsigned>(opcode))) {
				using enum AluOpcode;
				case ADD: out <= (rs1 + rs2); break;
				case SUB: out <= (rs1 - rs2); break;
				case SLL: out <= (rs1 << rs2); break;
				case SRL: out <= (rs1 >> rs2); break;
				case SRA: out <= (to_signed(rs1) >> to_unsigned(rs2));
				case AND: out <= (rs1 & rs2); break;
				case OR: out <= (rs1 | rs2); break;
				case XOR: out <= (rs1 ^ rs2); break;
				case SLT: out <= (to_signed(rs1) < to_signed(rs2)); break;
				case SLTU: out <= (rs1 < rs2); break;
				case SGE: out <= (to_signed(rs1) >= to_signed(rs2)); break;
				case SGEU: out <= (rs1 >= rs2); break;
				case SEQ: out <= (rs1 == rs2); break;
				case SNEQ: out <= (rs1 != rs2); break;
				default: dark::debug::assert(false, "Invalid opcode");
			}
			done <= 1;
		}
		else {
			done <= 0;
		}
	}
};
