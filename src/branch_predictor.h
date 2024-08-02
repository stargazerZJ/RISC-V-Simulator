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

} // namespace branch_prediction