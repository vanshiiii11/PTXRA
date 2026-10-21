#pragma once
#include <string>
#include <string_view>
#include <vector>
#include <sstream>
#include <algorithm>
#include <cctype>

namespace ptxra {

/// Trim leading whitespace
inline std::string_view ltrim(std::string_view s) {
    auto it = std::find_if(s.begin(), s.end(), [](unsigned char c){ return !std::isspace(c); });
    return s.substr(static_cast<std::size_t>(it - s.begin()));
}

/// Trim trailing whitespace
inline std::string_view rtrim(std::string_view s) {
    auto it = std::find_if(s.rbegin(), s.rend(), [](unsigned char c){ return !std::isspace(c); });
    return s.substr(0, s.size() - static_cast<std::size_t>(it - s.rbegin()));
}

/// Trim both ends
inline std::string_view trim(std::string_view s) { return rtrim(ltrim(s)); }

/// Convert to owned string from string_view
inline std::string toString(std::string_view sv) { return std::string(sv); }

/// Split string by delimiter
inline std::vector<std::string> split(std::string_view s, char delim) {
    std::vector<std::string> result;
    std::size_t start = 0;
    for (std::size_t i = 0; i <= s.size(); ++i) {
        if (i == s.size() || s[i] == delim) {
            result.emplace_back(s.substr(start, i - start));
            start = i + 1;
        }
    }
    return result;
}

/// Join vector of strings with separator
inline std::string join(const std::vector<std::string>& parts, std::string_view sep) {
    std::string result;
    for (std::size_t i = 0; i < parts.size(); ++i) {
        if (i > 0) result += sep;
        result += parts[i];
    }
    return result;
}

/// Simple format-to-string helpers
template<typename T>
std::string toStr(T val) {
    std::ostringstream os;
    os << val;
    return os.str();
}

/// Case-insensitive string compare
inline bool iequals(std::string_view a, std::string_view b) {
    if (a.size() != b.size()) return false;
    return std::equal(a.begin(), a.end(), b.begin(),
        [](unsigned char c1, unsigned char c2){ return std::tolower(c1) == std::tolower(c2); });
}

/// Check if string starts with prefix
inline bool startsWith(std::string_view s, std::string_view prefix) {
    return s.size() >= prefix.size() && s.substr(0, prefix.size()) == prefix;
}

/// Check if string ends with suffix
inline bool endsWith(std::string_view s, std::string_view suffix) {
    return s.size() >= suffix.size() && s.substr(s.size() - suffix.size()) == suffix;
}

} // namespace ptxra
