#pragma once

#include "ParaCL/nodes/common.hpp"

namespace paracl {
    enum class binary_operators_e {
        EQ,
        NE,
        LE,
        GE,
        LT,
        GT,

        OR,
        AND,

        ADD,
        SUB,
        MUL,
        DIV,
        MOD
    };
    class node_bin_op_t final : public node_expression_t {
        binary_operators_e type_;
        node_expression_t* left_;
        node_expression_t* right_;

    private:
        template <typename ParamsT>
        int evaluate(node_number_t* l_result, node_number_t* r_result, ParamsT& params) {
            assert(l_result);
            assert(r_result);

            int LHS = l_result->get_value();
            int RHS = r_result->get_value();
            switch (type_) {
                case binary_operators_e::EQ: return LHS == RHS;
                case binary_operators_e::NE: return LHS != RHS;
                case binary_operators_e::LE: return LHS <= RHS;
                case binary_operators_e::GE: return LHS >= RHS;
                case binary_operators_e::LT: return LHS <  RHS;
                case binary_operators_e::GT: return LHS >  RHS;

                case binary_operators_e::OR:  return LHS || RHS;
                case binary_operators_e::AND: return LHS && RHS;

                case binary_operators_e::ADD: return LHS + RHS;
                case binary_operators_e::SUB: return LHS - RHS;
                case binary_operators_e::MUL: return LHS * RHS;
                case binary_operators_e::DIV: return LHS / RHS;
                case binary_operators_e::MOD: return LHS % RHS;
                default: throw error_location_t{node_loc_t::loc(), params.program_str,
                                                "attempt to use unknown binary operator"};
            }
        }

    public:
        node_bin_op_t(const location_t& loc, binary_operators_e type,
                    node_expression_t* left, node_expression_t* right)
        : node_expression_t(loc), type_(type), left_(left), right_(right) {
            assert(left);
            assert(right);
        }

        value_t execute(execute_params_t& params) override {
            value_t l_result = left_ ->execute(params);
            value_t r_result = right_->execute(params);

            if (l_result.type == node_type_e::UNDEF ||
                r_result.type == node_type_e::UNDEF)
                return {node_type_e::UNDEF, params.buf()->add_node<node_undef_t>(node_loc_t::loc())};
            
            node_number_t* l_result_number = static_cast<node_number_t*>(l_result.value);
            node_number_t* r_result_number = static_cast<node_number_t*>(r_result.value);
            int result = evaluate(l_result_number, r_result_number, params);
            return {node_type_e::INTEGER, params.buf()->add_node<node_number_t>(node_loc_t::loc(), result)};
        }

        analyze_t analyze(analyze_params_t& params) override {
            analyze_t a_l_result = left_->analyze(params);
            analyze_t a_r_result = right_->analyze(params);

            value_t l_result = a_l_result.result;
            value_t r_result = a_r_result.result;

            if (l_result.type == node_type_e::UNDEF ||
                l_result.type == node_type_e::INPUT)
                return a_l_result;

            if (r_result.type == node_type_e::UNDEF ||
                r_result.type == node_type_e::INPUT)
                return a_r_result;

            expect_types_ne(l_result.type, node_type_e::ARRAY, node_loc_t::loc(), params);
            expect_types_ne(r_result.type, node_type_e::ARRAY, node_loc_t::loc(), params);
        
            node_number_t* l_result_number = static_cast<node_number_t*>(l_result.value);
            node_number_t* r_result_number = static_cast<node_number_t*>(r_result.value);
            int result = evaluate(l_result_number, r_result_number, params);

            analyze_t a_result{node_type_e::INTEGER, params.buf()->add_node<node_number_t>(node_loc_t::loc(), result)};
            a_result.is_constexpr = a_l_result.is_constexpr & a_r_result.is_constexpr;
            return a_result;
        }

        node_expression_t* copy(copy_params_t& params, scope_base_t* parent) const override {
            auto& buf = params.buf;
            return buf->add_node<node_bin_op_t>(node_loc_t::loc(), type_,
                                                left_->copy(params, parent), right_->copy(params, parent));
        }

        void set_predict(bool value) override {
            left_->set_predict(value);
            right_->set_predict(value);
        }
    };
}