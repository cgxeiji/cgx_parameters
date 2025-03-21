#include <iostream>

#include "parameter.hpp"

struct custom_type {
    int a;
    int b;

    int to_char(char* dst, size_t size) const {
        return snprintf(dst, size, "custom_type: a=%d, b=%d", a, b);
    }
};

void custom_printer(const char* str) {
    std::cout << str << std::endl;
}

int main() {
    using namespace cgx::parameter;
    unique_parameter_list params;

    auto& integer = cgx::make_param<0>(params, 42);
    auto& boolean = cgx::make_param<1>(params, false);
    auto& custom = cgx::make_param<2>(params, custom_type{1, 2});
    auto& array = cgx::make_param<3>(params, std::array<int, 3>{1});
    auto& custom_array = cgx::make_param<4>(params, std::array<custom_type, 20>{
        {custom_type{1, 2}},
    });

    char buffer[1024];
    integer.to_char(buffer, sizeof(buffer));
    std::cout << buffer << std::endl;
    boolean.to_char(buffer, sizeof(buffer));
    std::cout << buffer << std::endl;
    custom.to_char(buffer, sizeof(buffer));
    std::cout << buffer << std::endl;
    array.to_char(buffer, sizeof(buffer));
    std::cout << buffer << std::endl;
    custom_array.to_char(buffer, sizeof(buffer));
    std::cout << buffer << std::endl;

    params.print(custom_printer);

    integer = 420;
    boolean = true;
    custom = custom_type{3, 4};

    std::cout << "all parameters:" << std::endl;
    params.print(custom_printer);

    std::cout << "resetting all parameters:" << std::endl;
    params.reset();
    params.print(custom_printer);
    return 0;
}

