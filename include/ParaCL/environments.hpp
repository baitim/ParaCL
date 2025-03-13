#pragma once

#include <iostream>
#include <string>

namespace paracl {
    struct environments_t final {
        std::ostream* os = nullptr;
        std::istream* is = nullptr;
        std::string_view program_str = {};
    };
}