//
// Created by zj on 7/29/2024.
//

#pragma once
#include <array>
#include <cstdint>
#include <debug.h>
#include <istream>
#include <sstream>

constexpr int memory_size = 1048576;
/**
 * Physical memory simulator of RISC-V.
 */
class Memory {
public:
    uint8_t& get_byte(uint32_t addr);
    uint16_t& get_half(uint32_t addr);
    uint32_t& get_word(uint32_t addr);

    [[nodiscard]] uint8_t get_byte(uint32_t addr) const;
    [[nodiscard]] uint16_t get_half(uint32_t addr) const;
    [[nodiscard]] uint32_t get_word(uint32_t addr) const;

    /**
     * Load data from a memory dump file.
     * @param is input stream of the file.
     */
    void load_data(std::istream& is);

private:
    std::array<uint8_t, memory_size> memory;
};

inline uint8_t& Memory::get_byte(uint32_t addr) {
    dark::debug::assert(addr < memory_size, "Memory::get_byte: address out of range");
    return memory[addr];
}

inline uint16_t& Memory::get_half(uint32_t addr) {
    dark::debug::assert(addr < memory_size - 1, "Memory::get_half: address out of range");
    return *reinterpret_cast<uint16_t*>(&memory[addr]);
}

inline uint32_t& Memory::get_word(uint32_t addr) {
    dark::debug::assert(addr < memory_size - 3, "Memory::get_word: address out of range");
    return *reinterpret_cast<uint32_t*>(&memory[addr]);
}

inline uint8_t Memory::get_byte(uint32_t addr) const {
    dark::debug::assert(addr < memory_size, "Memory::get_byte: address out of range");
    return memory[addr];
}

inline uint16_t Memory::get_half(uint32_t addr) const {
    dark::debug::assert(addr < memory_size - 1, "Memory::get_half: address out of range");
    return *reinterpret_cast<const uint16_t*>(&memory[addr]);
}

inline uint32_t Memory::get_word(uint32_t addr) const {
    dark::debug::assert(addr < memory_size - 3, "Memory::get_word: address out of range");
    return *reinterpret_cast<const uint32_t*>(&memory[addr]);
}

inline void Memory::load_data(std::istream& is) {
    std::string line;
    uint32_t address = 0;

    while (std::getline(is, line)) {
        if (line.empty() || line[0] == '@') {
            if (line[0] == '@') {
                address = std::stoul(line.substr(1), nullptr, 16);
            }
            continue;
        }

        std::istringstream linestream(line);
        uint32_t byte;
        while (linestream >> std::hex >> byte) {
            dark::debug::assert(address < memory_size, "Memory::load_data: address out of range");
            memory[address++] = static_cast<uint8_t>(byte);
        }
    }
}
