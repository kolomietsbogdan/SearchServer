#include "read_input_functions.h"

#include <iostream>


int ReadLineWithNumber() {
    int result;
    std::cin >> result;
    ReadLine();
    return result;
}

std::string ReadLine() {
    std::string str;
    getline(std::cin, str);
    return str;
}
