#pragma once

#include <cstdio>
#include <memory>
#include <array>
#include <functional>
#include <cassert>

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
            std::memcpy(dst, &m_value, sizeof(T));
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
                                "   + ");
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
        T& value() {
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
    return snprintf(dst, size, "%d", m_value);
}

template <>
int parameter<float>::to_char(char* dst, size_t size) const {
    return snprintf(dst, size, "%f", m_value);
}

template <>
int parameter<bool>::to_char(char* dst, size_t size) const {
    return snprintf(dst, size, "%s", m_value ? "true" : "false");
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
                if (n < 0 || n >= sizeof(buffer)) {
                    continue;
                }

                n += m_value[i].to_char(buffer + n, sizeof(buffer) - n);
                if (n < 0 || n >= sizeof(buffer)) {
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
                                    "  |   + ");
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
            assert(index < N);
            return m_value[index];
        }

        auto begin() {
            return m_value.begin();
        }
        auto begin() const {
            return m_value.begin();
        }

        auto end() {
            return m_value.end();
        }
        auto end() const {
            return m_value.end();
        }

        const std::array<T, N>& value() const {
            return m_value;
        }
        std::array<T, N>& value() {
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

template <size_t N>
class parameter<char[N]> : virtual public parameter_i {
    public:
        parameter() = default;
        parameter(std::function<void(const char*)> print,
                const char* default_value) 
            : m_print(print) {
            std::copy(default_value, default_value + N, m_default);
            set_value(default_value);
        }
        parameter(const parameter&) = default;
        virtual ~parameter() = default;

        bool set_bytes(const uint8_t* src, size_t size) override {
            if (size != N) {
                return false;
            }
            set_value(reinterpret_cast<const char*>(src));
            return true;
        }

        bool get_bytes(uint8_t* dst, size_t size) const override {
            if (size != N) {
                return false;
            }
            std::copy(m_value, m_value + N, dst);
            return true;
        }

        int to_char(char* dst, size_t size) const override {
            return snprintf(dst, size, "char[%zu]: %s", N, m_value);
        }

        void print() const override {
            if (m_print == nullptr) {
                return;
            }
            char buffer[CGX_PARAMETER_PRINT_BUFFER_SIZE];
            to_char(buffer, sizeof(buffer));
            m_print(buffer);

            uint8_t bytes[N];
            if (this->get_bytes(bytes, sizeof(bytes))) {
                constexpr size_t group_size = 8;
                char dst[3*group_size + 6];
                int n = 0;
                for (size_t i = 0; i < N; ++i) {
                    if (i % group_size == 0) {
                        n += snprintf(
                                dst + n, 
                                sizeof(dst) - n, 
                                "   + ");
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

        operator const char*() const {
            return m_value;
        }

        parameter<char[N]>& operator=(const char* value) {
            set_value(value);
            return *this;
        }

        const char* value() const {
            return m_value;
        }

        bool set_value(const char* value) {
            if (strcmp(m_value, value) == 0) {
                return true;
            }
            size_t len = strlen(value);
            if (len >= N) {
                len = N - 1;
            }
            std::copy(value, value + len + 1, m_value);
            m_value[N - 1] = '\0';
            if (m_on_changed) {
                m_on_changed();
            }
            return true;
        }

        void reset() override {
            set_value(m_default);
        }

    protected:
        char m_default[N]{};
        char m_value[N]{};
        std::function<void(const char*)> m_print{nullptr};
};

} // namespace cgx::parameter

class unique_parameter_i : virtual public parameter::parameter_i {
    public:
        virtual ~unique_parameter_i() = default;
        virtual uint32_t uid() const = 0;
};

template <typename T, uint32_t uidT>
class unique_parameter 
        : public unique_parameter_i
        , public parameter::parameter<T> {
    public:
        unique_parameter() = default;
        unique_parameter(std::function<void(const char*)> print,
                const T& value) : parameter::parameter<T>(print, value) {}
        unique_parameter(const unique_parameter&) = default;
        virtual ~unique_parameter() = default;

        using parameter::parameter<T>::operator=;
        using parameter::parameter<T>::value;
        using parameter::parameter<T>::reset;
        using parameter::parameter<T>::set_value;
        using parameter::parameter<T>::set_bytes;
        using parameter::parameter<T>::get_bytes;
        using parameter::parameter<T>::print;

        uint32_t uid() const override {
            return uidT;
        }

        int to_char(char* dst, size_t size) const override {
            int n = snprintf(dst, size, "(p%08X) %s: ", 
                    this->uid(),
                    typeid(T).name()
                );
            if (n < 0) {
                return n;
            }
            if (n >= size) {
                return n;
            }

            n += parameter::parameter<T>::to_char(dst + n, size - n);
            if (n < 0) {
                return n;
            }
            if (n >= size) {
                return n;
            }

            return n;
        }

};

template <size_t N>
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
            if (m_size >= m_params.size()) {
                m_size -= 1;
                m_params[m_size++] = std::make_unique<unique_parameter<T, uidT>>(
                    m_print,
                    value
                );
                if (m_print) {
                    m_print("parameter list full when adding:");
                    m_params[m_size - 1]->print();
                }
                assert("parameter list full" && m_size < m_params.size());
                return *reinterpret_cast<unique_parameter<T, uidT>*>(
                    m_params[m_size - 1].get()
                );
            }

            auto p = this->find(uidT);
            m_params[m_size++] = std::make_unique<unique_parameter<T, uidT>>(
                m_print,
                value
            );

            if (p != nullptr) {
                if (m_print) {
                    m_print("conflicting parameters:");
                    p->print();
                    m_params[m_size - 1]->print();
                }
                assert("UID already exists" && p == nullptr);
                return *reinterpret_cast<unique_parameter<T, uidT>*>(
                    m_params[m_size - 1].get()
                );
            }

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

        bool uid_exists(uint32_t uid) const {
            for (const auto& param : m_params) {
                if (param && param->uid() == uid) {
                    return true;
                }
            }
            return false;
        }

    private:
        std::array<std::unique_ptr<unique_parameter_i>, N> m_params;
        size_t m_size = 0;

        std::function<void(const char*)> m_print{nullptr};

        unique_parameter_i* find(uint32_t uid) {
            for (auto& param : m_params) {
                if (param && param->uid() == uid) {
                    return param.get();
                }
            }
            return nullptr;
        }
};

} // namespace cgx
