#pragma once

#include "algorithm"
#include "string"

inline std::string to_lower(const std::string &str) {
    std::string tmp = str;
    std::transform(tmp.begin(), tmp.end(), tmp.begin(), ::tolower);
    return tmp;
}
