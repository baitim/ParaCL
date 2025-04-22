#pragma once

#include "ParaCL/nodes/common.hpp"

namespace paracl {
    enum class unary_operators_e {
        ADD,
        SUB,
        NOT
    };
    class node_un_op_t final : public node_expression_t {
        unary_operators_e  type_;
        node_expression_t* node_;

    private:
        template <typename ParamsT>
        int evaluate(node_number_t* result, ParamsT& params) const {
            assert(result);
            int value = result->get_value();
            switch (type_) {
                case unary_operators_e::ADD: return  value;
                case unary_operators_e::SUB: return -value;
                case unary_operators_e::NOT: return !value;
                default: throw error_location_t{node_loc_t::loc(), params.program_str,
                                                "attempt to use unknown unary operator"};
            }
        }

    public:
        node_un_op_t(const location_t& loc, unary_operators_e type, node_expression_t* node)
        : node_expression_t(loc), type_(type), node_(node) { assert(node_); }

        execute_t execute(execute_params_t& params) override {
            execute_t res_exec = node_->execute(params);

            if (res_exec.type == node_type_e::UNDEF)
                return {node_type_e::UNDEF, params.buf()->add_node<node_undef_t>(node_loc_t::loc())};

            int result = evaluate(static_cast<node_number_t*>(res_exec.value), params);
            return {node_type_e::INTEGER, params.buf()->add_node<node_number_t>(node_loc_t::loc(), result)};
        }

        analyze_t analyze(analyze_params_t& params) override {
            analyze_t res_exec = node_->analyze(params);

            if (res_exec.type == node_type_e::UNDEF ||
                res_exec.type == node_type_e::INPUT)
                return res_exec;

            expect_types_ne(res_exec.type, node_type_e::ARRAY, node_loc_t::loc(), params);

            int result = evaluate(static_cast<node_number_t*>(res_exec.value), params);
            analyze_t a_result{node_type_e::INTEGER, params.buf()->add_node<node_number_t>(node_loc_t::loc(), result)};
            a_result.is_constexpr = res_exec.is_constexpr;
            return a_result;
        }

        node_expression_t* copy(copy_params_t& params, scope_base_t* parent) const override {
            return params.buf->add_node<node_un_op_t>(node_loc_t::loc(), type_, node_->copy(params, parent));
        }

        void set_predict(bool value) override { node_->set_predict(value); }
    };
}