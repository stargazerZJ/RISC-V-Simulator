//
// Created by zj on 8/2/2024.
//

#pragma once

class Stats {
public:
    void record_branch_prediction_result(bool prediction, bool actual) {
        branch_count += 1;
        if (prediction == actual) {
            correct_count += 1;
        }
    }

    void report(unsigned long long cpu_cycle_count) {
        fprintf(stderr, "CPU simulator halted successfully.\n");
        fprintf(stderr, "branch count: %llu\n", branch_count);
        fprintf(stderr, "correct branch predictions: %llu\n", correct_count);
        fprintf(stderr, "branch prediction accuracy: %Lf\n", static_cast<long double>(correct_count) / branch_count);
        fprintf(stderr, "cpu cycle count: %llu\n", cpu_cycle_count);
        fprintf(stderr, "cpu cycle per branch: %Lf\n", static_cast<long double>(cpu_cycle_count) / branch_count);
    }

private:
    unsigned long long correct_count = 0;
    unsigned long long branch_count  = 0;
};
