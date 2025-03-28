#pragma once

#include "ParaCL/nodes/array.hpp"

namespace paracl {
    class settable_value_t : public node_t,
                             public node_loc_t {
        bool is_setted = false;
        value_t   e_value_;
        analyze_t a_value_;

    private:
        static void check_types_in_assign(general_type_e l_type, general_type_e r_type,
                                          const location_t& loc_set, analyze_params_t& params) {
            if (l_type == r_type)
                return;

            std::string error_msg =   "wrong types in assign: " + type2str(r_type)
                                    + " cannot be assigned to " + type2str(l_type);
            throw error_analyze_t{loc_set, params.program_str, error_msg};
        }

        static void expect_types_assignable(const analyze_t& a_lvalue, const analyze_t& a_rvalue,
                                            const location_t& loc_set, analyze_params_t& params) {
            value_t lresult = a_lvalue.result;
            value_t rresult = a_rvalue.result;

            node_type_t* lvalue = lresult.value;
            node_type_t* rvalue = rresult.value;

            general_type_e l_type = to_general_type(lresult.type);
            general_type_e r_type = to_general_type(rresult.type);

            check_types_in_assign(l_type, r_type, loc_set, params);

            if (l_type == general_type_e::ARRAY &&
                r_type == general_type_e::ARRAY) {
                int l_level = lvalue->level();
                int r_level = rvalue->level();
                if (l_level != r_level) {
                    std::string error_msg = "wrong levels of arrays in assign: "
                                            + std::to_string(r_level) + " levels of array nesting "
                                            + "cannot be assigned to "
                                            + std::to_string(l_level) + " levels of array nesting";
                    throw error_analyze_t{loc_set, params.program_str, error_msg};
                }
            }
        }

        value_t& shift(const std::vector<value_t>& indexes, execute_params_t& params) {
            if (indexes.size() == 0)
                return e_value_;

            node_array_t* array = static_cast<node_array_t*>(e_value_.value);
            return array->shift(indexes, params);
        }

        analyze_t& shift_analyze(const std::vector<analyze_t>& indexes, analyze_params_t& params) {
            if (indexes.size() == 0)
                return a_value_;
            
            value_t value = a_value_.result;
            expect_types_eq(value.type, node_type_e::ARRAY, node_loc_t::loc(), params);

            node_array_t* array = static_cast<node_array_t*>(value.value);
            return array->shift(indexes, params);
        }

    public:
        settable_value_t(const location_t& loc) : node_loc_t(loc) {}

        value_t execute(node_indexes_t* indexes, execute_params_t& params) {
            assert(indexes);
            value_t& real_value = shift(indexes->execute(params), params);
            return real_value;
        }

        analyze_t analyze(node_indexes_t* ext_indexes, analyze_params_t& params) {
            assert(ext_indexes);
            std::vector<analyze_t> indexes = ext_indexes->analyze(params);
            if (indexes.size() > 0 && !is_setted)
                throw error_analyze_t{node_loc_t::loc(), params.program_str,
                                      "attempt to indexing by not init variable"};
            return shift_analyze(indexes, params);
        }

        value_t set_value(value_t new_value, execute_params_t& params) {
            is_setted = true;
            return e_value_ = new_value;
        }

        analyze_t set_value_analyze(analyze_t new_value, analyze_params_t& params,
                                    const location_t& loc_set) {
            if (is_setted)
                expect_types_assignable(a_value_, new_value, loc_set, params);

            is_setted = true;
            a_value_.result = new_value.result;
            a_value_.is_constexpr &= new_value.is_constexpr;
            return a_value_;
        }

        value_t set_value(node_indexes_t* indexes, value_t new_value, execute_params_t& params) {
            assert(indexes);
            value_t& real_value = shift(indexes->execute(params), params);
            is_setted = true;
            return real_value = new_value;
        }

        analyze_t set_value_analyze(node_indexes_t* ext_indexes, analyze_t new_value,
                                    analyze_params_t& params, const location_t& loc_set) {
            assert(ext_indexes);
            std::vector<analyze_t> indexes = ext_indexes->analyze(params);
            if (indexes.size() > 0 && !is_setted)
                throw error_analyze_t{node_loc_t::loc(), params.program_str,
                                      "attempt to indexing by not init variable"};

            analyze_t& shift_result = shift_analyze(indexes, params);

            if (is_setted)
                expect_types_assignable(shift_result, new_value, loc_set, params);

            is_setted = true;
            shift_result.result = new_value.result;
            shift_result.is_constexpr &= new_value.is_constexpr;
            return shift_result;
        }

        void set_predict(bool value) { a_value_.is_constexpr = value; }

        virtual ~settable_value_t() = default;
    };

    /* ----------------------------------------------------- */

    class node_variable_t final : public id_t,
                                  public settable_value_t {
    public:
        node_variable_t(const location_t& loc, std::string_view id)
        : id_t(id), settable_value_t(loc) {}

        using id_t::get_name;

        node_variable_t* copy(copy_params_t& params) const {
            return params.buf->add_node<node_variable_t>(node_loc_t::loc(), get_name());
        }

        void set_loc(const location_t& loc) { node_loc_t::set_loc(loc); }
    };
}