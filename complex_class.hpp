#pragma once

#include <array>
#include <functional>
#include <iostream>

#include "type_name.hpp"

class complex_class {
    public:
        complex_class() = default;
        complex_class(const complex_class&) = default;
        ~complex_class() = default;

        void set_value(float value) {
            for (size_t i = 0; i < m_array.size(); ++i) {
                m_array[i] = value;
            }
            if (m_fn) {
                m_fn(value);
            }
        }

        void set_fn(std::function<void(float)> fn) {
            m_fn = fn;
        }

        
        int to_char(char* dst, size_t size) const {
            int n = 0;
            for (size_t i = 0; i < m_array.size(); ++i) {
                n += snprintf(dst + n, size - n, "%.1f ", m_array[i]);
                if (n < 0 || static_cast<size_t>(n) >= size) {
                    return n;
                }
            }
            n += snprintf(dst + n, size - n, "fn: %s", 
                    demangle(m_fn.target_type().name()).c_str()
                );
            if (n < 0 || static_cast<size_t>(n) >= size) {
                return n;
            }
            return n;
        }

        bool operator==(const complex_class& other) const {
            for (size_t i = 0; i < m_array.size(); ++i) {
                if (m_array[i] != other.m_array[i]) {
                    return false;
                }
            }
            if (m_fn.target_type() != other.m_fn.target_type()) {
                return false;
            }
            return true;
        }


    private:
        std::array<float, 2> m_array{0};
        std::function<void(float)> m_fn{[](float) {
            std::cout << "default" << std::endl;
        }};

};

