#pragma once

#include <cstdio>
#include <memory>
#include <array>
#include <functional>

#ifndef CGX_PARAMETER_PRINT_BUFFER_SIZE
#define CGX_PARAMETER_PRINT_BUFFER_SIZE 64
#endif

namespace cgx{
namespace parameter {

class parameter_i {
    public:
        virtual ~parameter_i() = default;
        virtual bool set_bytes(const uint8_t* src, size_t size) = 0;
        virtual bool get_bytes(uint8_t* dst, size_t size) const = 0;

        virtual int to_char(char* dst, size_t size) const = 0;
        virtual void print() const = 0;

        virtual void reset() = 0;

        virtual void on_changed(std::function<void()> callback) {
            m_on_changed = callback;
        }

    protected:
        std::function<void()> m_on_changed;
};

template <typename T>
class parameter : virtual public parameter_i {
    public:
        parameter() = default;
        parameter(
                std::function<void(const char*)> print, 
                const T& default_value) 
            : m_default(default_value)
            , m_print(print) {}
        parameter(const parameter&) = default;
        virtual ~parameter() = default;

        bool set_bytes(const uint8_t* src, size_t size) override {
            if (size != sizeof(T)) {
                return false;
            }
            set_value(*reinterpret_cast<const T*>(src));
            return true;
        }

        bool get_bytes(uint8_t* dst, size_t size) const override {
            if (size != sizeof(T)) {
                return false;
            }
            *reinterpret_cast<T*>(dst) = m_value;
            return true;
        }

        int to_char(char* dst, size_t size) const override {
            return m_value.to_char(dst, size);
        }

        void print() const override {
            if (m_print == nullptr) {
                return;
            }
            char buffer[CGX_PARAMETER_PRINT_BUFFER_SIZE];
            to_char(buffer, sizeof(buffer));
            m_print(buffer);

            uint8_t bytes[sizeof(T)];
            if (this->get_bytes(bytes, sizeof(bytes))) {
                constexpr size_t group_size = 8;
                char dst[3*group_size + 6];
                int n = 0;
                for (size_t i = 0; i < sizeof(T); ++i) {
                    if (i % group_size == 0) {
                        n += snprintf(
                                dst + n, 
                                sizeof(dst) - n, 
                                "  |- ");
                        if (n < 0 || n >= sizeof(dst)) {
                            m_print("error");
                            break;
                        }
                    }
                    n += snprintf(dst + n, sizeof(dst) - n, " %02X", bytes[i]);
                    if (n < 0 || n >= sizeof(dst)) {
                        m_print("error");
                        break;
                    }
                    if (i % group_size == group_size - 1) {
                        m_print(dst);
                        n = 0;
                    }
                }
                if (n > 0) {
                    m_print(dst);
                }
            }
        }

        operator T() const {
            return m_value;
        }

        parameter<T>& operator=(const T& value) {
            set_value(value);
            return *this;
        }

        const T& value() const {
            return m_value;
        }

        bool set_value(const T& value) {
            if (m_value == value) {
                return true;
            }
            m_value = value;
            if (m_on_changed) {
                m_on_changed();
            }
            return true;
        }

        void reset() override {
            set_value(m_default);
        }

        void set_print(std::function<void(const char*)> print) {
            m_print = print;
        }

    protected:
        T m_default{};
        T m_value{m_default};
        std::function<void(const char*)> m_print{nullptr};
};

template <>
int parameter<int>::to_char(char* dst, size_t size) const {
    return snprintf(dst, size, "int: %d", m_value);
}

template <>
int parameter<float>::to_char(char* dst, size_t size) const {
    return snprintf(dst, size, "float: %f", m_value);
}

template <>
int parameter<bool>::to_char(char* dst, size_t size) const {
    return snprintf(dst, size, "bool: %s", m_value ? "true" : "false");
}

template <typename T, size_t N>
class parameter<std::array<T, N>> : virtual public parameter_i {
    public:
        parameter() = default;
        parameter(std::function<void(const char*)> print,
                const T& default_value) 
            : m_print(print) {
            std::fill(
                    m_value.begin(), 
                    m_value.end(), 
                    parameter<T>(
                        m_print, 
                        default_value
                    )
                );
        }
        parameter(std::function<void(const char*)> print,
                const std::array<T, N>& default_value) 
            : m_print(print) {
            size_t i = 0;
            for (auto& value : m_value) {
                value = parameter<T>(m_print, default_value[i++]);
            }
        }
        parameter(const parameter&) = default;
        virtual ~parameter() = default;

        bool set_bytes(const uint8_t* src, size_t size) override {
            (void)src;
            (void)size;
            //if (size != sizeof(std::array<T, N>)) {
            //    return false;
            //}
            //set_value(*reinterpret_cast<const std::array<T, N>*>(src));
            return true;
        }

        bool get_bytes(uint8_t* dst, size_t size) const override {
            if (size != sizeof(std::array<T, N>)) {
                return false;
            }
            for (auto& value : m_value) {
                value.get_bytes(dst, sizeof(T));
                dst += sizeof(T);
            }
            return true;
        }

        bool get_bytes(size_t index, uint8_t* dst, size_t size) const {
            if (index >= N) {
                return false;
            }
            return m_value[index].get_bytes(dst, size);
        }

        int to_char(char* dst, size_t size) const override {
            int n = snprintf(dst, size, "array<%zu>", N);
            if (n < 0) {
                return n;
            }
            if (n >= size) {
                return n;
            }
            return n;
        }

        void print() const override {
            if (m_print == nullptr) {
                return;
            }
            {
                char buffer[16 * N];
                int n = this->to_char(buffer, sizeof(buffer));
                if (n < 0) {
                    m_print("error");
                    return;
                }
                if (n >= sizeof(buffer)) {
                    m_print("buffer overflow");
                    return;
                }
                m_print(buffer);
            }

            char buffer[CGX_PARAMETER_PRINT_BUFFER_SIZE];
            for (size_t i = 0; i < N; ++i) {
                int n = snprintf(
                        buffer, 
                        sizeof(buffer), 
                        "  |- [%*zu] ", 
                        N <= 10 ? 1 : 2, 
                        i
                    );
                if (n < 0) {
                    continue;
                }
                if (n >= sizeof(buffer)) {
                    continue;
                }

                n += m_value[i].to_char(buffer + n, sizeof(buffer) - n);
                if (n < 0) {
                    continue;
                }
                if (n >= sizeof(buffer)) {
                    continue;
                }

                m_print(buffer);

                uint8_t bytes[sizeof(T)];
                if (m_value[i].get_bytes(bytes, sizeof(bytes))) {
                    constexpr size_t group_size = 8;
                    char dst[3*group_size + 9];
                    int n = 0;
                    for (size_t i = 0; i < sizeof(T); ++i) {
                        if (i % group_size == 0) {
                            n += snprintf(
                                    dst + n, 
                                    sizeof(dst) - n, 
                                    "  |  |- ");
                            if (n < 0 || n >= sizeof(dst)) {
                                m_print("error");
                                break;
                            }
                        }
                        n += snprintf(dst + n, sizeof(dst) - n, " %02X", bytes[i]);
                        if (n < 0 || n >= sizeof(dst)) {
                            m_print("error");
                            break;
                        }
                        if (i % group_size == group_size - 1) {
                            m_print(dst);
                            n = 0;
                        }
                    }
                    if (n > 0) {
                        m_print(dst);
                    }
                }
            }
        }

        operator std::array<T, N>() const {
            return m_value;
        }

        parameter<std::array<T, N>>& operator=(const std::array<T, N>& value) {
            set_value(value);
            return *this;
        }

        parameter<std::array<T, N>>& operator=(const T& value) {
            set_value(value);
            return *this;
        }

        parameter<T>& operator[](size_t index) {
            return m_value[index];
        }

        const std::array<T, N>& value() const {
            return m_value;
        }

        bool set_value(const std::array<T, N>& value) {
            for (size_t i = 0; i < N; ++i) {
                m_value[i] = value[i];
            }
            return true;
        }

        bool set_value(const T& value) {
            for (size_t i = 0; i < N; ++i) {
                m_value[i] = value;
            }
            return true;
        }

        void reset() override {
            for (auto& value : m_value) {
                value.reset();
            }
        }

        void on_changed(std::function<void()> callback) override {
            for (auto& value : m_value) {
                value.on_changed(callback);
            }
        }

    protected:
        std::function<void(const char*)> m_print{nullptr};
        std::array<parameter<T>, N> m_value;
};


class unique_parameter_i : virtual public parameter_i {
    public:
        virtual ~unique_parameter_i() = default;
        virtual uint32_t uid() const = 0;
};

template <typename T, uint32_t uidT>
class unique_parameter : public unique_parameter_i, public parameter<T> {
    public:
        unique_parameter() = default;
        unique_parameter(std::function<void(const char*)> print,
                const T& value) : parameter<T>(print, value) {}
        unique_parameter(const unique_parameter&) = default;
        virtual ~unique_parameter() = default;

        using parameter<T>::operator T;
        using parameter<T>::operator=;
        using parameter<T>::value;
        using parameter<T>::reset;
        using parameter<T>::set_value;
        using parameter<T>::set_bytes;
        using parameter<T>::get_bytes;
        using parameter<T>::print;

        uint32_t uid() const override {
            return uidT;
        }

        int to_char(char* dst, size_t size) const override {
            int n = snprintf(dst, size, "(p%08X) ", this->uid());
            if (n < 0) {
                return n;
            }
            if (n >= size) {
                return n;
            }

            n += parameter<T>::to_char(dst + n, size - n);
            if (n < 0) {
                return n;
            }
            if (n >= size) {
                return n;
            }

            return n;
        }

};

class unique_parameter_list {
    public:
        unique_parameter_list() = delete;
        unique_parameter_list(std::function<void(const char*)> print) 
            : m_print(print) {}

        void print() const {
            for (const auto& param : m_params) {
                if (param) {
                    param->print();
                }
            }
        }

        template <uint32_t uidT, typename T>
        auto& add(const T& value) {
            m_params[m_size++] = std::make_unique<unique_parameter<T, uidT>>(
                m_print,
                value
            );
            return *reinterpret_cast<unique_parameter<T, uidT>*>(
                m_params[m_size - 1].get()
            );
        }

        void reset() {
            for (auto& param : m_params) {
                if (param) {
                    param->reset();
                }
            }
        }

    private:
        std::array<std::unique_ptr<unique_parameter_i>, 32> m_params;
        size_t m_size = 0;

        std::function<void(const char*)> m_print{nullptr};
};

} // namespace cgx::parameter

template <uint32_t uidT, typename T>
auto make_param(const T& value) {
    return parameter::unique_parameter<T, uidT>(value);
}

template <uint32_t uidT, typename T>
auto& make_param(parameter::unique_parameter_list& list, const T& value) {
    return list.add<uidT>(value);
}

} // namespace cgx
