#include "alu.h"
#include "tools.h"
#include <iostream>
#include <unordered_map>

int main() {
	std::string opstring;

	max_size_t opcode;
	max_size_t issue;
	max_size_t rs1;
	max_size_t rs2;

	dark::CPU cpu;
	AluModule alu;
	alu.opcode = [&]() { return opcode; };
	alu.issue = [&]() { return issue; };
	alu.rs1 = [&]() { return rs1; };
	alu.rs2 = [&]() { return rs2; };
	cpu.add_module(&alu);

	std::unordered_map<std::string, AluOpcode> cmd2op = {
			{"add", AluOpcode::ADD},
			{"sub", AluOpcode::SUB},
			{"sll", AluOpcode::SLL},
			{"src", AluOpcode::SRL},
			{"sra", AluOpcode::SRA},
			{"and", AluOpcode::AND},
			{"or", AluOpcode::OR},
			{"xor", AluOpcode::XOR},
			{"slt", AluOpcode::SLT},
			{"sltu", AluOpcode::SLTU},
			{"sge", AluOpcode::SGE},
			{"sgeu", AluOpcode::SGEU},
			{"seq", AluOpcode::SEQ},
			{"sneq", AluOpcode::SNEQ}};
	while (std::cin >> opstring) {
		if (cmd2op.find(opstring) == cmd2op.end()) {
			std::cout << "Invalid opcode" << std::endl;
			issue = 0;
		}
		else {
			issue = 1;
			std::cin >> rs1 >> rs2;
		}
		opcode = static_cast<max_size_t>(cmd2op[opstring]);
		cpu.run_once();
		std::cout << "out: " << static_cast<unsigned int>(alu.out) << std::endl;
		std::cout << "done: " << static_cast<unsigned int>(alu.done) << std::endl;
	}
	return 0;
}