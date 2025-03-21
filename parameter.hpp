#pragma once

#include <cstdio>
#include <memory>
#include <array>
#include <functional>

namespace cgx{
namespace parameter {

class parameter_i {
    public:
        virtual ~parameter_i() = default;
        virtual bool set_bytes(const uint8_t* src, size_t size) = 0;
        virtual bool get_bytes(uint8_t* dst, size_t size) const = 0;
        virtual int to_char(char* dst, size_t size) const = 0;

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
        parameter(const T& default_value) 
            : m_default(default_value) {}
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
            m_value = value;
            if (m_on_changed) {
                m_on_changed();
            }
            return true;
        }

        void reset() override {
            set_value(m_default);
        }

    private:
        const T m_default{};
        T m_value{m_default};

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
        parameter(const T& default_value) 
            : m_default(default_value) {reset();}
        parameter(const std::array<T, N>& default_value) 
            : m_default(default_value[0]) {reset();}
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
            (void)dst;
            (void)size;
            //if (size != sizeof(std::array<T, N>)) {
            //    return false;
            //}
            //*reinterpret_cast<std::array<T, N>*>(dst) = m_value;
            return true;
        }

        int to_char(char* dst, size_t size) const override {
            int n = snprintf(dst, size, "array:");
            if (n < 0) {
                return n;
            }
            if (n >= size) {
                return n;
            }
            for (size_t i = 0; i < N; ++i) {
                n += snprintf(
                        dst + n,
                        size - n,
                        "\n  |- [%*zu] ",
                        N <= 10 ? 1 : 2,
                        i
                    );
                n += m_value[i].to_char(dst + n, size - n);
                if (n < 0) {
                    return n;
                }
                if (n >= size) {
                    return n;
                }
            }
            return n;
        }

        operator std::array<T, N>() const {
            return m_value;
        }

        parameter<std::array<T, N>>& operator=(const std::array<T, N>& value) {
            set_value(value);
            return *this;
        }

        const std::array<T, N>& value() const {
            return m_value;
        }

        bool set_value(const std::array<T, N>& value) {
            for (size_t i = 0; i < N; ++i) {
                m_value[i] = value[i];
            }
            if (m_on_changed) {
                m_on_changed();
            }
            return true;
        }

        bool set_value(const T& value) {
            for (size_t i = 0; i < N; ++i) {
                m_value[i] = value;
            }
            if (m_on_changed) {
                m_on_changed();
            }
            return true;
        }

        void reset() override {
            set_value(m_default);
        }

    private:
        const T m_default{};
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
        unique_parameter() = delete;
        unique_parameter(const T& value) : parameter<T>(value) {}
        unique_parameter(const unique_parameter&) = delete;
        virtual ~unique_parameter() = default;

        using parameter<T>::operator T;
        using parameter<T>::operator=;
        using parameter<T>::value;
        using parameter<T>::reset;

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

            // print raw bytes
            uint8_t buffer[sizeof(T)];
            if (parameter<T>::get_bytes(buffer, sizeof(buffer))) {
                n += snprintf(dst + n, size - n, "\n  *bytes:");
                for (size_t i = 0; i < sizeof(T); ++i) {
                    n += snprintf(dst + n, size - n, " %02X", buffer[i]);
                }
            }
            return n;
        }
};

class unique_parameter_list {
    public:
        template <size_t bufSizeT = 1024, typename TPrint>
        void print(TPrint&& print) const {
            for (const auto& param : m_params) {
                if (param) {
                    char buffer[bufSizeT];
                    param->to_char(buffer, sizeof(buffer));
                    print(buffer);
                }
            }
        }

        template <uint32_t uidT, typename T>
        auto& add(const T& value) {
            m_params[m_size++] = std::make_unique<unique_parameter<T, uidT>>(
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
