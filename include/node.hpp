#pragma once

#include <cassert>
#include <iostream>
#include <memory>
#include <exception>
#include <vector>
#include <unordered_map>

namespace node {

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
    
    class node_expression_t : public node_t {
    public:
        virtual int execute() = 0;
    };

    /* ----------------------------------------------------- */
    
    class node_statement_t : public node_t {
    public:
        virtual void execute() = 0;
    };

    /* ----------------------------------------------------- */
    
    class node_instruction_t final : public node_statement_t {
        node_expression_t* expr_ = nullptr;

    public:
        node_instruction_t(node_expression_t* expr) : expr_(expr) {}

        void execute() override {
            assert(expr_);
            expr_->execute();
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
        int value_;

    public:
        node_variable_t(std::string_view id) : node_id_t(id) {}
        int set_value(int value) { return value_ = value; }
        int execute  () override { return value_; }
    };

    /* ----------------------------------------------------- */

    class node_number_t final : public node_expression_t {
        int number_;

    public:
        node_number_t(int number) : number_(number) {}
        int execute() override { return number_; }
    };

    /* ----------------------------------------------------- */

    class node_undef_t final : public node_expression_t {
    public:
        int execute() override { return 0; }
    };

    /* ----------------------------------------------------- */

    class node_assign_t final : public node_expression_t {
        node_variable_t*   lvalue_;
        node_expression_t* rvalue_;

    public:
        node_assign_t(node_variable_t* lvalue, node_expression_t* rvalue)
        : lvalue_(lvalue), rvalue_(rvalue) {}
        int execute() override { return lvalue_->set_value(rvalue_->execute()); }
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

        int execute() override {
            int LHS = left_->execute();
            int RHS = right_->execute();
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
            }
            throw error_t{"attempt to execute unknown binary operator"};
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

        int execute() override {
            int res_exec = node_->execute();
            switch (type_) {
                case unary_operators_e::ADD: return  res_exec;
                case unary_operators_e::SUB: return -res_exec;
                case unary_operators_e::NOT: return !res_exec;
            }
            throw error_t{"attempt to execute unknown unary operator"};
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

        void execute() override {
            for (auto node : statements_)
                node->execute();
        }
    };

    /* ----------------------------------------------------- */

    class node_print_t final : public node_expression_t {
        node_expression_t* argument_;

    public:
        node_print_t(node_expression_t* argument) : argument_(argument) {}

        int execute() override {
            int value = argument_->execute();
            std::cout << value << "\n";
            return value;
        }
    };

    /* ----------------------------------------------------- */

    class node_input_t final : public node_expression_t {
    public:
        int execute() override {
            int value;
            std::cin >> value;
            if (!std::cin.good())
                throw error_t{"invalid input: need integer"};
            return value;
        }
    };

    /* ----------------------------------------------------- */

    class node_loop_t final : public node_statement_t {
        node_expression_t* condition_;
        node_scope_t* body_;

    public:
        node_loop_t(node_expression_t* condition, node_scope_t* body)
        : condition_(condition), body_(body) {}

        void execute() override {
            while (condition_->execute())
                body_->execute();
        }
    };

    /* ----------------------------------------------------- */

    class node_fork_t final : public node_statement_t {
        node_expression_t* condition_;
        node_scope_t* body1_;
        node_scope_t* body2_;

    public:
        node_fork_t(node_expression_t* condition, node_scope_t* body1, node_scope_t* body2)
        : condition_(condition), body1_(body1), body2_(body2) {}

        void execute() override {
            if (condition_->execute())
                body1_->execute();
            else
                body2_->execute();
        }
    };

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
}