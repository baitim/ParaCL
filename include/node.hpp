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

    class node_t {
    public:
        virtual ~node_t() {}
        virtual int execute() = 0;
    };

    /* ----------------------------------------------------- */

    class node_var_t final : public node_t {
        std::string id_;
        int value_;

    public:
        node_var_t(std::string_view id) : id_(id) {}
        int set_value(int value) { return value_ = value; }
        int execute  ()          { return value_; }
        std::string_view get_name() const { return id_; }
    };

    /* ----------------------------------------------------- */

    class node_number_t final : public node_t {
        int number_;

    public:
        node_number_t(int number) : number_(number) {}
        int execute() { return number_; }
    };

    /* ----------------------------------------------------- */

    class node_assign_t final : public node_t {
        node_var_t* lvalue_;
        node_t*     rvalue_;

    public:
        node_assign_t(node_var_t* lvalue, node_t* rvalue) : lvalue_(lvalue), rvalue_(rvalue) {}
        int execute() { return lvalue_->set_value(rvalue_->execute()); }
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
    class node_bin_op_t final : public node_t {
        binary_operators_e type_;
        node_t* left_;
        node_t* right_;

    public:
        node_bin_op_t(binary_operators_e type, node_t* left, node_t* right)
        : type_(type), left_(left), right_(right) {}

        int execute() {
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
    class node_un_op_t final : public node_t {
        unary_operators_e type_;
        node_t* node_;

    public:
        node_un_op_t(unary_operators_e type, node_t* node)
        : type_(type), node_(node) {}

        int execute() {
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

    class node_scope_t final : public node_t {
        std::vector<node_t*> statements_;
        node_scope_t* parent_;

        using vars_container = std::unordered_map<std::string_view, node_var_t*>;
        vars_container variables_;

    public:
        node_scope_t(node_scope_t* parent) : parent_(parent) {}
        void add_statement(node_t* node) { statements_.push_back(node); }
        void add_variable (node_var_t* node) { variables_.emplace(node->get_name(), node); }

        bool contains(std::string_view name) const {
            for (auto scope = this; scope; scope = scope->parent_) {
                auto& scope_vars = scope->get_variables();
                if (scope_vars.find(name) != scope_vars.end())
                    return true;
            }
            return false;
        }

        node_var_t* get_node(std::string_view name) const {
            assert(contains(name));
            for (auto scope = this; scope; scope = scope->parent_) {
                auto& scope_vars = scope->get_variables();
                auto name_iter = scope_vars.find(name);
                if (name_iter != scope_vars.end())
                    return name_iter->second;
            }
            throw error_t{"attempt to access node, which is not exist, but node_scope_t contains"};
        }

        const vars_container& get_variables() const { return variables_; }

        int execute() {
            int result;
            for (auto node : statements_)
                result = node->execute();
            return result;
        }
    };

    /* ----------------------------------------------------- */

    class node_print_t final : public node_t {
        node_t* argument_;

    public:
        node_print_t(node_t* argument) : argument_(argument) {}

        int execute() {
            int value = argument_->execute();
            std::cout << value << "\n";
            return value;
        }
    };

    /* ----------------------------------------------------- */

    class node_input_t final : public node_t {
    public:
        int execute() {
            int value;
            std::cin >> value;
            if (!std::cin.good())
                throw error_t{"invalid input: need integer"};
            return value;
        }
    };

    /* ----------------------------------------------------- */

    class node_loop_t final : public node_t {
        node_t* condition_;
        node_t* body_;

    public:
        node_loop_t(node_t* condition, node_t* body) : condition_(condition), body_(body) {}

        int execute() {
            while (condition_->execute()) {
                if (body_)
                    body_->execute();
            }
            return 0;
        }
    };

    /* ----------------------------------------------------- */

    class node_fork_t final : public node_t {
        node_t* condition_;
        node_t* body1_;
        node_t* body2_;

    public:
        node_fork_t(node_t* condition, node_t* body1, node_t* body2)
        : condition_(condition), body1_(body1), body2_(body2) {}

        int execute() {
            if (condition_->execute()) {
                if(body1_)
                    body1_->execute();
            } else {
                if(body2_)
                    body2_->execute();
            }
            return 0;
        }
    };

    class buffer_t final {
        std::vector<std::unique_ptr<node::node_t>> nodes_;

    public:
        template <typename NodeT>
        NodeT* add_node(const NodeT& node) {
            return static_cast<NodeT*>((nodes_.emplace_back(std::make_unique<NodeT>(node))).get());
        }
    };
}