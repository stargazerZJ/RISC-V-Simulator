//
// Created by zj on 7/29/2024.

#include <iomanip>
#include <iostream>

#include "memory.h"
#include "tools.h"

namespace instructions {
    struct I {
        Bit<7> opcode;
        Bit<5> rd;
        Bit<3> funct3;
        Bit<5> rs1;
        Bit<12> imm;
    };

    I decode_I(Bit<32> instruction);

    struct R {
        Bit<7> opcode;
        Bit<5> rd;
        Bit<3> funct3;
        Bit<5> rs1;
        Bit<5> rs2;
        Bit<7> funct7;
    };

    R decode_R(Bit<32> instruction);

    struct S {
        Bit<7> opcode;
        Bit<12> imm;
        Bit<3> funct3;
        Bit<5> rs1;
        Bit<5> rs2;
    };

    S decode_S(Bit<32> instruction);

    struct B {
        Bit<7> opcode;
        Bit<13> imm; // the last bit is always 0
        Bit<5> rs1;
        Bit<3> funct3;
        Bit<5> rs2;
    };

    B decode_B(Bit<32> instruction);

    struct U {
        Bit<7> opcode;
        Bit<5> rd;
        Bit<20> imm;
    };

    U decode_U(Bit<32> instruction);

    struct J {
        Bit<7> opcode;
        Bit<5> rd;
        Bit<21> imm; // the last bit is always 0
    };

    J decode_J(Bit<32> instruction);

    struct I_star {
        Bit<7> opcode;
        Bit<5> rd;
        Bit<3> funct3;
        Bit<5> rs1;
        Bit<5> imm;
        Bit<7> funct7;
    };

    I_star decode_I_star(Bit<32> instruction);

    unsigned int get_opcode(Bit<32> instruction);
} // namespace instructions

/**
 * RISC-V interpreter
 * It supports a part of RV32I instruction set.
 */
class Interpreter {
public:
    explicit Interpreter(Memory* memory) : memory_(memory), program_counter_(0) {}

    uint8_t run(unsigned int max_instructions);

private:
    Memory* memory_;
    std::array<Bit<32>, 32> register_;
    uint32_t program_counter_;

    Bit<32>& get_register(Bit<5> index) { return register_[to_unsigned(index)]; }
    unsigned get_register_unsigned(Bit<5> index) { return to_unsigned(register_[to_unsigned(index)]); }
};

uint8_t Interpreter::run(unsigned int max_instructions) {
    auto log = [&](uint32_t pc, uint32_t reg_val, Bit<5> reg_id) {
        std::cerr << std::setw(8) << std::setfill(' ') << std::hex << pc << ": "
                  << std::setw(8) << reg_val << " -> "
                  << "r" << std::setw(2) << to_unsigned(reg_id) << std::endl;
    };
    auto log_branch = [&](uint32_t pc, bool taken, uint32_t target) {
        std::cerr << std::setw(8) << std::setfill(' ') << std::hex << pc << ": "
                  << "Branched to "<< std::setw(8) << target << std::endl;
    };
    for (unsigned instruction_count = 0; instruction_count < max_instructions; instruction_count++) {
        Bit<32> instruction = memory_->get_word(program_counter_);

        // if (program_counter_ == 0x1000) {
        //     std::cerr << std::setw(8) << std::setfill(' ') << std::hex << to_unsigned(get_register(15))
        //     << std::endl;
        // }

        // std::cerr << std::setw(8) << std::setfill(' ') << std::hex << program_counter_
        // << ": " << std::setw(8) << std::setfill(' ') << to_unsigned(instruction)
        // << " a0 " << std::setw(8) << std::setfill(' ') << to_unsigned(get_register(10))
        // << " a1 " << std::setw(8) << std::setfill(' ') << to_unsigned(get_register(11))
        // << " a2 " << std::setw(8) << std::setfill(' ') << to_unsigned(get_register(12))
        // << " a3 " << std::setw(8) << std::setfill(' ') << to_unsigned(get_register(13))
        // << std::endl;

        if (instruction == 0x0ff00513) {
            // special instruction to return the result and halt the simulator
            // std::cerr << "No. of instructions: " << std::dec << instruction_count << std::endl;
            return to_unsigned(register_[10].range<7, 0>());
        }

        get_register(0x0) = 0; // hard-wired zero

        auto opcode = instructions::get_opcode(instruction);

        switch (opcode) {
        case 0b0110111: {
            // U-type: LUI
            auto decoded = instructions::decode_U(instruction);
            get_register(decoded.rd) = to_unsigned(decoded.imm) << 12;
            log(program_counter_, to_unsigned(decoded.imm) << 12, decoded.rd);
            program_counter_ += 4;
            break;
        }

        case 0b0010111: {
            // U-type: AUIPC
            auto decoded = instructions::decode_U(instruction);
            get_register(decoded.rd) = program_counter_ + (to_unsigned(decoded.imm) << 12);
            log(program_counter_, program_counter_ + (to_unsigned(decoded.imm) << 12), decoded.rd);
            program_counter_ += 4;
            break;
        }

        case 0b1101111: {
            // J-type: JAL
            auto decoded = instructions::decode_J(instruction);
            get_register(decoded.rd) = program_counter_ + 4;
            log(program_counter_, program_counter_ + 4, decoded.rd);
            program_counter_ += to_signed(decoded.imm);
            break;
        }

        case 0b1100111: {
            // I-type: JALR
            auto decoded = instructions::decode_I(instruction);
            get_register(decoded.rd) = program_counter_ + 4;
            log(program_counter_, program_counter_ + 4, decoded.rd);
            program_counter_ = to_unsigned((get_register(decoded.rs1) + to_signed(decoded.imm)) & ~1);
            break;
        }

        case 0b1100011: {
            // B-type: Branch instructions
            auto decoded = instructions::decode_B(instruction);
            Bit<32> rs1_val = get_register(decoded.rs1);
            Bit<32> rs2_val = get_register(decoded.rs2);
            bool taken = false;

            switch (to_unsigned(decoded.funct3)) {
            case 0b000: // BEQ
                taken = (rs1_val == rs2_val);
                break;
            case 0b001: // BNE
                taken = (rs1_val != rs2_val);
                break;
            case 0b100: // BLT
                taken = to_signed(rs1_val) < to_signed(rs2_val);
                break;
            case 0b101: // BGE
                taken = to_signed(rs1_val) >= to_signed(rs2_val);
                break;
            case 0b110: // BLTU
                taken = to_unsigned(rs1_val) < to_unsigned(rs2_val);
                break;
            case 0b111: // BGEU
                taken = to_unsigned(rs1_val) >= to_unsigned(rs2_val);
                break;
            default:
                dark::debug::unreachable();
            }

            if (taken) {
                log_branch(program_counter_, true, program_counter_ + to_signed(decoded.imm));
                program_counter_ += to_signed(decoded.imm);
            } else {
                log_branch(program_counter_, false, program_counter_ + 4);
                program_counter_ += 4;
            }
            break;
        }

        case 0b0000011: {
            // I-type: Load instructions
            auto decoded = instructions::decode_I(instruction);
            unsigned int addr = to_unsigned(get_register(decoded.rs1) + to_signed(decoded.imm));

            switch (to_unsigned(decoded.funct3)) {
            case 0b000: // LB
                get_register(decoded.rd) = sign_extend<8, 32>(memory_->get_byte(addr));
                break;
            case 0b001: // LH
                get_register(decoded.rd) = sign_extend<16, 32>(memory_->get_half(addr));
                break;
            case 0b010: // LW
                get_register(decoded.rd) = memory_->get_word(addr);
                break;
            case 0b100: // LBU
                get_register(decoded.rd) = zero_extend<8, 32>(memory_->get_byte(addr));
                break;
            case 0b101: // LHU
                get_register(decoded.rd) = zero_extend<16, 32>(memory_->get_half(addr));
                break;
            default:
                dark::debug::unreachable();
            }
            log(program_counter_, get_register_unsigned(decoded.rd), decoded.rd);

            program_counter_ += 4;
            break;
        }

        case 0b0100011: {
            // S-type: Store instructions
            auto decoded = instructions::decode_S(instruction);
            unsigned int addr = to_unsigned(get_register(decoded.rs1) + to_signed(decoded.imm));

            switch (to_unsigned(decoded.funct3)) {
            case 0b000: // SB
                memory_->get_byte(addr) = to_unsigned(get_register(decoded.rs2));
                break;
            case 0b001: // SH
                memory_->get_half(addr) = to_unsigned(get_register(decoded.rs2));
                break;
            case 0b010: // SW
                memory_->get_word(addr) = to_unsigned(get_register(decoded.rs2));
                break;
            default:
                dark::debug::unreachable();
            }
            log(program_counter_, 0, 0);

            program_counter_ += 4;
            break;
        }

        case 0b0010011: {
            // I-type: ALU instructions
            auto decoded = instructions::decode_I(instruction);

            switch (to_unsigned(decoded.funct3)) {
            case 0b000: // ADDI
                get_register(decoded.rd) = to_signed(get_register(decoded.rs1)) + to_signed(decoded.imm);
                break;
            case 0b010: // SLTI
                get_register(decoded.rd) = (to_signed(get_register(decoded.rs1)) < to_signed(decoded.imm)) ? 1 : 0;
                break;
            case 0b011: // SLTIU
                get_register(decoded.rd) = (to_unsigned(get_register(decoded.rs1)) < to_unsigned(decoded.imm)) ? 1 : 0;
                break;
            case 0b100: // XORI
                get_register(decoded.rd) = get_register(decoded.rs1) ^ to_unsigned(decoded.imm);
                break;
            case 0b110: // ORI
                get_register(decoded.rd) = get_register(decoded.rs1) | to_unsigned(decoded.imm);
                break;
            case 0b111: // ANDI
                get_register(decoded.rd) = get_register(decoded.rs1) & to_unsigned(decoded.imm);
                break;
            case 0b001: // SLLI
                get_register(decoded.rd) = get_register_unsigned(decoded.rs1) << (to_unsigned(decoded.imm) & 0b11111);
                break;
            case 0b101: // SRLI/SRAI
                if (to_unsigned(decoded.imm) >> 5 == 0b000000) {
                    get_register(decoded.rd) = to_unsigned(get_register(decoded.rs1)) >> to_unsigned(
                        decoded.imm & 0b11111);
                } else {
                    get_register(decoded.rd) = to_signed(get_register(decoded.rs1)) >> to_unsigned(
                        decoded.imm & 0b11111);
                }
                break;
            default:
                dark::debug::unreachable();
            }
            log(program_counter_, get_register_unsigned(decoded.rd), decoded.rd);

            program_counter_ += 4;
            break;
        }

        case 0b0110011: {
            // R-type: ALU instructions
            auto decoded = instructions::decode_R(instruction);

            switch (to_unsigned(decoded.funct3)) {
            case 0b000: // ADD / SUB
                if (decoded.funct7 == 0b0000000) {
                    // ADD
                    get_register(decoded.rd) = to_signed(get_register(decoded.rs1)) + to_signed(
                        get_register(decoded.rs2));
                } else if (decoded.funct7 == 0b0100000) {
                    // SUB
                    get_register(decoded.rd) = to_signed(get_register(decoded.rs1)) - to_signed(
                        get_register(decoded.rs2));
                } else {
                    dark::debug::unreachable();
                }
                break;
            case 0b001: // SLL
                get_register(decoded.rd) = get_register_unsigned(decoded.rs1) << to_unsigned(get_register(decoded.rs2))
                    &
                    0b11111;
                break;
            case 0b010: // SLT
                get_register(decoded.rd) = (to_signed(get_register(decoded.rs1)) < to_signed(get_register(decoded.rs2)))
                                               ? 1
                                               : 0;
                break;
            case 0b011: // SLTU
                get_register(decoded.rd) = (to_unsigned(get_register(decoded.rs1)) < to_unsigned(
                                               get_register(decoded.rs2)))
                                               ? 1
                                               : 0;
                break;
            case 0b100: // XOR
                get_register(decoded.rd) = get_register(decoded.rs1) ^ get_register(decoded.rs2);
                break;
            case 0b101: // SRL / SRA
                if (decoded.funct7 == 0b0000000) {
                    // SRL
                    get_register(decoded.rd) = to_unsigned(get_register(decoded.rs1)) >> (to_unsigned(
                        get_register(decoded.rs2)) & 0b11111);
                } else if (decoded.funct7 == 0b0100000) {
                    // SRA
                    get_register(decoded.rd) = to_signed(get_register(decoded.rs1)) >> (to_unsigned(
                        get_register(decoded.rs2)) & 0b11111);
                } else {
                    dark::debug::unreachable();
                }
                break;
            case 0b110: // OR
                get_register(decoded.rd) = get_register(decoded.rs1) | get_register(decoded.rs2);
                break;
            case 0b111: // AND
                get_register(decoded.rd) = get_register(decoded.rs1) & get_register(decoded.rs2);
                break;
            default:
                dark::debug::unreachable();
            }
            log(program_counter_, get_register_unsigned(decoded.rd), decoded.rd);

            program_counter_ += 4;
            break;
        }

        default:
            dark::debug::unreachable();
        }
    }
    dark::debug::assert(false, "Interpreter::run: Maximum number of instructions reached");
    return 0;
}

namespace instructions {
    // Decoding for I-Type instruction
    I decode_I(Bit<32> instruction) {
        I decoded;
        decoded.opcode = instruction.range<6, 0>();
        decoded.rd = instruction.range<11, 7>();
        decoded.funct3 = instruction.range<14, 12>();
        decoded.rs1 = instruction.range<19, 15>();
        decoded.imm = instruction.range<31, 20>();
        return decoded;
    }

    // Decoding for R-Type instruction
    R decode_R(Bit<32> instruction) {
        R decoded;
        decoded.opcode = instruction.range<6, 0>();
        decoded.rd = instruction.range<11, 7>();
        decoded.funct3 = instruction.range<14, 12>();
        decoded.rs1 = instruction.range<19, 15>();
        decoded.rs2 = instruction.range<24, 20>();
        decoded.funct7 = instruction.range<31, 25>();
        return decoded;
    }

    // Decoding for S-Type instruction
    S decode_S(Bit<32> instruction) {
        S decoded;
        decoded.opcode = instruction.range<6, 0>();
        Bit<7> imm_11_5 = instruction.range<31, 25>();
        Bit<5> imm_4_0 = instruction.range<11, 7>();
        decoded.imm = Bit{imm_11_5, imm_4_0};
        decoded.funct3 = instruction.range<14, 12>();
        decoded.rs1 = instruction.range<19, 15>();
        decoded.rs2 = instruction.range<24, 20>();
        return decoded;
    }

    // Decoding for B-Type instruction
    B decode_B(Bit<32> instruction) {
        B decoded;
        decoded.opcode = instruction.range<6, 0>();
        Bit<1> imm_11 = instruction.range<31, 31>();
        Bit<6> imm_10_5 = instruction.range<30, 25>();
        Bit<4> imm_4_1 = instruction.range<11, 8>();
        Bit<1> imm_12 = instruction.range<7, 7>();
        decoded.imm = Bit{imm_12, imm_11, imm_10_5, imm_4_1, Bit<1>{0}}; // Last bit is 0
        decoded.funct3 = instruction.range<14, 12>();
        decoded.rs1 = instruction.range<19, 15>();
        decoded.rs2 = instruction.range<24, 20>();
        return decoded;
    }

    // Decoding for U-Type instruction
    U decode_U(Bit<32> instruction) {
        U decoded;
        decoded.opcode = instruction.range<6, 0>();
        decoded.rd = instruction.range<11, 7>();
        Bit<20> imm_31_12 = instruction.range<31, 12>();
        decoded.imm = imm_31_12;
        return decoded;
    }

    // Decoding for J-Type instruction
    J decode_J(Bit<32> instruction) {
        J decoded;
        decoded.opcode = instruction.range<6, 0>();
        decoded.rd = instruction.range<11, 7>();

        Bit<1> imm_20 = instruction.range<31, 31>();
        Bit<10> imm_10_1 = instruction.range<30, 21>();
        Bit<1> imm_11 = instruction.range<20, 20>();
        Bit<8> imm_19_12 = instruction.range<19, 12>();

        decoded.imm = Bit{imm_20, imm_19_12, imm_11, imm_10_1, Bit<1>{0}}; // Last bit is 0

        return decoded;
    }

    // Decoding for I* Type instruction
    I_star decode_I_star(Bit<32> instruction) {
        I_star decoded;
        decoded.opcode = instruction.range<6, 0>();
        decoded.rd = instruction.range<11, 7>();
        decoded.funct3 = instruction.range<14, 12>();
        decoded.rs1 = instruction.range<19, 15>();
        decoded.imm = instruction.range<24, 20>();
        decoded.funct7 = instruction.range<31, 25>();
        return decoded;
    }

    unsigned int get_opcode(Bit<32> instruction) {
        return static_cast<max_size_t>(instruction.range<6, 0>());
    }
} // namespace instructionsn

int main() {
    auto memory = std::make_unique<Memory>();
    std::ios_base::sync_with_stdio(false);
    memory->load_data(std::cin);

    Interpreter interpreter(memory.get());

    unsigned int result = interpreter.run(1e9);

    std::cout << result << std::endl;
    return 0;
}
