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
    unsigned             prediction_table[PREDICTOR_SIZE]{};

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
    unsigned             prediction_table[PREDICTOR_SIZE]{};
    unsigned             global_history      = 0;
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
        unsigned local_history_index  = get_local_history_index(pc);
        unsigned local_pattern_index  = local_history_table[local_history_index];
        unsigned global_pattern_index = get_global_pattern_index(pc);

        unsigned local_prediction  = local_pattern_table[local_pattern_index];
        unsigned global_prediction = global_pattern_table[global_pattern_index];

        // Combine local and global predictions
        unsigned combined_prediction = (local_prediction + global_prediction) / 2;
        return combined_prediction >= 2; // 2, 3 represent Taken
    }

    void update(unsigned pc, bool taken) {
        unsigned local_history_index  = get_local_history_index(pc);
        unsigned local_pattern_index  = local_history_table[local_history_index];
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
        std::fill(local_pattern_table.begin(), local_pattern_table.end(), 1);   // Start with weakly not taken
        std::fill(global_pattern_table.begin(), global_pattern_table.end(), 1); // Start with weakly not taken
        global_history = 0;
    }

private:
    static constexpr int      LOCAL_HISTORY_TABLE_SIZE = 1024; // Example size, should be a power of 2
    static constexpr int      PATTERN_TABLE_SIZE       = 1024; // Example size, should be a power of 2
    static constexpr int      GLOBAL_HISTORY_BITS      = 10;
    static constexpr unsigned LOCAL_HISTORY_MASK       = (1 << GLOBAL_HISTORY_BITS) - 1;
    static constexpr unsigned GLOBAL_HISTORY_MASK      = (1 << GLOBAL_HISTORY_BITS) - 1;

    std::array<unsigned, LOCAL_HISTORY_TABLE_SIZE> local_history_table{};
    std::array<unsigned, PATTERN_TABLE_SIZE>       local_pattern_table{};
    std::array<unsigned, PATTERN_TABLE_SIZE>       global_pattern_table{};

    unsigned global_history = 0;

    unsigned get_local_history_index(unsigned pc) const {
        return (pc >> 2) % LOCAL_HISTORY_TABLE_SIZE;
    }

    unsigned get_global_pattern_index(unsigned pc) const {
        return (global_history ^ (pc >> 2)) % PATTERN_TABLE_SIZE;
    }
};

class TAGEPredictionEntry {
public:
    int8_t   counter; // 2-bit saturating counter
    uint32_t tag;
    TAGEPredictionEntry() : counter(0), tag(0) {} // Initialize as weakly not taken
};

class TAGEPredictionTable {
public:
    TAGEPredictionTable(int size, int historyLength) : size(size), historyLength(historyLength) {
        table.resize(size);
    }

    TAGEPredictionEntry& getEntry(uint32_t index) {
        return table[index % size];
    }

    uint32_t getHistoryLength() const {
        return historyLength;
    }

private:
    int                              size;
    int                              historyLength;
    std::vector<TAGEPredictionEntry> table;
};

class TAGEPredictor {
public:
    TAGEPredictor() {
        reset();
    }

    void addTable(int size, int historyLength) {
        tables.emplace_back(size, historyLength);
    }

    bool predict(uint32_t pc) {
        int providerTableIdx = -1;
        for (size_t i = 0; i < tables.size(); ++i) {
            uint32_t    index = get_index(pc, i);
            const auto& entry = tables[i].getEntry(index);
            if (entry.tag == get_tag(pc, i)) {
                providerTableIdx = i;
            }
        }

        if (providerTableIdx >= 0) {
            const auto& entry = tables[providerTableIdx].getEntry(get_index(pc, providerTableIdx));
            return entry.counter >= 0;
        }

        return base_predictor[pc % PREDICTOR_SIZE] >= 0; // Base predictor
    }

    void update(uint32_t pc, bool taken) {
        bool pred             = predict(pc);
        int  providerTableIdx = -1;

        for (size_t i = 0; i < tables.size(); ++i) {
            uint32_t index = get_index(pc, i);
            auto&    entry = tables[i].getEntry(index);
            if (entry.tag == get_tag(pc, i)) {
                providerTableIdx = i;
                break;
            }
        }

        // Update the provider
        if (providerTableIdx >= 0) {
            auto& entry = tables[providerTableIdx].getEntry(get_index(pc, providerTableIdx));
            if (taken) {
                if (entry.counter < 1) entry.counter++;
            } else {
                if (entry.counter > -2) entry.counter--;
            }
        } else {
            // Update the base predictor
            uint32_t index = pc % PREDICTOR_SIZE;
            if (taken) {
                if (base_predictor[index] < 1) base_predictor[index]++;
            } else {
                if (base_predictor[index] > -2) base_predictor[index]--;
            }
        }

        // Allocate new entry if misprediction
        if (pred != taken && providerTableIdx < static_cast<int>(tables.size()) - 1) {
            for (size_t i = providerTableIdx + 1; i < tables.size(); ++i) {
                uint32_t index = get_index(pc, i);
                auto&    entry = tables[i].getEntry(index);
                if (entry.counter == -1 || entry.counter == 0) {
                    entry.counter = taken ? 0 : -1;
                    entry.tag     = get_tag(pc, i);
                    break;
                }
            }
        }

        update_global_history(taken);
    }

    void reset() {
        std::fill(std::begin(base_predictor), std::end(base_predictor), -1);
        // Start with weakly not taken in base predictor
        global_history = 0;
        addTable(1024, 4);  // Example table with size 1024 and history length 4
        addTable(1024, 8);  // Example table with size 1024 and history length 8
        addTable(1024, 12); // Example table with size 1024 and history length 12
    }

private:
    static constexpr int             PREDICTOR_SIZE   = 1024; // Example size, could be any power of 2
    static constexpr int             MAX_HISTORY_BITS = 64;
    std::vector<TAGEPredictionTable> tables;
    int8_t                           base_predictor[PREDICTOR_SIZE]{};
    uint32_t                         global_history = 0;

    uint32_t get_index(uint32_t pc, size_t tableIndex) const {
        return (pc ^ (global_history & ((1 << tables[tableIndex].getHistoryLength()) - 1))) % tables[tableIndex].
            getHistoryLength();
    }

    uint32_t get_tag(uint32_t pc, size_t tableIndex) const {
        return (pc >> 2) ^ (global_history & ((1 << tables[tableIndex].getHistoryLength()) - 1));
    }

    void update_global_history(bool taken) {
        global_history = ((global_history << 1) | taken) & ((1ULL << MAX_HISTORY_BITS) - 1);
    }
};
} // namespace branch_prediction
