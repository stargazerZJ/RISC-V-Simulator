//
// Created by zj on 7/31/2024.
//

#pragma once

#include "tools.h"
#include "constants.h"

namespace regfile {
struct ROB_WB_Input {
    Wire<1>            enabled;
    Wire<5>            reg_id;
    Wire<32>           data;
    Wire<ROB_SIZE_LOG> rob_id;
};

struct Decoder_WB_Input {
    Wire<1>            enabled;
    Wire<5>            reg_id;
    Wire<ROB_SIZE_LOG> rob_id;
};

struct RegFile_Input {
    ROB_WB_Input     rob_wb_input;
    Decoder_WB_Input decoder_wb_input;
    Wire<1> flush_input;
};

/**
 * The output could have contained the information of rs1 and rs2 only, but that requires combinational logic.
 */
struct RegFile_Output {
    std::array<Register<ROB_SIZE_LOG>, 32> rob_id;
    std::array<Register<32>, 32>           data;
};

struct RegFile final : dark::Module<RegFile_Input, RegFile_Output> {
    void work() {
        if (flush_input) {
            return flush();
        }
        if (rob_wb_input.enabled) {
            unsigned reg_id = to_unsigned(rob_wb_input.reg_id);
            data_[reg_id]   = rob_wb_input.data;
            if (rob_wb_input.rob_id == rob_id_[reg_id]) {
                rob_id_[reg_id] = 0;
            }
        }
        if (decoder_wb_input.enabled) {
            unsigned reg_id = to_unsigned(decoder_wb_input.reg_id);
            rob_id_[reg_id] = decoder_wb_input.rob_id;
        }
        rob_id_[0] = 0; // x0 is always 0.
        data_[0]   = 0;
        for (int i = 0; i < 32; ++i) {
            rob_id[i] <= rob_id_[i];
            data[i] <= data[i];
        }
    }

    void flush() {
        for (int i = 0; i < 32; ++i) {
            rob_id_[i] = 0;
        }
        for (int i = 0; i < 32; ++i) {
            rob_id[i] <= rob_id_[i];
            data[i] <= data[i];
        }
    }

private:
    std::array<Bit<ROB_SIZE_LOG>, 32> rob_id_ = {}; // Bit is used to enable combinational logic.
    std::array<Bit<32>, 32>           data_   = {};
};
} // namespace regfile
