#pragma once

#include <iostream>

namespace environments {
    struct environments_t {
        std::ostream& os;
        std::istream& is;
        bool is_analize;
    };

    class null_buffer_t : public std::streambuf {
    public:
        int overflow(int c) { return c; }
    };
}