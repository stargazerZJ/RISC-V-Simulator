//
// Created by zj on 7/31/2024.
//

#pragma once

#include "memory.h"
#include "tools.h"

namespace fetcher {

class BranchPredictor {
public:
    BranchPredictor() {
        reset();
    }

    static unsigned get_index(unsigned pc) { return (pc >> 2) % PREDICTOR_SIZE; }

    bool predict(unsigned pc) {
        unsigned index = get_index(pc);
        return prediction_table[index] >= 2; // 2, 3 represent Taken
    }

    void update(unsigned pc, bool taken) {
        unsigned index = get_index(pc);
        if (taken) {
            if (prediction_table[index] < 3) prediction_table[index]++;
        } else {
            if (prediction_table[index] > 0) prediction_table[index]--;
        }
    }

    void reset() {
        std::fill_n(prediction_table, PREDICTOR_SIZE, 1); // Start with weakly not taken
    }

private:
    static constexpr int PREDICTOR_SIZE = 1024; // Example size
    unsigned prediction_table[PREDICTOR_SIZE]{};
};

struct Fetcher_Input {
    Wire<32> last_PC_plus_4;

    Wire<32> pc_from_decoder;
    Wire<1> pc_from_decoder_enabled;

    Wire<32> pc_from_ROB;   // the last bit should be 0
    Wire<1> pc_from_ROB_enabled;

    Wire<32> pc_of_branch;  // from ROB, used for updating the branch predictor
    Wire<1> branch_taken;
    Wire<1> branch_record_enabled;
};

struct Fetcher_Output {
    Register<32> instruction;
    Register<32> program_counter;
    Register<1> predicted_branch_taken;
};

/**
 * Fetcher is responsible for fetching the instruction from memory,
 * and predicting whether the branch will be taken (assuming every instruction is a branch).
 *
 * The brahch predictor is not implemented yet.
 */
struct Fetcher final : dark::Module<Fetcher_Input, Fetcher_Output> {
    explicit Fetcher(Memory *memory) : memory(memory) {}
    void work() {
        static bool is_first_run = true;
        if (is_first_run) {
            first_run();
            is_first_run = false;
            return ;
        }
        unsigned pc;
        if (pc_from_ROB_enabled) {
            pc = to_unsigned(pc_from_ROB);
        } else if (pc_from_decoder_enabled) {
            pc = to_unsigned(pc_from_decoder);
        } else {
            pc = to_unsigned(last_PC_plus_4);
        }

        if (branch_record_enabled) {
            branch_predictor.update(to_unsigned(pc_of_branch), to_unsigned(branch_taken));
        }

        instruction <= memory->get_word(pc);    // fetching the instruction takes only 1 cycle
        program_counter <= pc;
        predicted_branch_taken <= branch_predictor.predict(pc);    // TODO: implement branch predictor;
    }
    void first_run() {
        unsigned pc = 0;
        instruction <= memory->get_word(pc);
        program_counter <= pc;
        predicted_branch_taken <= false;
        branch_predictor.reset();
    }
private:
    Memory *memory;
    BranchPredictor branch_predictor;
};
}