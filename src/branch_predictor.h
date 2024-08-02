//
// Created by zj on 8/2/2024.
//

#pragma once

#include <algorithm>
#include <array>

namespace branch_prediction {

class BimodalPredictor {
public:
    BimodalPredictor() {
        reset();
    }

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

    static unsigned get_index(unsigned pc) { return (pc >> 2) % PREDICTOR_SIZE; }
};

class GSharePredictor {
public:
    GSharePredictor() {
        reset();
    }

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
        update_global_history(taken);
    }

    void reset() {
        std::fill_n(prediction_table, PREDICTOR_SIZE, 1); // Start with weakly not taken
        global_history = 0;
    }

private:
    static constexpr int PREDICTOR_SIZE = 1024; // Example size, should be a power of 2
    unsigned prediction_table[PREDICTOR_SIZE]{};
    unsigned global_history = 0;
    static constexpr int GLOBAL_HISTORY_BITS = 14;

    unsigned get_index(unsigned pc) {
        return ((pc >> 2) ^ global_history) % PREDICTOR_SIZE;
    }

    void update_global_history(bool taken) {
        global_history = ((global_history << 1) | taken) & ((1 << GLOBAL_HISTORY_BITS) - 1);
    }
};

class TwoLevelAdaptivePredictor {
public:
    TwoLevelAdaptivePredictor() {
        reset();
    }

    bool predict(unsigned pc) {
        unsigned local_history_index = get_local_history_index(pc);
        unsigned local_pattern_index = local_history_table[local_history_index];
        unsigned global_pattern_index = get_global_pattern_index(pc);

        unsigned local_prediction = local_pattern_table[local_pattern_index];
        unsigned global_prediction = global_pattern_table[global_pattern_index];

        // Combine local and global predictions
        unsigned combined_prediction = (local_prediction + global_prediction) / 2;
        return combined_prediction >= 2; // 2, 3 represent Taken
    }

    void update(unsigned pc, bool taken) {
        unsigned local_history_index = get_local_history_index(pc);
        unsigned local_pattern_index = local_history_table[local_history_index];
        unsigned global_pattern_index = get_global_pattern_index(pc);

        // Update the local pattern table
        if (taken) {
            if (local_pattern_table[local_pattern_index] < 3) local_pattern_table[local_pattern_index]++;
        } else {
            if (local_pattern_table[local_pattern_index] > 0) local_pattern_table[local_pattern_index]--;
        }

        // Update the global pattern table
        if (taken) {
            if (global_pattern_table[global_pattern_index] < 3) global_pattern_table[global_pattern_index]++;
        } else {
            if (global_pattern_table[global_pattern_index] > 0) global_pattern_table[global_pattern_index]--;
        }

        // Update the local history table
        local_history_table[local_history_index] = ((local_pattern_index << 1) | taken) & LOCAL_HISTORY_MASK;

        // Update the global history register
        global_history = ((global_history << 1) | taken) & GLOBAL_HISTORY_MASK;
    }

    void reset() {
        std::fill(local_history_table.begin(), local_history_table.end(), 0);
        std::fill(local_pattern_table.begin(), local_pattern_table.end(), 1); // Start with weakly not taken
        std::fill(global_pattern_table.begin(), global_pattern_table.end(), 1); // Start with weakly not taken
        global_history = 0;
    }

private:
    static constexpr int LOCAL_HISTORY_TABLE_SIZE = 1024; // Example size, should be a power of 2
    static constexpr int PATTERN_TABLE_SIZE = 1024; // Example size, should be a power of 2
    static constexpr int GLOBAL_HISTORY_BITS = 10;
    static constexpr unsigned LOCAL_HISTORY_MASK = (1 << GLOBAL_HISTORY_BITS) - 1;
    static constexpr unsigned GLOBAL_HISTORY_MASK = (1 << GLOBAL_HISTORY_BITS) - 1;

    std::array<unsigned, LOCAL_HISTORY_TABLE_SIZE> local_history_table{};
    std::array<unsigned, PATTERN_TABLE_SIZE> local_pattern_table{};
    std::array<unsigned, PATTERN_TABLE_SIZE> global_pattern_table{};

    unsigned global_history = 0;

    unsigned get_local_history_index(unsigned pc) const {
        return (pc >> 2) % LOCAL_HISTORY_TABLE_SIZE;
    }

    unsigned get_global_pattern_index(unsigned pc) const {
        return (global_history ^ (pc >> 2)) % PATTERN_TABLE_SIZE;
    }
};

} // namespace branch_prediction