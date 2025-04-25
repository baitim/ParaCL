#pragma once

#include "ParaCL/nodes/common.hpp"

#include <algorithm>
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

    protected:
        void clear_memory() {
            std::ranges::for_each(arrays_, [](auto iter) {
                iter->clear();
            });
        }

    public:
        void add_array(node_memory_t* node) { assert(node); arrays_.push_back(node); }
        virtual ~memory_table_t() = default;
    };

    /* ----------------------------------------------------- */

    class node_return_t : public node_interpretable_t {
        node_expression_t* return_expr_ = nullptr;

    public:
        node_return_t(const location_t& loc, node_expression_t* return_expr)
        : node_interpretable_t(loc), return_expr_(return_expr) { assert(return_expr_); }

        void execute(execute_params_t& params) override {
            if (auto result = params.get_evaluated(this))
                params.add_return(*result);

            bool is_visited = params.is_visited(this);
            params.visit(this);

            execute_t result = return_expr_->execute(params);
            if (is_visited)
                params.add_return(result);
            else if (params.is_executed())
                params.add_value(this, result);
        }
    };

    /* ----------------------------------------------------- */

    class scope_base_t : public name_table_t,
                         public memory_table_t {
    protected:
        node_expression_t* last_expr_ = nullptr;
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

        void set_predict_impl(bool value) {
            through_statements([value](auto statement) { statement->set_predict(value); });
            if (return_expr_)
                return_expr_->set_predict(value);
        }

        void push_back_last_expr(buffer_t* buf) {
            if (last_expr_) {
                node_statement_t* last_stmt = buf->add_node<node_instruction_t>(last_expr_->loc(), last_expr_);
                statements_.push_back(last_stmt);
                last_expr_ = nullptr;
            }
        }

        template <typename ResultT, typename FuncT, typename ParamsT>
        ResultT process_return(FuncT&& func, ParamsT& params) {
            ResultT result = std::invoke(func, return_expr_, params);
            expect_types_eq(to_general_type(result.type), general_type_e::INTEGER,
                            return_expr_->loc(), params);
            return result;
        }

        node_return_t* make_return_node(copy_params_t& params) {
            return params.buf->add_node<node_return_t>(return_expr_->loc(), return_expr_);
        }

    public:
        scope_base_t(scope_base_t* parent) : parent_(parent) {} 

        void push_statement(node_statement_t* node) {
            assert(node);
            statements_.push_back(node);
        }

        void push_statement_build(node_statement_t* node, buffer_t* buf) {
            assert(node);
            if (return_expr_)
                return;

            push_back_last_expr(buf);
            statements_.push_back(node);
        }

        void push_expression(node_expression_t* node, buffer_t* buf) {
            assert(node);
            if (return_expr_)
                return;

            push_back_last_expr(buf);
            last_expr_ = node;
        }

        void set_return(node_expression_t* node) {
            assert(node);
            if (return_expr_)
                return;

            return_expr_ = node;
        }

        void add_return(node_expression_t* node, buffer_t* buf) {
            assert(node);
            if (return_expr_)
                return;

            push_back_last_expr(buf);
            return_expr_ = node;
        }

        void finish_return() {
            if (return_expr_)
                return;

            return_expr_ = last_expr_;
        }

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

    class node_scope_t final : public node_statement_t,
                               public scope_base_t {
    public:
        node_scope_t(const location_t& loc, scope_base_t* parent)
        : node_statement_t(loc), scope_base_t(parent) {}

        void execute(execute_params_t& params) override {
            if (params.is_visited(this))
                return;

            params.visit(this);
            if (return_expr_)
                params.insert_statement(make_return_node(params.copy_params));
            params.insert_statements(statements_.rbegin(), statements_.rend());
        }

        void analyze(analyze_params_t& params) override {
            auto&& analyze_funct = [](auto node, auto& params) { return node->analyze(params); };
            process_statements(analyze_funct, params);

            if (params.analyze_state == analyze_state_e::PROCESS && return_expr_) {
                params.stack.emplace(process_return<analyze_t>(analyze_funct, params));
                params.analyze_state = analyze_state_e::RETURN;
            }
            memory_table_t::clear_memory();
        }

        node_statement_t* copy(copy_params_t& params, scope_base_t* parent) const override {
            node_scope_t* scope = params.buf->add_node<node_scope_t>(node_loc_t::loc(), parent);
            return copy_impl<node_scope_t>(scope, params);
        }

        void set_predict(bool value) override {
            set_predict_impl(value);
        }
    };

    /* ----------------------------------------------------- */

    class node_scope_return_t final : public node_expression_t,
                                      public scope_base_t {
    private:
        template <typename ResultT>
        ResultT get_return(stack_t<ResultT>& stack) {
            auto&& result = stack.top();
            stack.pop();
            return result;
        }

    public:
        node_scope_return_t(const location_t& loc, scope_base_t* parent) : node_expression_t(loc), scope_base_t(parent) {}

        execute_t execute(execute_params_t& params) override {
            if (params.is_visited(this))
                return get_return<execute_t>(params.stack);

            params.visit(this);
            params.add_last_scope_r();
            params.insert_statement(make_return_node(params.copy_params));
            params.insert_statements(statements_.rbegin(), statements_.rend());
            return {};
        }

        analyze_t analyze(analyze_params_t& params) override {
            if (!return_expr_)
                throw error_type_deduction_t{node_loc_t::loc(), params.program_str,
                                                "missing required return statement"};

            analyze_state_e old_analyze_state = params.analyze_state;

            auto&& analyze_funct = [](auto node, auto& params) { return node->analyze(params); };
            process_statements(analyze_funct, params);

            analyze_t result;
            if (params.analyze_state == analyze_state_e::RETURN)
                result = get_return<analyze_t>(params.stack);
            else
                result = process_return<analyze_t>(analyze_funct, params);

            params.analyze_state = old_analyze_state;
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
}