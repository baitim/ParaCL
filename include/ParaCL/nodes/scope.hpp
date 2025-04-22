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

    class scope_base_t : public name_table_t,
                         public memory_table_t {
        scope_base_t* parent_;
        node_expression_t* last_expr_ = nullptr;

    protected:
        std::vector<node_statement_t*> statements_;
        node_expression_t* return_expr_ = nullptr;

    protected:
        template <typename FuncT, typename ParamsT>
        void process_statements(FuncT&& func, ParamsT& params) const {
            for (auto statement : statements_) {
                std::invoke(func, statement, params);
                if (params.stack_state != stack_state_e::PROCESS)
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
        ResultT eval_return(FuncT func, ParamsT& params) {
            ResultT result = std::invoke(func, return_expr_, params);
            expect_types_eq(to_general_type(result.type), general_type_e::INTEGER,
                            return_expr_->loc(), params);
            return result;
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

    private:
        template<typename ResultT, typename FuncT, typename ParamsT>
        void process(FuncT func, ParamsT& params) {
            process_statements(func, params);

            if (params.stack_state == stack_state_e::PROCESS && return_expr_) {
                params.stack.emplace(eval_return<ResultT>(func, params));
                params.stack_state = stack_state_e::RETURN;
            }
            memory_table_t::clear_memory();
        }

    public:
        node_scope_t(const location_t& loc, scope_base_t* parent)
        : node_statement_t(loc), scope_base_t(parent) {} 

        void execute(execute_params_t& params) override {
            process<execute_t>([](auto node, auto& params) { return node->execute(params); }, params);
        }

        void analyze(analyze_params_t& params) override {
            process<analyze_t>([](auto node, auto& params) { return node->analyze(params); }, params);
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

    class node_scope_return_t : public node_expression_t,
                                public scope_base_t {
    protected:
        template <typename ElemT, typename ParamsT>
        ElemT get_return(ParamsT& params) {
            auto& stack = params.stack;
            assert(!stack.empty());
            auto&& result = stack.top();
            stack.pop();
            return result;
        }

    public:
        node_scope_return_t(const location_t& loc, scope_base_t* parent)
        : node_expression_t(loc), scope_base_t(parent) {}

        void set_predict(bool value) override {
            set_predict_impl(value);
        }
    };

    /* ----------------------------------------------------- */

    class node_block_t final : public node_scope_return_t {
    private:
        template <typename ResultT, typename StatementFuncT, typename ReturnFuncT, typename ParamsT>
        ResultT process(StatementFuncT statement_func, ReturnFuncT return_func, ParamsT& params) {
            stack_state_e old_stack_state = params.stack_state;
            process_statements(statement_func, params);

            ResultT result;
            if (params.stack_state == stack_state_e::RETURN)
                result = get_return<ResultT>(params);
            else
                result = eval_return<ResultT>(return_func, params);

            params.stack_state = old_stack_state;
            memory_table_t::clear_memory();
            return result;
        }

        template <typename ElemT, typename ParamsT>
        ElemT get_return(ParamsT& params) {
            auto& stack = params.stack;
            auto&& result = stack.top();
            stack.pop();
            return result;
        }

    public:
        node_block_t(const location_t& loc, scope_base_t* parent)
        : node_scope_return_t(loc, parent) {}

        execute_t execute(execute_params_t& params) override {
            return process<execute_t>(
                [](auto node, auto& params) { node->execute(params); },
                [](auto node, auto& params) { return node->execute(params); },
                params
            );
        }

        analyze_t analyze(analyze_params_t& params) override {
            if (!return_expr_)
                throw error_type_deduction_t{node_loc_t::loc(), params.program_str,
                                                "missing required return statement"};

            return process<analyze_t>(
                [](auto node, auto& params) { node->analyze(params); },
                [](auto node, auto& params) { return node->analyze(params); },
                params
            );
        }

        node_expression_t* copy(copy_params_t& params, scope_base_t* parent) const override {
            node_block_t* block = params.buf->add_node<node_block_t>(node_loc_t::loc(), parent);
            return copy_impl<node_block_t>(block, params);
        }
    };

    /* ----------------------------------------------------- */

    class node_function_body_t final : public node_scope_return_t {
    private:
    template <typename ResultT, typename StatementFuncT, typename ReturnFuncT, typename ParamsT>
    ResultT process(StatementFuncT statement_func, ReturnFuncT return_func, ParamsT& params) {
        stack_state_e old_stack_state = params.stack_state;
        process_statements(statement_func, params);

        ResultT result;
        switch (params.stack_state) {
            case stack_state_e::FUNCTION_CALL:
                return result;

            case stack_state_e::RETURN:
                result = get_return<ResultT>(params);
                break;

            case stack_state_e::PROCESS:
                result = eval_return<ResultT>(return_func, params);
                break;

            default: throw error_location_t{node_loc_t::loc(), params.program_str,
                                            "attempt to use unknown stack state"};
        }

        params.stack_state = old_stack_state;
        memory_table_t::clear_memory();
        return result;
    }

    public:
        node_function_body_t(const location_t& loc, scope_base_t* parent)
        : node_scope_return_t(loc, parent) {}

        execute_t execute(execute_params_t& params) override {
            return process<execute_t>(
                [](auto node, auto& params) { node->execute(params); },
                [](auto node, auto& params) { return node->execute(params); },
                params
            );
        }

        analyze_t analyze(analyze_params_t& params) override {
            if (!return_expr_)
                throw error_type_deduction_t{node_loc_t::loc(), params.program_str,
                                             "missing required return statement"};

            return process<analyze_t>(
                [](auto node, auto& params) { node->analyze(params); },
                [](auto node, auto& params) { return node->analyze(params); },
                params
            );
        }

        node_expression_t* copy(copy_params_t& params, scope_base_t* parent) const override {
            node_function_body_t* body = params.buf->add_node<node_function_body_t>(node_loc_t::loc(), parent);
            return copy_impl<node_function_body_t>(body, params);
        }

        template <typename IterT>
        node_function_body_t* copy_with_args(copy_params_t& params, scope_base_t* parent,
                                                IterT args_begin, IterT args_end) const {
            auto* body_copy = params.buf->add_node<node_function_body_t>(node_loc_t::loc(), parent);
            body_copy->add_variables(args_begin, args_end);
            return copy_impl<node_function_body_t>(body_copy, params);
        }
    };
}