//
// Created by zj on 7/29/2024.
//

#include"tools.h"

int main() {
    Bit<12> a = 0b1;
    if (a >> 5 == 0x000000) {
        std::cout << "OK" << std::endl;
    } else {
        std::cout << "a >> 5 is not equal to 0x0, it's " << to_unsigned(a >> 5) << std::endl;
    }
    if (a << 5 == (1 << 5)) {
        std::cout << "OK" << std::endl;
    } else {
        std::cout << "a << 5 is not equal to 0x10, it's " << to_unsigned(a << 5) << std::endl;
    }
    return 0;
}