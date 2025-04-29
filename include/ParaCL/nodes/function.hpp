#pragma once

#include "ParaCL/nodes/assign.hpp"

#include <iomanip>
#include <unordered_set>

namespace paracl {
    class node_function_args_initializator_t final : public node_interpretable_t {
        std::vector<node_variable_t*> args_;

    private:
        node_instruction_t* make_assign_instruction(node_variable_t* var, node_expression_t* rvalue,
                                                    buffer_t* buf) {
            node_indexes_t* indexes = buf->add_node<node_indexes_t>(var->loc());
            node_lvalue_t*  lvalue  = buf->add_node<node_lvalue_t> (var->loc(), var, indexes);
            node_assign_t*  assign  = buf->add_node<node_assign_t> (var->loc(), lvalue, rvalue);
            return buf->add_node<node_instruction_t>(assign->loc(), assign);
        }

    public:
        node_function_args_initializator_t(const location_t& loc, const std::vector<node_variable_t*>& args)
        : node_interpretable_t(loc), args_(args) {}

        void execute(execute_params_t& params) override {
            if (params.is_visited(this))
                return;
            params.visit(this);

            auto values = params.stack.pop_values(args_.size());
            std::vector<node_interpretable_t*> instructions;
            buffer_t* buf = params.buf();
            for (int i = 0, end = args_.size(); i < end; ++i)
                instructions.push_back(make_assign_instruction(args_[i], values[i].value, buf));
            params.insert_statements(instructions.rbegin(), instructions.rend());
        }
    };

    /* ----------------------------------------------------- */

    class node_function_args_t final : public node_t,
                                       public node_loc_t {
        static const int DEFAULT_DUPLICATE_IDX = -1;

        std::unordered_set<std::string_view> name_table;
        int duplicate_idx_ = DEFAULT_DUPLICATE_IDX;
        
        std::vector<node_variable_t*> args_;

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
            params.insert_statement(
                params.buf()->add_node<node_function_args_initializator_t>(node_loc_t::loc(), args_)
            );
        }

        void analyze(analyze_params_t& params) {
            if (duplicate_idx_ != DEFAULT_DUPLICATE_IDX)
                throw error_analyze_t{args_[duplicate_idx_]->loc(), params.program_str,
                                      "attempt to create function with 2 similar variable names"};

            int i = 0;
            auto values = params.stack.pop_values(args_.size());
            for (auto arg : std::ranges::reverse_view(values))
                args_[i++]->set_value_analyze(arg, params, arg.value->loc());
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

    class node_stack_filler_t final : public node_interpretable_t {
        node_expression_t* expr_;

    public:
        node_stack_filler_t(const location_t& loc, node_expression_t* expr)
        : node_interpretable_t(loc), expr_(expr) { assert(expr_); }

        void execute(execute_params_t& params) override {
            if (auto result = params.get_evaluated(this)) {
                params.stack.emplace(*result);
                return;
            }

            execute_t result = expr_->execute(params);
            if (!params.is_executed())
                return;

            params.stack.emplace(result);
            params.add_value(this, result);
        }
    };

    /* ----------------------------------------------------- */

    class node_function_call_args_t final : public node_t,
                                            public node_loc_t {
        std::vector<node_expression_t*> args_;

    public:
        node_function_call_args_t(const location_t& loc) : node_loc_t(loc) {}

        void add_arg(node_expression_t* arg) {
            assert(arg);
            args_.push_back(arg);
        }

        void execute(execute_params_t& params) {
            std::ranges::for_each(args_, [&params](auto arg) {
                node_stack_filler_t* stack_filler = params.buf()->add_node<node_stack_filler_t>(arg->loc(), arg);
                params.insert_statement(stack_filler);
            });
        }

        void analyze(analyze_params_t& params) {
            std::ranges::for_each(args_, [&params](auto arg) {
                analyze_t arg_value = arg->analyze(params);
                expect_types_eq(to_general_type(arg_value.type), general_type_e::INTEGER,
                                arg->loc(), params);
                params.stack.emplace(arg_value);
            });
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
        node_scope_return_t*  body_;

        static constexpr std::string_view default_function_name_prefix  = "#default_function_name_";
        static constexpr std::string_view default_function_name_postfix = "_#";
        static inline int                 default_function_name_index   = 1;

    private:
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
                        node_scope_return_t* body, std::string_view id)
        : node_simple_type_t(loc), id_t(get_function_name(id)), args_(args), body_(body) {}

        void bind_body(node_scope_return_t* body) {
            body_ = body;
            assert(body_);
            assert(args_);
        }

        execute_t execute(execute_params_t& params) override {
            return {node_type_e::FUNCTION, this};
        }

        analyze_t analyze(analyze_params_t& params) override {
            return {node_type_e::FUNCTION, this};
        }

        void upload_state(execute_params_t& params) {
            params.upload_variables(args_->begin(), args_->end());
        }

        void load_state(execute_params_t& params) {
            params.load_variables(args_->begin(), args_->end());
        }

        void args_execute(execute_params_t& params) {
            args_->execute(params);
        }

        execute_t body_execute(execute_params_t& params) {
            return body_->execute(params);
        }

        analyze_t real_analyze(analyze_params_t& params) {
            assert(body_);
            args_->analyze(params);
            return body_->analyze(params);
        }

        void print(execute_params_t& params) override { *(params.os) << "function " << get_name() << "\n"; }

        node_expression_t* copy(copy_params_t& params, scope_base_t* parent) const override {
            assert(body_);

            node_function_args_t* args_copy = args_->copy(params);

            auto& buf = params.buf;
            node_function_t* function_copy =
                buf->add_node<node_function_t>(node_loc_t::loc(), args_copy, nullptr, get_name());
            params.global_scope.add_variable(function_copy);

            node_scope_return_t* body_copy = body_->copy_with_args(
                params, parent, args_copy->begin(), args_copy->end()
            );
            function_copy->bind_body(body_copy);

            return function_copy;
        }

        size_t count_args() const { return args_->size(); }
    };

    /* ----------------------------------------------------- */

    class node_function_state_loader_t final : public node_interpretable_t {
        node_function_t* func_;

    public:
        node_function_state_loader_t(const location_t& loc, node_function_t* func)
        : node_interpretable_t(loc), func_(func) { assert(func_); }

        void execute(execute_params_t& params) override {
            func_->load_state(params);
        }
    };

    /* ----------------------------------------------------- */

    class node_function_call_t : public node_expression_t {
        static inline int default_analyze_return = 42;

    protected:
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

        static id_t* function_to_id(node_expression_t* function) {
            return static_cast<id_t*>(static_cast<node_function_t*>(function));
        }

        std::optional<analyze_t> analyze_call_by_name(analyze_params_t& params) const {
            if (!is_call_by_name_)
                return std::nullopt;
 
            if (params.is_name_visited(function_to_id(function_)))
                return analyze_t{make_number(default_analyze_return, params, node_loc_t::loc()), false};

            return std::nullopt;
        }

        execute_t execute_function_body(bool is_visiting_prev, node_function_t* function,
                                        execute_params_t& params) {
            params.is_visiting_prev = is_visiting_prev;
            execute_t result = function->body_execute(params);
            params.is_visiting_prev = false;
            return result;
        }

    public:
        node_function_call_t(const location_t& loc, node_expression_t* function,
                             node_function_call_args_t* args, bool is_call_by_name)
        : node_expression_t(loc), function_(function), args_(args), is_call_by_name_(is_call_by_name) {
            assert(function_);
            assert(args_);
        }

        execute_t execute(execute_params_t& params) override {        
            execute_t func_value = function_->execute(params);
            node_function_t* function = static_cast<node_function_t*>(func_value.value);

            int  number_of_visit = params.number_of_visit(this);
            bool is_prev_upload  = params.is_name_visited(function);

            if (!params.is_visited(this)) {
                params.visit(this);
                if (is_prev_upload)
                    function->upload_state(params);
                function->args_execute(params);
                args_->execute(params);
                return {};
            }
            params.visit(this);
            params.visit_name(function_to_id(function), params.get_step());

            if (!is_prev_upload)
                function->upload_state(params);

            bool is_uploaded = (is_prev_upload && (number_of_visit == 1));
            if (is_uploaded) {
                params.insert_statement(
                    params.buf()->add_node<node_function_state_loader_t>(node_loc_t::loc(), function)
                );
            }

            execute_t result = execute_function_body(is_uploaded, function, params);
            if (params.is_executed())
                params.unvisit_name(function_to_id(function), params.get_step());
            return result;
        }

        analyze_t analyze(analyze_params_t& params) override {
            if (auto result = analyze_call_by_name(params))
                return *result;

            if (is_call_by_name_)
                params.visit_name(function_to_id(function_));

            args_->analyze(params);

            analyze_t function_a = function_->analyze(params);
            expect_types_eq(function_a.type, node_type_e::FUNCTION, node_loc_t::loc(), params);

            node_function_t* function = static_cast<node_function_t*>(function_a.value);
            process_count_arguments(function->count_args(), args_->size(), params);

            analyze_t result = function->real_analyze(params);
            if (is_call_by_name_)
                params.unvisit_name(function_to_id(function_));
            return result;
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

    /* ----------------------------------------------------- */

    class node_function_call_wrapper_t final : public node_function_call_t {
    public:
        node_function_call_wrapper_t(const location_t& loc, node_expression_t* function,
                                     node_function_call_args_t* args, bool is_call_by_name)
        : node_function_call_t(loc, function, args, is_call_by_name) {}

        execute_t execute(execute_params_t& params) override {
            if (auto result = params.get_evaluated(this))
                return *result;

            if (!params.is_visited(this)) {
                params.visit(this);
                params.insert_statement_before(
                    make_empty_interpretable(node_loc_t::loc(), params.copy_params)
                );
            }

            params.execute_state = execute_state_e::PROCESS;
            execute_t result = node_function_call_t::execute(params);
            if (params.is_executed()) {
                params.erase_statement_before();
                params.add_value(this, result);
            }
            return result;
        }
    };
}