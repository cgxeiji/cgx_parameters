#include <iostream>

#include "parameter.hpp"

struct custom_type {
    int a;
    int b;

    int to_char(char* dst, size_t size) const {
        return snprintf(dst, size, "custom_type: a=%d, b=%d", a, b);
    }

    bool operator==(const custom_type& other) const {
        return a == other.a && b == other.b;
    }
};

void custom_printer(const char* str) {
    std::cout << str << std::endl;
}

int main() {
    using namespace cgx::parameter;
    unique_parameter_list params(custom_printer);

    auto& integer = cgx::make_param<0>(params, 42);
    auto& boolean = cgx::make_param<1>(params, false);
    auto& custom = cgx::make_param<2>(params, custom_type{1, 2});
    auto& array = cgx::make_param<3>(params, std::array<int, 3>{
        1,
        5,
    });
    auto& custom_array = cgx::make_param<4>(params, []() {
        std::array<custom_type, 20> arr;
        for (size_t i = 0; i < 20; ++i) {
            arr[i] = custom_type{1, static_cast<int>(i)};
        }
        return arr;
    }());

    integer.on_changed([]() {
        std::cout << "integer changed" << std::endl;
    });
    boolean.on_changed([]() {
        std::cout << "boolean changed" << std::endl;
    });
    custom_array.on_changed([]() {
        std::cout << "custom_array changed" << std::endl;
    });

    integer.print();
    boolean.print();
    custom.print();
    array.print();
    custom_array.print();
    std::cout << std::endl;

    std::cout << "change parameters:" << std::endl;
    integer = 420;
    boolean = true;
    custom = custom_type{3, 4};
    custom_array[2] = custom_type{2, 2};
    params.print();
    std::cout << std::endl;

    std::cout << "resetting all parameters:" << std::endl;
    params.reset();
    params.print();
    std::cout << std::endl;

    std::cout << "resetting again:" << std::endl;
    params.reset();
    return 0;
}

