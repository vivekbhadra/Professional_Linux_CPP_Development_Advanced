#include <iostream>
int add(int x, int y)
{
    return x + y;
}

int subtract(int x, int y)
{
    return x - y;
}

int divide(int x, int y)
{
    if (y == 0)
    {
        std::cerr << "Error: Division by zero!" << "\n";
        return 0; // or throw an exception
    }
    return x / y;
}

int multiply(int x, int y)
{
    return x * y;
}