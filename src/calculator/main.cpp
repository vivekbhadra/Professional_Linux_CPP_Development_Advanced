#include "calculator.h"

#include <iostream>

int main()
{
    int a = 10;
    int b = 5;

    std::cout << "Addition: " << add(a, b) << "\n";
    std::cout << "Subtraction: " << subtract(a, b) << "\n";
    std::cout << "Multiplication: " << multiply(a, b) << "\n";
    std::cout << "Division: " << divide(a, b) << "\n";

    return 0;
}