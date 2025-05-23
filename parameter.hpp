#pragma once

#include <array>
#include <cassert>
#include <cstdio>
#include <cstring>
#include <functional>
#include <memory>

#include "type_name.hpp"
#include "uid.hpp"

#ifndef CGX_PARAMETER_PRINT_BUFFER_SIZE
#define CGX_PARAMETER_PRINT_BUFFER_SIZE 128
#endif

namespace cgx {

constexpr uint32_t _crc32_single_byte(uint32_t byte) {
    uint32_t crc = byte;
    for (int i = 0; i < 8; ++i) {
        crc = (crc >> 1) ^ (0xEDB88320 & -(crc & 1));
    }
    return crc;
}

constexpr std::array<uint32_t, 256> _generate_crc32_table() {
    std::array<uint32_t, 256> table{};
    for (uint32_t i = 0; i < 256; ++i) {
        table[i] = _crc32_single_byte(i);
    }
    return table;
}

constexpr uint32_t _init_crc32() {
    return 0xFFFFFFFF;
}

constexpr std::array<uint32_t, 256> _crc32_table = _generate_crc32_table();

constexpr uint32_t _calc_crc(const uint8_t* data, size_t size) {
    uint32_t crc = _init_crc32();
    for (size_t i = 0; i < size; ++i) {
        uint8_t byte = data[i];
        crc          = (crc >> 8) ^ _crc32_table[(crc ^ byte) & 0xFF];
    }
    return crc ^ 0xFFFFFFFF;
}

constexpr uint32_t _calc_crc(uint32_t crc, const uint8_t* data, size_t size) {
    for (size_t i = 0; i < size; ++i) {
        uint8_t byte = data[i];
        crc          = (crc >> 8) ^ _crc32_table[(crc ^ byte) & 0xFF];
    }
    return crc ^ 0xFFFFFFFF;
}

namespace parameter {

class parameter_i {
   public:
    virtual ~parameter_i() = default;

    virtual bool set_bytes(const uint8_t* src, size_t size) = 0;
    virtual bool get_bytes(uint8_t* dst, size_t size) const = 0;

    virtual int  to_char(char* dst, size_t size) const = 0;
    virtual void print() const                         = 0;

    virtual void reset() = 0;

    virtual uint32_t get_crc() const = 0;

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
    parameter(std::function<void(const char*)> print, const T& default_value)
        : m_default(default_value), m_print(print) {
    }
    parameter(const parameter&) = default;
    virtual ~parameter()        = default;

    bool set_bytes(const uint8_t* src, size_t size) override {
        if (size != sizeof(T)) {
            return false;
        }
        std::memcpy(&m_value, src, sizeof(T));
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

    uint32_t get_crc() const override {
        uint32_t crc = _init_crc32();

        crc = _calc_crc(
            crc, reinterpret_cast<const uint8_t*>(&m_value), sizeof(T)
        );

        return crc;
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
            char             dst[3 * group_size + 6];
            int              n = 0;
            for (size_t i = 0; i < sizeof(T); ++i) {
                if (i % group_size == 0) {
                    n += snprintf(dst + n, sizeof(dst) - n, "   + ");
                    if (n < 0 || static_cast<size_t>(n) >= sizeof(dst)) {
                        m_print("error");
                        break;
                    }
                }
                n += snprintf(dst + n, sizeof(dst) - n, " %02X", bytes[i]);
                if (n < 0 || static_cast<size_t>(n) >= sizeof(dst)) {
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
    T                                m_default{};
    T                                m_value{m_default};
    std::function<void(const char*)> m_print{nullptr};
};

template <>
inline int parameter<int>::to_char(char* dst, size_t size) const {
    return snprintf(dst, size, "%d (%d)", m_value, m_default);
}

template <>
inline int parameter<float>::to_char(char* dst, size_t size) const {
    return snprintf(dst, size, "%f (%f)", m_value, m_default);
}

template <>
inline int parameter<bool>::to_char(char* dst, size_t size) const {
    return snprintf(
        dst,
        size,
        "%s (%s)",
        m_value ? "true" : "false",
        m_default ? "true" : "false"
    );
}

template <typename T, size_t N>
class parameter<std::array<T, N>> : virtual public parameter_i {
   public:
    parameter() = default;
    parameter(std::function<void(const char*)> print, const T& default_value)
        : m_print(print) {
        std::fill(
            m_value.begin(), m_value.end(), parameter<T>(m_print, default_value)
        );
    }
    parameter(
        std::function<void(const char*)> print,
        const std::array<T, N>&          default_value
    )
        : m_print(print) {
        size_t i = 0;
        for (auto& value : m_value) {
            value = parameter<T>(m_print, default_value[i++]);
        }
    }
    parameter(const parameter&) = default;
    virtual ~parameter()        = default;

    bool set_bytes(const uint8_t* src, size_t size) override {
        (void)src;
        (void)size;
        // if (size != sizeof(std::array<T, N>)) {
        //     return false;
        // }
        // set_value(*reinterpret_cast<const std::array<T, N>*>(src));
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

    uint32_t get_crc() const override {
        uint32_t crc = _init_crc32();

        for (const auto& value : m_value) {
            crc = _calc_crc(
                crc, reinterpret_cast<const uint8_t*>(&value), sizeof(T)
            );
        }

        return crc;
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
        [&]() {
            char buffer[CGX_PARAMETER_PRINT_BUFFER_SIZE];
            int  n = this->to_char(buffer, sizeof(buffer));
            if (n < 0) {
                m_print("error");
                return;
            }
            if (n >= sizeof(buffer)) {
                m_print("buffer overflow");
                return;
            }
            m_print(buffer);
        }();

        char buffer[CGX_PARAMETER_PRINT_BUFFER_SIZE];
        for (size_t i = 0; i < N; ++i) {
            int n = snprintf(
                buffer, sizeof(buffer), "  |- [%*zu] ", N <= 10 ? 1 : 2, i
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
                char             dst[3 * group_size + 9];
                int              n = 0;
                for (size_t i = 0; i < sizeof(T); ++i) {
                    if (i % group_size == 0) {
                        n += snprintf(dst + n, sizeof(dst) - n, "  |   + ");
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
    std::array<parameter<T>, N>      m_value;
};

template <size_t N>
class parameter<char[N]> : virtual public parameter_i {
   public:
    parameter() = default;
    parameter(std::function<void(const char*)> print, const char* default_value)
        : m_print(print) {
        std::copy(default_value, default_value + N, m_default);
        set_value(default_value);
    }
    parameter(const parameter&) = default;
    virtual ~parameter()        = default;

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
        return snprintf(dst, size, "%s (%s)", m_value, m_default);
    }

    uint32_t get_crc() const override {
        uint32_t crc = _init_crc32();

        crc = _calc_crc(crc, reinterpret_cast<const uint8_t*>(m_value), N);

        return crc;
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
            char             dst[3 * group_size + 6];
            int              n = 0;
            for (size_t i = 0; i < N; ++i) {
                if (i % group_size == 0) {
                    n += snprintf(dst + n, sizeof(dst) - n, "   + ");
                    if (n < 0 || static_cast<size_t>(n) >= sizeof(dst)) {
                        m_print("error");
                        break;
                    }
                }
                n += snprintf(dst + n, sizeof(dst) - n, " %02X", bytes[i]);
                if (n < 0 || static_cast<size_t>(n) >= sizeof(dst)) {
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
    char                             m_default[N]{};
    char                             m_value[N]{};
    std::function<void(const char*)> m_print{nullptr};
};

extern bool set_bytes(size_t lun, uint32_t uid, const uint8_t* src, size_t len);
extern bool get_bytes(size_t lun, uint32_t uid, uint8_t* dst, size_t len);

}  // namespace parameter

class storable_parameter_i {
   public:
    storable_parameter_i() = default;
    storable_parameter_i(size_t lun) : m_lun(lun) {
    }
    virtual ~storable_parameter_i() = default;

    virtual bool store()    = 0;
    virtual bool retrieve() = 0;
    virtual bool validate() = 0;

    void set_lun(size_t lun) {
        m_lun = lun;
    }
    size_t get_lun() const {
        return m_lun;
    }

    bool is_valid() const {
        return m_valid;
    }

    void set_valid(bool valid) {
        m_valid = valid;
    }

   protected:
    size_t m_lun{0};
    bool   m_valid{false};
};

class unique_parameter_i
    : virtual public parameter::parameter_i
    , public storable_parameter_i {
   public:
    unique_parameter_i(size_t lun) : storable_parameter_i(lun) {
    }
    virtual ~unique_parameter_i() = default;
    virtual uint32_t uid() const  = 0;
};

template <typename T>
class unique_parameter
    : public unique_parameter_i
    , public parameter::parameter<T> {
   public:
    unique_parameter() = default;
    unique_parameter(
        std::function<void(const char*)> print,
        const parameter::uid_t&          uid,
        const T&                         value
    )
        : parameter::parameter<T>(print, value), m_uid(uid) {
    }
    unique_parameter(
        std::function<void(const char*)> print,
        size_t                           lun,
        const parameter::uid_t&          uid,
        const T&                         value
    )
        : unique_parameter_i(lun)
        , parameter::parameter<T>(print, value)
        , m_uid(uid) {
    }
    unique_parameter(const unique_parameter&) = default;
    virtual ~unique_parameter()               = default;

    using parameter::parameter<T>::operator=;
    using parameter::parameter<T>::value;
    using parameter::parameter<T>::reset;
    using parameter::parameter<T>::print;
    using parameter::parameter<T>::set_bytes;
    using parameter::parameter<T>::get_bytes;

    bool validate() override {
        if (this->is_valid()) {
            return true;
        }

        if (this->retrieve()) {
            this->set_valid(true);
            return true;
        }

        this->reset();
        return this->store();
    }

    bool retrieve() override {
        uint8_t buffer[sizeof(T)];
        if (!cgx::parameter::get_bytes(
                this->get_lun(), this->uid(), buffer, sizeof(T)
            )) {
            return false;
        }
        return this->set_bytes(buffer, sizeof(T));
    }

    bool store() override {
        uint8_t on_store[sizeof(T)];
        bool    ok = cgx::parameter::get_bytes(
            this->get_lun(), this->uid(), on_store, sizeof(T)
        );

        uint8_t buffer[sizeof(T)];
        if (!this->get_bytes(buffer, sizeof(T))) {
            return false;
        }

        if (ok) {
            for (size_t i = 0; i < sizeof(T); i++) {
                if (buffer[i] != on_store[i]) {
                    ok = false;
                    break;
                }
            }
            if (ok) {
                return true;
            }
        }

        if (!cgx::parameter::set_bytes(
                this->get_lun(), this->uid(), buffer, sizeof(T)
            )) {
            return false;
        }
        this->set_valid(true);
        return true;
    }

    uint32_t uid() const override {
        return m_uid;
    }

    int to_char(char* dst, size_t size) const override {
        int n = snprintf(
            dst, size, "%s %s = ", type_name<T>().data(), this->m_uid.data()
        );
        if (n < 0) {
            return n;
        }
        if (n >= static_cast<int>(size)) {
            return n;
        }

        n += parameter::parameter<T>::to_char(dst + n, size - n);
        if (n < 0) {
            return n;
        }
        if (n >= static_cast<int>(size)) {
            return n;
        }

        return n;
    }

   private:
    const parameter::uid_t m_uid{"unnamed"};
};

template <size_t LUN, size_t N>
class unique_parameter_list {
   public:
    unique_parameter_list() = delete;
    unique_parameter_list(std::function<void(const char*)> print)
        : m_print(print) {
    }

    bool init() {
        for (const auto& param : m_params) {
            if (!param) {
                continue;
            }
            if (!param->validate()) {
                return false;
            }
        }

        return true;
    }

    void print() const {
        for (const auto& param : m_params) {
            if (param) {
                param->print();
            }
        }
    }

    uint32_t get_crc() const {
        uint32_t crc = _init_crc32();

        for (const auto& param : m_params) {
            if (param) {
                crc = _calc_crc(
                    crc,
                    reinterpret_cast<const uint8_t*>(param.get()),
                    sizeof(*param)
                );
            }
        }

        return crc;
    }

    template <typename T>
    auto& add(const std::string_view& name, const T& value) {
        parameter::uid_t uid(name);
        if (m_size >= m_params.size()) {
            m_size -= 1;
            m_params[m_size++] =
                std::make_unique<unique_parameter<T>>(m_print, LUN, uid, value);
            if (m_print) {
                m_print("parameter list full when adding:");
                m_params[m_size - 1]->print();
            }
            assert("parameter list full" && m_size < m_params.size());
            return *reinterpret_cast<unique_parameter<T>*>(
                m_params[m_size - 1].get()
            );
        }

        auto p = this->find(uid);
        m_params[m_size++] =
            std::make_unique<unique_parameter<T>>(m_print, LUN, uid, value);

        if (p != nullptr) {
            if (m_print) {
                m_print("conflicting parameters:");
                p->print();
                m_params[m_size - 1]->print();
            }
            assert("UID already exists" && p == nullptr);
            return *reinterpret_cast<unique_parameter<T>*>(
                m_params[m_size - 1].get()
            );
        }

        return *reinterpret_cast<unique_parameter<T>*>(m_params[m_size - 1].get(
        ));
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

}  // namespace cgx
