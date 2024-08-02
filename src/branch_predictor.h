//
// Created by zj on 8/2/2024.
//

#pragma once

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

} // namespace branch_prediction