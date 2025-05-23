#pragma once

#include "ParaCL/nodes/array.hpp"

namespace paracl {
    class settable_value_t : public node_t,
                             public node_loc_t,
                             public node_settable_t {
        bool is_setted = false;
        execute_t e_value_;
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
            general_type_e l_type = to_general_type(a_lvalue.type);
            general_type_e r_type = to_general_type(a_rvalue.type);
            check_types_in_assign(l_type, r_type, loc_set, params);

            if (l_type == general_type_e::ARRAY &&
                r_type == general_type_e::ARRAY) {
                int l_level = a_lvalue.value->level();
                int r_level = a_rvalue.value->level();
                if (l_level != r_level) {
                    std::string error_msg = "wrong levels of arrays in assign: "
                                            + std::to_string(r_level) + " levels of array nesting "
                                            + "cannot be assigned to "
                                            + std::to_string(l_level) + " levels of array nesting";
                    throw error_analyze_t{loc_set, params.program_str, error_msg};
                }
            }
        }

        execute_t& shift(const std::vector<execute_t>& indexes, execute_params_t& params) {
            if (indexes.size() == 0)
                return e_value_;

            node_array_t* array = static_cast<node_array_t*>(e_value_.value);
            return array->shift(indexes, params);
        }

        analyze_t& shift_analyze(const std::vector<analyze_t>& indexes, analyze_params_t& params) {
            if (indexes.size() == 0)
                return a_value_;
            
            expect_types_eq(a_value_.type, node_type_e::ARRAY, node_loc_t::loc(), params);
            node_array_t* array = static_cast<node_array_t*>(a_value_.value);
            return array->shift(indexes, params);
        }

    public:
        settable_value_t(const location_t& loc) : node_loc_t(loc) {}

        execute_t execute(execute_params_t& params) override {
            return e_value_;
        }

        execute_t execute(node_indexes_t* indexes, execute_params_t& params) {
            assert(indexes);
            execute_t real_value = shift(indexes->execute(params), params);
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

        execute_t set_value(execute_t new_value, execute_params_t& params) override {
            is_setted = true;
            return e_value_ = new_value;
        }

        analyze_t set_value_analyze(analyze_t new_value, analyze_params_t& params,
                                    const location_t& loc_set) {
            if (is_setted)
                expect_types_assignable(a_value_, new_value, loc_set, params);

            is_setted = true;
            a_value_.type  = new_value.type;
            a_value_.value = new_value.value;
            a_value_.is_constexpr &= new_value.is_constexpr;
            return a_value_;
        }

        execute_t set_value(node_indexes_t* indexes, execute_t new_value, execute_params_t& params) {
            assert(indexes);
            execute_t& real_value = shift(indexes->execute(params), params);
            if (!params.is_executed())
                return {};

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
            shift_result.type  = new_value.type;
            shift_result.value = new_value.value;
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