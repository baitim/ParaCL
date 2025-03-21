#pragma once

#include "ParaCL/nodes/variable.hpp"

namespace paracl {
    class node_lvalue_t final : public node_expression_t {
        node_variable_t* variable_;
        node_indexes_t*  indexes_;

    public:
        node_lvalue_t(const location_t& loc, node_variable_t* variable, node_indexes_t* indexes)
        : node_expression_t(loc), variable_(variable), indexes_(indexes) {
            assert(indexes_);
        }

        value_t execute(execute_params_t& params) override {
            return variable_->execute(indexes_, params);
        }

        analyze_t analyze(analyze_params_t& params) override {
            if (!variable_)
                throw error_declaration_t{node_loc_t::loc(), params.program_str, "undeclared variable"};
            return variable_->analyze(indexes_, params);
        }

        value_t set_value(value_t new_value, execute_params_t& params) {
            return variable_->set_value(indexes_, new_value, params);
        }

        analyze_t set_value_analyze(analyze_t new_value, analyze_params_t& params,
                                    const location_t& loc_set) {
            return variable_->set_value_analyze(indexes_, new_value, params, loc_set);
        }

        node_expression_t* copy(copy_params_t& params, scope_base_t* parent) const override {
            node_variable_t* var_node = nullptr;
            if (variable_) {
                if (parent)
                    var_node = static_cast<node_variable_t*>(parent->get_node(variable_->get_name()));
                
                if (!var_node)
                    var_node = variable_->copy(params);
                else
                    var_node->set_loc(node_loc_t::loc());

                parent->add_variable(var_node);
            }

            auto& buf = params.buf;
            return buf->add_node<node_lvalue_t>(node_loc_t::loc(), var_node, indexes_->copy(params, parent));
        }

        std::string_view get_name() const { assert(variable_); return variable_->get_name(); }

        void set_predict(bool value) override { if (variable_) variable_->set_predict(value); }
    };
}