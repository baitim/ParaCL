#pragma once

#include <string>

namespace paracl {
    struct error_t final {
        std::string msg_;
        error_t(const char*        msg) : msg_(msg) {}
        error_t(const std::string& msg) : msg_(msg) {}
    };
}