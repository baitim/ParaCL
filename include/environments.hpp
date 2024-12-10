#pragma once

#include <iostream>

namespace environments {
    struct environments_t {
        std::ostream& os;
        std::istream& is;
        bool is_analize;
    };
}