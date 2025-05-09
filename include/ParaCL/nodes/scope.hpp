#pragma once

#include "ParaCL/nodes/simple_types.hpp"

#include <functional>

namespace paracl {
    class node_memory_t {
    public:
        virtual void clear() = 0;
        virtual ~node_memory_t() = default;
    };

    /* ----------------------------------------------------- */

    class memory_table_t {
        std::vector<node_memory_t*> arrays_;

    public:
        void add_array(node_memory_t* node) { assert(node); arrays_.push_back(node); }

        void clear_memory() {
            std::ranges::for_each(arrays_, [](auto iter) {
                iter->clear();
            });
        }

        virtual ~memory_table_t() = default;
    };

    /* ----------------------------------------------------- */

    class node_memory_cleaner_t : public node_interpretable_t {
        memory_table_t* memory_table_;

    public:
        node_memory_cleaner_t(const location_t& loc, memory_table_t* memory_table)
        : node_interpretable_t(loc), memory_table_(memory_table) { assert(memory_table_); }

        void execute(execute_params_t& params) override {
            memory_table_->clear_memory();
        }
    };

    /* ----------------------------------------------------- */

    class node_return_t : public node_interpretable_t {
        node_expression_t* return_expr_ = nullptr;

    public:
        node_return_t(const location_t& loc, node_expression_t* return_expr)
        : node_interpretable_t(loc), return_expr_(return_expr) { assert(return_expr_); }

        void execute(execute_params_t& params) override {
            execute_t result = return_expr_->execute(params);
            if (params.is_executed())
                params.add_return(result);
        }
    };

    /* ----------------------------------------------------- */

    class node_scope_return_t;
    class node_scope_t;

    class scope_base_t : public name_table_t,
                         public memory_table_t {
    protected:
        scope_base_t* parent_;
        std::vector<node_statement_t*> statements_;
        node_expression_t* return_expr_ = nullptr;

    protected:
        template <typename FuncT, typename ParamsT>
        void process_statements(FuncT&& func, ParamsT& params) const {
            for (auto statement : statements_) {
                std::invoke(func, statement, params);
                if (params.analyze_state != analyze_state_e::PROCESS)
                    return;
            }
        }

        template <typename FuncT>
        void through_statements(FuncT&& func) const {
            std::ranges::for_each(statements_, [&func](auto statement) {
                std::invoke(func, statement);
            });
        }

        template <typename ScopeT>
        ScopeT* copy_impl(ScopeT* scope, copy_params_t& params) const {
            through_statements([&](auto statement) { scope->push_statement(statement->copy(params, scope)); });
            if (return_expr_)
                scope->set_return(return_expr_->copy(params, scope));
            return scope;
        }

        template <typename ScopeT>
        ScopeT* simple_copy_impl(ScopeT* scope, copy_params_t& params) const {
            through_statements([&](auto statement) { scope->push_statement(statement); });
            if (return_expr_)
                scope->set_return(return_expr_);
            return scope;
        }

        void set_predict_impl(bool value) {
            through_statements([value](auto statement) { statement->set_predict(value); });
            if (return_expr_)
                return_expr_->set_predict(value);
        }

        analyze_t analyze_return(analyze_params_t& params) {
            analyze_t result = return_expr_->analyze(params);
            if (result.value != nullptr)
                expect_types_eq(to_general_type(result.type), general_type_e::INTEGER,
                                return_expr_->loc(), params);
            return result;
        }

        node_memory_cleaner_t* make_memory_cleaner(const location_t& loc, copy_params_t& params) {
            return params.buf->add_node<node_memory_cleaner_t>(loc, this);
        }

        node_return_t* make_return_node(copy_params_t& params) {
            return params.buf->add_node<node_return_t>(return_expr_->loc(), return_expr_);
        }

    public:
        scope_base_t(scope_base_t* parent) : parent_(parent) {} 

        void push_statement(node_statement_t* node) {
            assert(node);

            if (!return_expr_)
                statements_.push_back(node);
        }

        void set_return(node_expression_t* node) {
            assert(node);
            if (return_expr_)
                return;

            return_expr_ = node;
        }

        void finish_return(buffer_t* buf) {
            if (return_expr_)
                return;

            if (!statements_.empty()) {
                copy_params_t params{buf};
                return_expr_ = statements_.back()->to_expression(params, this);
                if (return_expr_)
                    statements_.pop_back();
            }
        }

        bool empty() const { return statements_.empty() && !return_expr_; }

        id_t* get_node(std::string_view name) const {
            for (auto scope = this; scope; scope = scope->parent_) {
                id_t* var_node = scope->get_var_node(name);
                if (var_node)
                    return static_cast<id_t*>(var_node);
            }
            return nullptr;
        }
    };

    /* ----------------------------------------------------- */

    class node_scope_return_t final : public node_expression_t,
                                      public scope_base_t {
    public:
        node_scope_return_t(const location_t& loc, scope_base_t* parent)
        : node_expression_t(loc), scope_base_t(parent) {}

        execute_t execute(execute_params_t& params) override {
            if (params.is_visited(this)) {
                execute_t result = params.stack.pop_value();
                if (!result.value)
                    throw error_execute_t{node_loc_t::loc(), params.program_str, "missing return value"};
                return result;
            }
            params.visit(this);

            params.insert_statement(make_memory_cleaner(node_loc_t::loc(), params.copy_params));
            params.add_return_receiver();
            params.insert_statement(make_return_node(params.copy_params));
            params.insert_statements(statements_.rbegin(), statements_.rend());
            return {};
        }

        analyze_t analyze(analyze_params_t& params) override {
            if (!return_expr_)
                throw error_type_deduction_t{node_loc_t::loc(), params.program_str,
                                                "missing required return statement"};

            analyze_state_e& state = params.analyze_state;
            analyze_state_e old_analyze_state = state;

            auto&& analyze_funct = [](auto node, auto& params) { return node->analyze(params); };
            process_statements(analyze_funct, params);

            analyze_t result;
            if (state == analyze_state_e::RETURN) {
                result = params.stack.pop_value();
                state = analyze_state_e::PROCESS;
                analyze_return(params);
            } else {
                result = analyze_return(params);
            }

            state = old_analyze_state;
            memory_table_t::clear_memory();
            return result;
        }

        node_expression_t* copy(copy_params_t& params, scope_base_t* parent) const override {
            node_scope_return_t* scope_r = params.buf->add_node<node_scope_return_t>(node_loc_t::loc(), parent);
            return copy_impl<node_scope_return_t>(scope_r, params);
        }

        template <typename IterT>
        node_scope_return_t* copy_with_args(copy_params_t& params, scope_base_t* parent,
                                          IterT args_begin, IterT args_end) const {
            node_scope_return_t* scope_r = params.buf->add_node<node_scope_return_t>(node_loc_t::loc(), parent);
            scope_r->add_variables(args_begin, args_end);
            return copy_impl<node_scope_return_t>(scope_r, params);
        }

        void set_predict(bool value) override {
            set_predict_impl(value);
        }
    };

    /* ----------------------------------------------------- */

    class node_scope_t final : public node_strong_statement_t,
                               public scope_base_t {
    public:
        node_scope_t(const location_t& loc, scope_base_t* parent)
        : node_strong_statement_t(loc), scope_base_t(parent) {}

        void execute(execute_params_t& params) override {
            if (params.is_visited(this))
                return;

            params.visit(this);
            params.insert_statement(make_memory_cleaner(node_loc_t::loc(), params.copy_params));
            if (return_expr_)
                params.insert_statement(make_return_node(params.copy_params));
            params.insert_statements(statements_.rbegin(), statements_.rend());
        }

        void analyze(analyze_params_t& params) override {
            auto&& analyze_funct = [](auto node, auto& params) { return node->analyze(params); };
            process_statements(analyze_funct, params);

            if (params.analyze_state == analyze_state_e::PROCESS && return_expr_) {
                params.stack.emplace(analyze_return(params));
                params.analyze_state = analyze_state_e::RETURN;
            }
            memory_table_t::clear_memory();
        }

        node_statement_t* copy(copy_params_t& params, scope_base_t* parent) const override {
            node_scope_t* scope = params.buf->add_node<node_scope_t>(node_loc_t::loc(), parent);
            return copy_impl<node_scope_t>(scope, params);
        }

        node_scope_return_t* to_scope_r(copy_params_t& params, scope_base_t* parent) const {
            node_scope_return_t* scope_r = params.buf->add_node<node_scope_return_t>(node_loc_t::loc(), parent);
            simple_copy_impl<node_scope_return_t>(scope_r, params);
            scope_r->finish_return(params.buf);
            return scope_r;
        }

        void set_predict(bool value) override {
            set_predict_impl(value);
        }
    };
}