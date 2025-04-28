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
        std::optional<int> evaluate_by_left(node_number_t* result, ParamsT& params) {
            assert(result);

            int value = result->get_value();
            switch (type_) {
                case binary_operators_e::OR:  
                    return  value ? std::optional<int>{value} : std::nullopt;

                case binary_operators_e::AND:
                    return !value ? std::optional<int>{value} : std::nullopt;

                case binary_operators_e::EQ:
                case binary_operators_e::NE:
                case binary_operators_e::LE:
                case binary_operators_e::GE:
                case binary_operators_e::LT:
                case binary_operators_e::GT:

                case binary_operators_e::ADD:
                case binary_operators_e::SUB:
                case binary_operators_e::MUL:
                case binary_operators_e::DIV:
                case binary_operators_e::MOD: return std::nullopt;

                default: throw error_location_t{node_loc_t::loc(), params.program_str,
                                                "attempt to use unknown binary operator"};
            }
        }

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

        node_number_t* execute_node(node_expression_t* node, execute_params_t& params) {
            execute_t result = node->execute(params);
            if (result.type == node_type_e::UNDEF || !params.is_executed())
                return nullptr;
            return static_cast<node_number_t*>(result.value);
        }

        std::pair<analyze_t, node_number_t*> analyze_node(node_expression_t* node, analyze_params_t& params) {
            analyze_t result = node->analyze(params);

            if (result.type == node_type_e::UNDEF || result.type == node_type_e::INPUT)
                return {result, nullptr};

            expect_types_ne(result.type, node_type_e::ARRAY, node_loc_t::loc(), params);

            return {result, static_cast<node_number_t*>(result.value)};
        }

    public:
        node_bin_op_t(const location_t& loc, binary_operators_e type,
                      node_expression_t* left, node_expression_t* right)
        : node_expression_t(loc), type_(type), left_(left), right_(right) {
            assert(left);
            assert(right);
        }

        execute_t execute(execute_params_t& params) override {
            node_number_t* l_value = execute_node(left_, params);
            if (!l_value) return make_undef(params, node_loc_t::loc());

            if (auto value_by_left = evaluate_by_left(l_value, params))
                return make_number(*value_by_left, params, node_loc_t::loc());

            node_number_t* r_value = execute_node(right_, params);
            if (!r_value) return make_undef(params, node_loc_t::loc());

            return make_number(evaluate(l_value, r_value, params), params, node_loc_t::loc());
        }

        analyze_t analyze(analyze_params_t& params) override {
            auto [a_l_result, l_value] = analyze_node(left_, params);
            if (!l_value) return a_l_result;

            if (auto value_by_left = evaluate_by_left(l_value, params))
                return analyze_t{make_number(*value_by_left, params, node_loc_t::loc()), a_l_result.is_constexpr};

            auto [a_r_result, r_value] = analyze_node(right_, params);
            if (!r_value) return a_r_result;

            return analyze_t{
                make_number(evaluate(l_value, r_value, params), params, node_loc_t::loc()),
                a_l_result.is_constexpr & a_r_result.is_constexpr
            };
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