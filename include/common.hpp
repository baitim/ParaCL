#pragma once

#include "ANSI_colors.hpp"
#include <ranges>
#include <sstream>
#include <fstream>

namespace rng  = std::ranges;
namespace view = rng::views;

namespace common {

    template <typename MsgT>
    concept error_str =
    std::is_constructible_v<std::string, MsgT> &&
    requires(std::ostream& os, MsgT msg) {
        { os << msg } -> std::same_as<std::ostream&>;
    };

    class error_t {
        std::string msg_;
    public:
        template <error_str MsgT>
        error_t(MsgT msg) : msg_(msg) {}

        virtual const char* what() const { return msg_.c_str(); }

        virtual ~error_t() = default;
    };

    inline std::string file2str(const std::string& file_name) {
        std::ifstream input_file(file_name);
        if (input_file.is_open()) {
            std::stringstream sstr;
            sstr << input_file.rdbuf();
            input_file.close();
            return sstr.str();
        }
        throw common::error_t{str_red("can't open program file")};
    }
}