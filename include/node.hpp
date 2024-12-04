#pragma once

#include <cassert>
#include <iostream>
#include <memory>
#include <exception>
#include <vector>
#include <unordered_map>

#ifndef DYNAMIC_ANALYZE
#define DYNAMIC_ANALYZE 0
#endif

#define dynamic_analize_d(args)     \
        if (!DYNAMIC_ANALYZE) {     \
        } else                      \
            dynamic_analize(args)   \

namespace node {

    struct environments_t {
        std::ostream& os;
        std::istream& is;
    };

    class error_t final : std::exception {
        std::string msg_;
    
    public:
        error_t(const char*      msg) : msg_(msg) {}
        error_t(std::string_view msg) : msg_(msg) {}
        const char* what() const noexcept { return msg_.c_str(); }
    };

    /* ----------------------------------------------------- */
    
    class node_t {
    public:
        virtual ~node_t() = 0;
    };
    inline node_t::~node_t() {};

    /* ----------------------------------------------------- */

    class buffer_t final {
        std::vector<std::unique_ptr<node::node_t>> nodes_;

    public:
        template <typename NodeT, typename ...ArgsT>
        NodeT* add_node(ArgsT&&... args) {
            return static_cast<NodeT*>(
                (nodes_.emplace_back(std::make_unique<NodeT>(std::forward<ArgsT>(args)...))).get()
            );
        }
    };

    /* ----------------------------------------------------- */

    class result_t;
    
    class node_expression_t : public node_t {
    public:
        virtual result_t execute(buffer_t& buf, environments_t& env) = 0;
    };

    /* ----------------------------------------------------- */

    class node_type_t : public node_expression_t {
    public:
        virtual void print(std::ostream& os) const = 0;
    };

    /* ----------------------------------------------------- */

    enum class node_type_e {
        NUMBER,
        ARRAY,
        UNDEF
    };

    struct result_t final {
        node_type_e type;
        const node_type_t* value;
    };

    /* ----------------------------------------------------- */
    
    class node_statement_t : public node_t {
    public:
        virtual void execute(buffer_t& buf, environments_t& env) = 0;
    };

    /* ----------------------------------------------------- */
    
    class node_instruction_t final : public node_statement_t {
        node_expression_t* expr_ = nullptr;

    public:
        node_instruction_t(node_expression_t* expr) : expr_(expr) {}

        void execute(buffer_t& buf, environments_t& env) override {
            assert(expr_);
            expr_->execute(buf, env);
        };
    };

    /* ----------------------------------------------------- */

    class node_id_t {
        std::string id_;

    public:
        node_id_t(std::string_view id) : id_(id) {}
        std::string_view get_name() const { return id_; }
    };

    /* ----------------------------------------------------- */

    class node_variable_t final : public node_id_t, public node_expression_t {
        result_t value_;

    public:
        node_variable_t(std::string_view id) : node_id_t(id) {}
        result_t set_value(result_t value) { return value_ = value; }
        result_t execute  (buffer_t& buf, environments_t& env) override { return value_; }
    };

    /* ----------------------------------------------------- */

    class node_number_t final : public node_type_t {
        int number_;

    public:
        node_number_t(int number) : number_(number) {}
        result_t execute(buffer_t& buf, environments_t& env) override { return {node_type_e::NUMBER, this}; }
        void print(std::ostream& os) const { os << number_ << "\n"; }
        int get_value() const noexcept { return number_; }
    };

    /* ----------------------------------------------------- */

    class node_undef_t final : public node_type_t {
    public:
        result_t execute(buffer_t& buf, environments_t& env) override { return {node_type_e::UNDEF, this}; }
        void print(std::ostream& os) const { os << "undef\n"; }
    };

    /* ----------------------------------------------------- */

    class node_assign_t final : public node_expression_t {
        node_variable_t*   lvalue_;
        node_expression_t* rvalue_;

    public:
        node_assign_t(node_variable_t* lvalue, node_expression_t* rvalue)
        : lvalue_(lvalue), rvalue_(rvalue) {}
        result_t execute(buffer_t& buf, environments_t& env) override {
            return lvalue_->set_value(rvalue_->execute(buf, env));
        }
    };

    /* ----------------------------------------------------- */

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

    public:
        node_bin_op_t(binary_operators_e type, node_expression_t* left, node_expression_t* right)
        : type_(type), left_(left), right_(right) {}

        result_t execute(buffer_t& buf, environments_t& env) override {
            result_t l_result = left_ ->execute(buf, env);
            result_t r_result = right_->execute(buf, env);

            if (l_result.type == node_type_e::UNDEF ||
                r_result.type == node_type_e::UNDEF)
                return {node_type_e::UNDEF, buf.add_node<node_undef_t>()};
            
            int LHS = static_cast<const node_number_t*>(l_result.value)->get_value();
            int RHS = static_cast<const node_number_t*>(r_result.value)->get_value();
            int result;
            switch (type_) {
                case binary_operators_e::EQ: result = LHS == RHS; break;
                case binary_operators_e::NE: result = LHS != RHS; break;
                case binary_operators_e::LE: result = LHS <= RHS; break;
                case binary_operators_e::GE: result = LHS >= RHS; break;
                case binary_operators_e::LT: result = LHS <  RHS; break;
                case binary_operators_e::GT: result = LHS >  RHS; break;

                case binary_operators_e::OR:  result = LHS || RHS; break;
                case binary_operators_e::AND: result = LHS && RHS; break;

                case binary_operators_e::ADD: result = LHS + RHS; break;
                case binary_operators_e::SUB: result = LHS - RHS; break;
                case binary_operators_e::MUL: result = LHS * RHS; break;
                case binary_operators_e::DIV: result = LHS / RHS; break;
                case binary_operators_e::MOD: result = LHS % RHS; break;
                default: throw error_t{"attempt to execute unknown binary operator"};
            }
            return {node_type_e::NUMBER, buf.add_node<node_number_t>(result)};
        }

        void stat_analize(const result_t& lhs, const result_t& rhs) {
            node_type_e l_type = lhs.type;
            node_type_e r_type = rhs.type;

            if (!(l_type == node_type_e::UNDEF  || r_type == node_type_e::UNDEF) &&
                !(l_type == node_type_e::NUMBER && r_type == node_type_e::NUMBER))
                throw error_t{"different types of arguments in binary operator"};
        }
    };

    /* ----------------------------------------------------- */

    enum class unary_operators_e {
        ADD,
        SUB,
        NOT
    };
    class node_un_op_t final : public node_expression_t {
        unary_operators_e  type_;
        node_expression_t* node_;

    public:
        node_un_op_t(unary_operators_e type, node_expression_t* node)
        : type_(type), node_(node) {}

        result_t execute(buffer_t& buf, environments_t& env) override {
            result_t res_exec = node_->execute(buf, env);
            if (res_exec.type == node_type_e::UNDEF)
                return {node_type_e::UNDEF, buf.add_node<node_undef_t>()};

            int value = static_cast<const node_number_t*>(res_exec.value)->get_value();
            int result;
            switch (type_) {
                case unary_operators_e::ADD: result =  value; break;
                case unary_operators_e::SUB: result = -value; break;
                case unary_operators_e::NOT: result = !value; break;
                default: throw error_t{"attempt to execute unknown unary operator"};
            }
            return {node_type_e::NUMBER, buf.add_node<node_number_t>(result)};
        }

        void stat_analize(const result_t& argument) {
            if (argument.type != node_type_e::NUMBER ||
                argument.type != node_type_e::UNDEF)
                throw error_t{"wrong type in unary operator"};
        }
    };

    /* ----------------------------------------------------- */
    
    class name_table_t {
    protected:
        using vars_container = std::unordered_map<std::string_view, node_id_t*>;
        vars_container variables_;

    public:
        void add_variable(node_id_t* node) { variables_.emplace(node->get_name(), node); }

        node_id_t* get_var_node(std::string_view name) const {
            auto var_iter = variables_.find(name);
            if (var_iter != variables_.end())
                return var_iter->second;
            return nullptr;
        }

        virtual ~name_table_t() {};
    };

    /* ----------------------------------------------------- */

    class node_scope_t final : public node_statement_t, public name_table_t {
        node_scope_t* parent_;
        std::vector<node_statement_t*> statements_;

    public:
        node_scope_t(node_scope_t* parent) : parent_(parent) {} 
        void add_statement(node_statement_t* node) { statements_.push_back(node); }

        node_variable_t* get_node(std::string_view name) const {
            for (auto scope = this; scope; scope = scope->parent_) {
                node_id_t* var_node = scope->get_var_node(name);
                if (var_node)
                    return static_cast<node_variable_t*>(var_node);
            }
            return nullptr;
        }

        void execute(buffer_t& buf, environments_t& env) override {
            for (auto node : statements_)
                node->execute(buf, env);
        }
    };

    /* ----------------------------------------------------- */

    class node_print_t final : public node_expression_t {
        node_expression_t* argument_;

    public:
        node_print_t(node_expression_t* argument) : argument_(argument) {}

        result_t execute(buffer_t& buf, environments_t& env) override {
            result_t result = argument_->execute(buf, env);
            result.value->print(env.os);
            return result;
        }
    };

    /* ----------------------------------------------------- */

    class node_input_t final : public node_expression_t {
    public:
        result_t execute(buffer_t& buf, environments_t& env) override {
            int value;
            env.is >> value;
            if (!env.is.good())
                throw error_t{"invalid input: need integer"};
            return {node_type_e::NUMBER, buf.add_node<node_number_t>(value)};
        }
    };

    /* ----------------------------------------------------- */

    class node_loop_t final : public node_statement_t {
        node_expression_t* condition_;
        node_scope_t* body_;

    private:
        inline void dynamic_analize(const result_t& result) {
            if (result.type != node_type_e::NUMBER)
                throw error_t{"wrong type of loop condition"};
        }

        inline int step(buffer_t& buf, environments_t& env) {
            result_t result = condition_->execute(buf, env);

            dynamic_analize_d(result);

            return static_cast<const node_number_t*>(result.value)->get_value();
        }

    public:
        node_loop_t(node_expression_t* condition, node_scope_t* body)
        : condition_(condition), body_(body) {}

        void execute(buffer_t& buf, environments_t& env) override {
            while (step(buf, env))
                body_->execute(buf, env);
        }
    };

    /* ----------------------------------------------------- */

    class node_fork_t final : public node_statement_t {
        node_expression_t* condition_;
        node_scope_t* body1_;
        node_scope_t* body2_;

    private:
        inline void dynamic_analize(const result_t& result) {
            if (result.type != node_type_e::NUMBER)
                throw error_t{"wrong type of fork condition"};
        }

    public:
        node_fork_t(node_expression_t* condition, node_scope_t* body1, node_scope_t* body2)
        : condition_(condition), body1_(body1), body2_(body2) {}

        void execute(buffer_t& buf, environments_t& env) override {
            result_t result = condition_->execute(buf, env);

            dynamic_analize_d(result);

            int value = static_cast<const node_number_t*>(result.value)->get_value();
            if (value)
                body1_->execute(buf, env);
            else
                body2_->execute(buf, env);
        }
    };
}