#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

#include "my_params.hpp"

bool cgx::parameter::set_bytes(
    size_t         lun,
    uint32_t       uid,
    const uint8_t* src,
    size_t         len
) {
    std::cout << "[s] " << "lun: " << lun << " uid: " << std::hex << uid
              << " src: [";

    for (size_t i = 0; i < len; i++) {
        std::cout << " " << std::hex << static_cast<uint32_t>(src[i]);
    }

    std::cout << " ]" << std::endl;

    std::stringstream ss;
    ss << "./stored/p" << std::hex << uid << ".bin";
    std::fstream s{ss.str(), s.binary | s.trunc | s.in | s.out};
    if (!s.is_open()) {
        std::cout << "failed to open " << ss.str() << std::endl;
        return true;
    }

    s.seekp(0);
    s.write(reinterpret_cast<const char*>(src), len);

    return true;
};

bool cgx::parameter::get_bytes(
    size_t   lun,
    uint32_t uid,
    uint8_t* dst,
    size_t   len
) {
    std::stringstream ss;
    ss << "./stored/p" << std::hex << uid << ".bin";

    std::cout << "reading: " << ss.str() << std::endl;

    std::FILE* f = std::fopen(ss.str().c_str(), "r");
    if (!f) {
        std::cout << "failed to open " << ss.str() << std::endl;
        return false;
    }

    // s.read(reinterpret_cast<char*>(dst), len);
    std::fread(dst, sizeof(dst[0]), len, f);
    std::fclose(f);

    std::cout << "[g] " << "lun: " << lun << " uid: " << std::hex << uid
              << " dst: [";

    for (size_t i = 0; i < len; i++) {
        std::cout << " " << std::hex << static_cast<uint32_t>(dst[i]);
    }

    std::cout << " ]" << std::endl;

    return true;
};

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

cgx::unique_parameter_list<0, 10> params(custom_printer);

auto& integer      = params.add<0>(42);
auto& boolean      = params.add<1>(false);
auto& custom       = params.add<2>(custom_type{1, 2});
auto& text         = params.add<5>(__DATE__ " " __TIME__);
auto& array        = params.add<3>(std::array<int, 3>{
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

    params.init();

    complex.on_changed([]() {
        std::cout << "complex changed:" << std::endl << "  ";
        complex.print();
    });

    integer.print();
    params.print();
    std::cout << std::endl;

    std::cout << "change parameters:" << std::endl;
    integer         = integer + 420;
    boolean         = true;
    custom          = custom_type{3, 4};
    custom_array[0] = custom_type{2, 2};
    text            = "good bye";

    complex.value().set_fn([](float value) {
        std::cout << "custom fn: " << value << std::endl;
    });
    complex.value().set_value(3.14f);

    for (auto& value : array) {
        value = value + 10;
    }

    complex.print();
    // params.print();
    std::cout << std::endl;

    std::cout << "resetting all parameters:" << std::endl;
    params.reset();
    complex.print();
    // params.print();
    std::cout << std::endl;

    integer = 123;
    integer.store();

    std::cout << "resetting again:" << std::endl;
    params.reset();
    complex.value().set_value(2.71f);
    complex.print();
    complex.value().set_fn(nullptr);
    complex.print();
    return 0;
}

