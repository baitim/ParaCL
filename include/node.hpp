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

    enum class node_type_e {
        ID,
        NUMBER,
        BIN_OP,
        UN_OP,
        SCOPE,
        PRINT,
        INPUT,
        LOOP,
        FORK
    };
    class node_t {
    public:
        virtual ~node_t() {}
        virtual int set_value(int value) = 0;
        virtual int execute()            = 0;
    };

    /* ----------------------------------------------------- */

    class node_lvalue_t final : public node_t {
        std::string id_;
        int value_;

    public:
        node_lvalue_t(std::string_view id) : id_(id) {}
        int set_value(int value) { return value_ = value; }
        int execute  ()          { return value_; }
    };

    /* ----------------------------------------------------- */

    class node_number_t final : public node_t {
        int number_;

    public:
        node_number_t(int number) : number_(number) {}
        int set_value(int value) { throw error_t{"attempt to set value for node_number_t"}; }
        int execute() { return number_; }
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

        ASSIGN,
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
        int set_value(int value) { throw error_t{"attempt to set value for node_bin_op_t"}; }

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

                case binary_operators_e::ASSIGN: { return left_->set_value(RHS); }

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
        int set_value(int value) { throw error_t{"attempt to set value for node_un_op_t"}; }

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

        using vars_container = std::unordered_map<std::string, node_t*>;
        vars_container variables_;

    public:
        node_scope_t(node_scope_t* parent) : parent_(parent) {}
        void    add_statement(node_t* node) { statements_.push_back(node); }
        void    add_variable (std::string_view name, node_t* node) { variables_.emplace(name, node); }
        bool    contains     (const std::string& name) { return variables_.find(name) != variables_.end(); }
        node_t* get_node     (const std::string& name) {
            assert(contains(name));
            return variables_.find(name)->second;
        }

        const vars_container& get_variables() { return variables_; }
        void copy_variables(const vars_container& variables) { variables_ = variables;  }

        int set_value(int value) { throw error_t{"attempt to set value for node_scope_t"}; }

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
        int set_value(int value) { throw error_t{"attempt to set value for node_print_t"}; }

        int execute() {
            int value = argument_->execute();
            std::cout << value << "\n";
            return value;
        }
    };

    /* ----------------------------------------------------- */

    class node_input_t final : public node_t {
    public:
        int set_value(int value) { throw error_t{"attempt to set value for node_input_t"}; }
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
        int set_value(int value) { throw error_t{"attempt to set value for node_loop_t"}; }

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
        int set_value(int value) { throw error_t{"attempt to set value for node_fork_t"}; }

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