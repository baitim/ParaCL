#pragma once

#include <iostream>
#include <string>

namespace environments {
    struct environments_t {
        std::ostream& os;
        std::istream& is;
        bool is_analyzing;
        std::string_view program_str;
    };
}