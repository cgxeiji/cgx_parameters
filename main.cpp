#include <iostream>

#include "my_params.hpp"

struct custom_type {
    int a;
    int b;

    int to_char(char* dst, size_t size) const {
        return snprintf(dst, size, "a=%d, b=%d", a, b);
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

auto& complex = params.add<100>(complex_class{});

int main() {
    using namespace cgx::parameter;

    complex.on_changed([]() {
        std::cout << "complex changed:" << std::endl << "  ";
        complex.print();
    });

    integer.print();
    params.print();
    std::cout << std::endl;

    std::cout << "change parameters:" << std::endl;
    integer = integer + 420;
    boolean = true;
    custom = custom_type{3, 4};
    custom_array[0] = custom_type{2, 2};
    text = "good bye";

    complex.value().set_fn([](float value) {
        std::cout << "custom fn: " << value << std::endl;
    });
    complex.value().set_value(3.14f);

    for (auto& value : array) {
        value = value + 10;
    }

    complex.print();
    //params.print();
    std::cout << std::endl;

    std::cout << "resetting all parameters:" << std::endl;
    params.reset();
    complex.print();
    //params.print();
    std::cout << std::endl;

    std::cout << "resetting again:" << std::endl;
    params.reset();
    complex.value().set_value(2.71f);
    complex.print();
    complex.value().set_fn(nullptr);
    complex.print();
    return 0;
}

