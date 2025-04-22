#pragma once

#include "ParaCL/nodes/common.hpp"

namespace paracl {
    class node_print_t final : public node_expression_t {
        node_expression_t* argument_;

    public:
        node_print_t(const location_t& loc, node_expression_t* argument)
        : node_expression_t(loc), argument_(argument) { assert(argument_); }

        execute_t execute(execute_params_t& params) override {
            execute_t result = argument_->execute(params);
            result.value->print(params);
            return result;
        }

        analyze_t analyze(analyze_params_t& params) override {
            return argument_->analyze(params);
        }

        node_expression_t* copy(copy_params_t& params, scope_base_t* parent) const override {
            return params.buf->add_node<node_print_t>(node_loc_t::loc(), argument_->copy(params, parent));
        }

        void set_predict(bool value) override { argument_->set_predict(value); }
    };
}