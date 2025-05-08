#pragma once

#include "ParaCL/nodes/common.hpp"

namespace paracl {
    class node_fork_t final : public node_statement_t {
        node_expression_t* condition_;
        node_scope_t* body1_;
        node_scope_t* body2_;

    public:
        node_fork_t(const location_t& loc, node_expression_t* condition,
                    node_scope_t* body1, node_scope_t* body2)
        : node_statement_t(loc), condition_(condition), body1_(body1), body2_(body2) {
            assert(condition_);
            assert(body1_);
            assert(body2_);
        }

        void execute(execute_params_t& params) override {
            if (params.get_evaluated(this))
                return;

            execute_t result = condition_->execute(params);
            if (!params.is_executed()) return;
            params.add_value(this, result);

            int value = static_cast<node_number_t*>(result.value)->get_value();
            if (value)
                params.insert_statement(body1_);
            else
                params.insert_statement(body2_);
        }

        void analyze(analyze_params_t& params) override {
            analyze_t result = condition_->analyze(params);

            expect_types_ne(result.type, node_type_e::ARRAY, node_loc_t::loc(), params);
            expect_types_ne(result.type, node_type_e::UNDEF, node_loc_t::loc(), params);

            body1_->set_predict(false);
            body2_->set_predict(false);

            body1_->analyze(params);
            body2_->analyze(params);
        }

        node_statement_t* copy(copy_params_t& params, scope_base_t* parent) const override {
            auto& buf = params.buf;
            return buf->add_node<node_fork_t>(node_loc_t::loc(), condition_->copy(params, parent),
                                              static_cast<node_scope_t*>(body1_->copy(params, parent)),
                                              static_cast<node_scope_t*>(body2_->copy(params, parent)));
        }

        void set_predict(bool value) override {
            body1_->set_predict(value);
            body2_->set_predict(value);
        };
    };
}