#include <iostream>
#include <string>
#include "CoreEngine.hpp" // or your header

extern "C" {
    const char* evaluate_expression(const char* input) {
        static std::string result;
        try {
            // Call C++ CoreEngine directly
            result = CoreEngine::compute(std::string(input));
        } catch (const std::exception& e) {
            result = std::string("Error: ") + e.what();
        }
        return result.c_str();
    }
}
