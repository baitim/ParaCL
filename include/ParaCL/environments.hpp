#pragma once

#include <iostream>
#include <string>

namespace environments {
    struct environments_t final {
        std::ostream* os;
        std::istream* is;
        std::string_view program_str;
    };
}