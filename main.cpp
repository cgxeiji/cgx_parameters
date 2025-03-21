#include <iostream>

#include "my_params.hpp"

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

cgx::unique_parameter_list<10> params(custom_printer);

auto& integer = params.add<0>(42);
auto& boolean = params.add<1>(false);
auto& custom = params.add<2>(custom_type{1, 2});
auto& text = params.add<5>(__DATE__ " " __TIME__);
auto& array = params.add<3>(std::array<int, 3>{
    1,
    5,
});
auto& custom_array = params.add<4>([]() {
    std::array<custom_type, 2> arr;
    for (size_t i = 0; i < 2; ++i) {
        arr[i] = custom_type{1, static_cast<int>(i)};
    }
    return arr;
}());

int main() {
    using namespace cgx::parameter;

    integer.on_changed([]() {
        std::cout << "integer changed: " << integer << std::endl;
    });
    boolean.on_changed([]() {
        std::cout << "boolean changed:" << std::endl << "  ";
        boolean.print();
    });
    array.on_changed([]() {
        std::cout << "array changed:" << std::endl;
        for (const auto& value : array) {
            std::cout << "  " << value << std::endl;
        }
    });
    custom_array.on_changed([]() {
        std::cout << "custom_array changed:" << std::endl;
        for (const auto& value : custom_array) {
            std::cout << "  ";
            value.print();
        }
    });
    text.on_changed([]() {
        std::cout << "text changed: " << text << std::endl;
    });

    integer.print();
    boolean.print();
    custom.print();
    array.print();
    custom_array.print();
    std::cout << std::endl;

    std::cout << "change parameters:" << std::endl;
    integer = integer + 420;
    boolean = true;
    custom = custom_type{3, 4};
    custom_array[0] = custom_type{2, 2};
    text = "good bye";

    for (auto& value : array) {
        value = value + 10;
    }

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

