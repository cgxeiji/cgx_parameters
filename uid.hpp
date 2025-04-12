#pragma once

#include <cstdint>
#include <cstring>
#include <string_view>

namespace cgx::parameter {

class uid_t {
   public:
    constexpr uid_t(uint32_t uid = 0) : m_name(""), m_uid(uid) {
    }
    constexpr uid_t(std::string_view name) : m_name(name), m_uid(hash(name)) {
    }

    static constexpr uint32_t hash(std::string_view name) {
        uint32_t hash = 0;
        for (size_t i = 0; i < name.size(); ++i) {
            hash = (hash * 31) + static_cast<uint32_t>(name[i]);
        }
        return hash;
    }

    constexpr uint32_t get_uid() const {
        return m_uid;
    }

    constexpr std::string_view get_name() const {
        return m_name;
    }

    constexpr bool operator==(const uid_t& other) const {
        return m_uid == other.m_uid;
    }

    constexpr bool operator!=(const uid_t& other) const {
        return m_uid != other.m_uid;
    }

    constexpr int to_char(char* dst, size_t size) const {
        return snprintf(dst, size, "%s", m_name.data());
    }

    constexpr const char* data() const {
        return const_cast<char*>(m_name.data());
    }

    constexpr operator uint32_t() const {
        return m_uid;
    }

   private:
    const std::string_view m_name;
    const uint32_t         m_uid{0};
};

}  // namespace cgx::parameter
