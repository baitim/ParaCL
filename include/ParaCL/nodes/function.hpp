#pragma once

#include "ParaCL/nodes/variable.hpp"

#include <iomanip>
#include <unordered_set>

namespace paracl {
    class node_function_args_t final : public node_t,
                                       public node_loc_t {
        static const int DEFAULT_DUPLICATE_IDX = -1;

        std::unordered_set<std::string_view> name_table;
        int duplicate_idx_ = DEFAULT_DUPLICATE_IDX;
        
        std::vector<node_variable_t*> args_;

    private:
        template<typename FuncT, typename ParamsT>
        void process_args(FuncT&& func, ParamsT& params) {
            int i = 0;
            auto values = params.stack.pop_values(args_.size());
            for (auto arg : std::ranges::reverse_view(values))
                std::invoke(func, args_[i++], arg, params);
        }

    public:
        node_function_args_t(const location_t& loc) : node_loc_t(loc) {}

        void add_arg(node_variable_t* arg) {
            assert(arg);
            std::string_view name = arg->get_name();

            if (name_table.find(name) != name_table.end())
                duplicate_idx_ = args_.size();

            args_.push_back(arg);
            name_table.insert(name);
        }

        void execute(execute_params_t& params) {
            process_args([](auto* arg_ptr, auto& arg_value, auto& params) {
                arg_ptr->set_value(arg_value, params);
            }, params);
        }

        void analyze(analyze_params_t& params) {
            if (duplicate_idx_ != DEFAULT_DUPLICATE_IDX)
                throw error_analyze_t{args_[duplicate_idx_]->loc(), params.program_str,
                                      "attempt to create function with 2 similar variable names"};

            process_args([](auto* arg_ptr, auto& arg_value, auto& params) {
                arg_ptr->set_value_analyze(arg_value, params, arg_value.result.value->loc());
            }, params);
        }

        node_function_args_t* copy(copy_params_t& params) const {
            node_function_args_t* copy =
                params.buf->add_node<node_function_args_t>(node_loc_t::loc());;

            std::ranges::for_each(args_, [&params, &copy](auto arg) {
                node_variable_t* var_copy = arg->copy(params);
                copy->add_arg(var_copy);
                
            });
            return copy;
        }

        auto   begin() const noexcept { return args_.begin(); }
        auto   end()   const noexcept { return args_.end  (); }
        size_t size()  const noexcept { return args_.size (); }
    };

    /* ----------------------------------------------------- */

    class node_function_call_args_t final : public node_t,
                                            public node_loc_t {
        std::vector<node_expression_t*> args_;

    private:
        template<typename FuncT, typename ParamsT>
        void process_args(FuncT&& func, ParamsT& params) {
            std::ranges::for_each(args_, [&params, &func](auto arg) {
                params.stack.push_value(std::invoke(func, arg, params));
            });
        }

    public:
        node_function_call_args_t(const location_t& loc) : node_loc_t(loc) {}

        void add_arg(node_expression_t* arg) {
            assert(arg);
            args_.push_back(arg);
        }

        void execute(execute_params_t& params) {
            process_args([](auto arg, auto& params) {
                return arg->execute(params);
            }, params);
        }

        void analyze(analyze_params_t& params) {
            process_args([](auto arg, auto& params) {
                analyze_t arg_value = arg->analyze(params);
                expect_types_eq(to_general_type(arg_value.result.type), general_type_e::INTEGER,
                                arg->loc(), params);
                return arg_value;
            }, params);
        }

        void set_predict(bool value) {
            std::ranges::for_each(args_, [value](auto arg) {
                arg->set_predict(value);
            });
        }

        node_function_call_args_t* copy(copy_params_t& params, scope_base_t* parent) const {
            node_function_call_args_t* copy =
                params.buf->add_node<node_function_call_args_t>(node_loc_t::loc());

            std::ranges::for_each(args_, [&](auto arg) {
                copy->add_arg(arg->copy(params, parent));
            });
            return copy;
        }

        size_t size() const noexcept { return args_.size(); }
    };

    /* ----------------------------------------------------- */

    class node_function_t final : public node_simple_type_t,
                                  public id_t {
        node_function_args_t* args_;
        node_block_t*         block_;

        analyze_t a_value_;

        static inline const std::string default_function_name_prefix  = "#default_function_name_";
        static inline const std::string default_function_name_postfix = "_#";
        static inline int               default_function_name_index   = 1;

    private:
        template<typename ElemT, typename FuncT>
        ElemT process_real(FuncT&& func) {
            assert(block_);
            std::invoke(func, args_);
            return std::invoke(func, block_);
        }

        std::string get_function_name(std::string_view id) {
            if (id.empty()) {
                std::ostringstream oss;
                oss << default_function_name_prefix
                    << std::setw(3) << std::setfill('0') << default_function_name_index++
                    << default_function_name_postfix;
                return oss.str();
            }
            return std::string(id);
        }

    public:
        node_function_t(const location_t& loc, node_function_args_t* args,
                        node_block_t* block, std::string_view id)
        : node_simple_type_t(loc), id_t(get_function_name(id)), args_(args), block_(block) {}

        void bind_block(node_block_t* block) {
            block_ = block;
            assert(block_);
            assert(args_);
        }

        value_t execute(execute_params_t& params) override {
            return {node_type_e::FUNCTION, this};
        }

        analyze_t analyze(analyze_params_t& params) override {
            return {node_type_e::FUNCTION, this};
        }

        value_t real_execute(execute_params_t& params) {
            return process_real<value_t>([&params](auto arg) {
                return arg->execute(params);
            });
        }

        analyze_t real_analyze(analyze_params_t& params) {
            a_value_ = process_real<analyze_t>([&params](auto arg) {
                return arg->analyze(params);
            });
            params.copy_params.global_scope.add_variable(this);
            return a_value_;
        }

        void print(execute_params_t& params) override { *(params.os) << "function " << get_name() << "\n"; }

        node_expression_t* copy(copy_params_t& params, scope_base_t* parent) const override {
            assert(block_);

            node_function_args_t* args_copy = args_->copy(params);

            auto& buf = params.buf;
            node_function_t* function_copy =
                buf->add_node<node_function_t>(node_loc_t::loc(), args_copy, nullptr, get_name());
            params.global_scope.add_variable(function_copy);

            node_block_t* block_copy = block_->copy_with_args(
                params, parent, args_copy->begin(), args_copy->end()
            );
            function_copy->bind_block(block_copy);

            return function_copy;
        }

        size_t count_args() const { return args_->size(); }

        analyze_t a_value() const { return a_value_; }
    };

    /* ----------------------------------------------------- */

    class node_function_call_t final : public node_expression_t {
        node_expression_t*         function_;
        node_function_call_args_t* args_;

        bool is_call_by_name_;

    private:
        void process_count_arguments(size_t cnt_args_decl, size_t cnt_args_call,
                                     analyze_params_t& params) const {
            if (cnt_args_decl == cnt_args_call)
                return;

            throw error_analyze_t{node_loc_t::loc(), params.program_str,
                  "different count of declared arguments("   + std::to_string(cnt_args_decl)
                + ") and count arguments for function call(" + std::to_string(cnt_args_call) + ")"
            };
        }

        node_function_t* get_function(const name_table_t& global_scope) const {
            assert(is_call_by_name_);
            node_function_t*      function = static_cast<node_function_t*>(function_);
            node_function_t* real_function = static_cast<node_function_t*>(
                global_scope.get_var_node(function->get_name())
            );
            return real_function;
        }

    public:
        node_function_call_t(const location_t& loc, node_expression_t* function,
                             node_function_call_args_t* args, bool is_call_by_name)
        : node_expression_t(loc), function_(function), args_(args), is_call_by_name_(is_call_by_name) {
            assert(function_);
            assert(args_);
        }

        value_t execute(execute_params_t& params) override {
            args_->execute(params);
            value_t func_value = function_->execute(params);

            node_function_t* func = static_cast<node_function_t*>(func_value.value);
            return func->real_execute(params);
        }

        analyze_t analyze(analyze_params_t& params) override {
            if (is_call_by_name_) {
                node_function_t* function = get_function(params.copy_params.global_scope);
                if (function) {
                    analyze_t result = function->a_value();
                    result.is_constexpr = false;
                    return result;
                }
            }

            args_->analyze(params);
            analyze_t function = function_->analyze(params);

            expect_types_eq(function.result.type, node_type_e::FUNCTION, node_loc_t::loc(), params);

            node_function_t* func = static_cast<node_function_t*>(function.result.value);
            process_count_arguments(func->count_args(), args_->size(), params);

            return func->real_analyze(params);
        }

        node_expression_t* copy(copy_params_t& params, scope_base_t* parent) const override {
            node_expression_t* function_copy = nullptr;
            if (is_call_by_name_) {
                function_copy = get_function(params.global_scope);
                if (!function_copy)
                    function_copy = function_->copy(params, parent);
            } else {
                function_copy = function_->copy(params, parent);
            }

            return params.buf->add_node<node_function_call_t>(node_loc_t::loc(),
                                                              function_copy,
                                                              args_->copy(params, parent),
                                                              is_call_by_name_);
        }

        void set_predict(bool value) override {
            function_->set_predict(value);
            args_->set_predict(value);
        }
    };
}