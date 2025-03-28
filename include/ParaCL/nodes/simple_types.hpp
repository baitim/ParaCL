#pragma once

#include "ParaCL/nodes/common.hpp"

namespace paracl {
    class node_number_t final : public node_simple_type_t {
        int number_;

    public:
        node_number_t(const location_t& loc, int number) : node_simple_type_t(loc), number_(number) {}

        value_t execute(execute_params_t& params) override {
            return {node_type_e::INTEGER, this};
        }

        analyze_t analyze(analyze_params_t& params) override {
            return {node_type_e::INTEGER, this};
        }

        void print(execute_params_t& params) override { *(params.os) << number_ << '\n'; }

        int get_value() const noexcept { return number_; }

        node_expression_t* copy(copy_params_t& params, scope_base_t* parent) const override {
            return params.buf->add_node<node_number_t>(node_loc_t::loc(), number_);
        }
    };

    /* ----------------------------------------------------- */

    class node_undef_t final : public node_simple_type_t {
    public:
        node_undef_t(const location_t& loc) : node_simple_type_t(loc) {}

        value_t execute(execute_params_t& params) override {
            return {node_type_e::UNDEF, this};
        }

        void print(execute_params_t& params) override { *(params.os) << "undef\n"; }

        analyze_t analyze(analyze_params_t& params) override {
            return {node_type_e::UNDEF, this};
        }

        node_expression_t* copy(copy_params_t& params, scope_base_t* parent) const override {
            return params.buf->add_node<node_undef_t>(node_loc_t::loc());
        }
    };

    /* ----------------------------------------------------- */

    class node_input_t final : public node_simple_type_t {
    public:
        node_input_t(const location_t& loc) : node_simple_type_t(loc) {}

        value_t execute(execute_params_t& params) override {
            int value;
            *(params.is) >> value;
            if (!params.is->good())
                throw error_execute_t{node_loc_t::loc(), params.program_str, "invalid input: need integer"};
            
            return {node_type_e::INTEGER, params.buf()->add_node<node_number_t>(node_loc_t::loc(), value)};
        }

        analyze_t analyze(analyze_params_t& params) override {
            return {node_type_e::INPUT, params.buf()->add_node<node_input_t>(node_loc_t::loc())};
        }

        void print(execute_params_t& params) override { *(params.os) << "?\n"; }

        node_expression_t* copy(copy_params_t& params, scope_base_t* parent) const override {
            return params.buf->add_node<node_input_t>(node_loc_t::loc());
        }
    };

    /* ----------------------------------------------------- */

    template <typename T>
    concept existed_params = std::same_as<T, execute_params_t> || std::same_as<T, analyze_params_t>;

    template <existed_params ParamsT>
    value_t make_undef(ParamsT& params, const location_t& loc) {
        return {node_type_e::UNDEF, params.buf()->template add_node<node_undef_t>(loc)};
    }

    template <existed_params ParamsT>
    value_t make_number(int value, ParamsT& params, const location_t& loc) {
        return {node_type_e::INTEGER, params.buf()->template add_node<node_number_t>(loc, value)};
    }
}