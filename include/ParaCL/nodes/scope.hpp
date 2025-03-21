#pragma once

#include "ParaCL/nodes/common.hpp"

#include <algorithm>

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

    class scope_base_t : public name_table_t,
                         public memory_table_t {
        scope_base_t* parent_;

    protected:
        std::vector<node_statement_t*> statements_;
        node_expression_t* return_expr_ = nullptr;
        node_expression_t* last_expr_   = nullptr;

    protected:
        template <typename FuncT, typename ParamsT>
        bool process_statements(FuncT&& func, ParamsT& params) const {
            size_t old_stack_size = params.stack.size();
            for (auto statement : statements_) {
                std::forward<FuncT>(func)(statement, params);
                if (old_stack_size != params.stack.size())
                    return true;
            }
            return false;
        }

        template <typename FuncT>
        void through_statements(FuncT&& func) const {
            std::ranges::for_each(statements_, [&func](auto statement) {
                std::forward<FuncT>(func)(statement);
            });
        }

        template <typename ScopeT, typename ResT>
        ResT* copy_impl(ScopeT* scope, copy_params_t& params) const {
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
            return_expr_ = node;
        }

        void add_return(node_expression_t* node, buffer_t* buf) {
            assert(node);
            if (return_expr_)
                return;

            push_back_last_expr(buf);
            return_expr_ = node;
        }

        void update_return() {
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
            bool is_return = process_statements([](auto statements, auto& params) {
                                                    statements->execute(params);
                                                }, params);

            if (!is_return && return_expr_)
                params.stack.push_value(return_expr_->execute(params));

            memory_table_t::clear_memory();
        }

        void analyze(analyze_params_t& params) override {
            through_statements([&params](auto statement) { statement->analyze(params); });
            bool is_return = process_statements([](auto statements, auto& params) {
                                                    statements->analyze(params);
                                                }, params);

            if (!is_return && return_expr_)
                params.stack.push_value(return_expr_->analyze(params));

            memory_table_t::clear_memory();
        }

        node_statement_t* copy(copy_params_t& params, scope_base_t* parent) const override {
            node_scope_t* scope = params.buf->add_node<node_scope_t>(node_loc_t::loc(), parent);
            return copy_impl<node_scope_t, node_statement_t>(scope, params);
        }

        void set_predict(bool value) override {
            set_predict_impl(value);
        }
    };

    /* ----------------------------------------------------- */

    class node_block_t final : public node_expression_t,
                               public scope_base_t {

    public:
        node_block_t(const location_t& loc, scope_base_t* parent)
        : node_expression_t(loc), scope_base_t(parent) {}

        value_t execute(execute_params_t& params) override {
            bool is_return = process_statements([](auto statement, auto& params) {
                                                    statement->execute(params);
                                                }, params);

            if (is_return)
                return params.stack.pop_value();

            value_t result = return_expr_->execute(params);
            memory_table_t::clear_memory();
            return result;
        }

        analyze_t analyze(analyze_params_t& params) override {
            if (!return_expr_)
                throw error_type_deduction_t{node_loc_t::loc(), params.program_str,
                                                "missing required return statement"};

            through_statements([&params](auto statement) { statement->analyze(params); });
            bool is_return = process_statements([](auto statement, auto& params) {
                                                    statement->analyze(params);
                                                }, params);

            if (is_return)
                return params.stack.pop_value();

            analyze_t result = return_expr_->analyze(params);
            memory_table_t::clear_memory();
            return result;
        }

        node_expression_t* copy(copy_params_t& params, scope_base_t* parent) const override {
            node_block_t* block = params.buf->add_node<node_block_t>(node_loc_t::loc(), parent);
            return copy_impl<node_block_t, node_expression_t>(block, params);
        }

        template <typename IterT>
        node_block_t* copy_with_args(copy_params_t& params, scope_base_t* parent,
                                        IterT args_begin, IterT args_end) const {
            auto* block_copy = params.buf->add_node<node_block_t>(node_loc_t::loc(), parent);
            block_copy->add_variables(args_begin, args_end);
            return copy_impl<node_block_t, node_block_t>(block_copy, params);
        }

        void set_predict(bool value) override {
            set_predict_impl(value);
        }
    };
}