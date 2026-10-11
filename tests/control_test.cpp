#include "dmotor/control.hpp"
#include <cstdlib>
#include <iostream>
int main() {
    int32_t duty, integral;
    bool fault;
    dmotor_pi(65536, 65536, 65536, 65536, 1000, 0, 0, 1000, duty, integral, fault);
    if (fault || duty != 65536 || integral != 0) {
        return 1; // anti-windup at saturation
    }
    dmotor_pi(655, 65536, 65536, 65536, 1000, 0, 0, 1000, duty, integral, fault);
    if (fault || duty != 721 || integral != 66) {
        return 2;
    }
    dmotor_pi(655, 65536, 65536, 65536, -1000, 0, 0, 1000, duty, integral, fault);
    if (fault || duty != -721 || integral != -66) {
        return 3;
    }
    dmotor_pi(655, 65536, 65536, 65536, INT32_MIN, 0, 0, 1000, duty, integral, fault);
    if (!fault || duty != 0) {
        return 4;
    }
    dmotor_pi(1048576, 1048576, 65536, 65536, 1000000, -1000000, 0, 100000, duty, integral, fault);
    if (fault || duty != 65536 || integral != 0) {
        return 5;
    }
    std::cout << "PI: signed rounding, limits, anti-windup, range checks passed\n";
}
