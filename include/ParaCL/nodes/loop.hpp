#pragma once

#include "ParaCL/nodes/common.hpp"

namespace paracl {
    class node_loop_t final : public node_statement_t {
        node_expression_t* condition_;
        node_scope_t* body_;

    private:
        void check_step_type(node_type_e type, execute_params_t& params) const {
            if (type == node_type_e::UNDEF)
                throw error_execute_t{node_loc_t::loc(), params.program_str,
                                      "wrong type: undef, excpected int"};
        }

        int step(execute_params_t& params) {
            execute_t result = condition_->execute(params);

            check_step_type(result.type, params);

            return static_cast<node_number_t*>(result.value)->get_value();
        }

        void check_condition(analyze_params_t& params) {
            analyze_t result = condition_->analyze(params);
            expect_types_ne(result.type, node_type_e::ARRAY, node_loc_t::loc(), params);
            expect_types_ne(result.type, node_type_e::UNDEF, node_loc_t::loc(), params);
        }

    public:
        node_loop_t(const location_t& loc, node_expression_t* condition, node_scope_t* body)
        : node_statement_t(loc), condition_(condition), body_(body) {
            assert(condition_);
            assert(body_);
        }

        void execute(execute_params_t& params) override {
            while (step(params))
                body_->execute(params);
        }

        void analyze(analyze_params_t& params) override {
            check_condition(params);
            body_->set_predict(false);
            body_->analyze(params);
        }

        node_statement_t* copy(copy_params_t& params, scope_base_t* parent) const override {
            auto& buf = params.buf;
            return buf->add_node<node_loop_t>(node_loc_t::loc(), condition_->copy(params, parent),
                                              static_cast<node_scope_t*>(body_->copy(params, parent)));
        }

        void set_predict(bool value) override {
            body_->set_predict(value);
        };
    };
}