#pragma once

#include "ANSI_colors.hpp"
#include <ranges>
#include <sstream>

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
    protected:
        std::string msg_;

    private:
        template <error_str MsgT>
        std::string convert2colored(MsgT msg, bool is_colored) const {
            std::stringstream error;
            if (is_colored) error << print_red(msg);
            else            error << msg;
            return error.str();
        }

    public:
        error_t() {}
        template <error_str MsgT>
        error_t(MsgT msg, bool is_colored = false) : msg_(convert2colored(msg, is_colored)) {}

        virtual const char* what() const { return msg_.c_str(); }

        virtual ~error_t() {};
    };

}