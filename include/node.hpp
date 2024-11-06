#pragma once

#include <iostream>
#include <memory>
#include <exception>
#include <list>
#include <unordered_map>

namespace node {

    struct error_t final {
        std::string msg_;
        error_t(const char*        msg) : msg_(msg) {}
        error_t(const std::string& msg) : msg_(msg) {}
    };

    enum class node_type_e {
        ID,
        NUMBER,
        BIN_OP,
        SCOPE,
        PRINT,
        INPUT,
        LOOP,
        FORK
    };
    class node_t {
    public:
        virtual ~node_t() {}
        virtual int set_value(int value)     { throw error_t{"attempt to set value to base node"}; }
        virtual int execute()                { throw error_t{"attempt to execute base node"}; }
        virtual node_type_e get_type() const { throw error_t{"attempt to get type of base node"}; }
    };

    /* ----------------------------------------------------- */

    class node_id_t final : public node_t {
        int value_;

    public:
        int set_value(int value) { return value_ = value; }
        int execute  ()          { return value_; }

        node_type_e get_type() const { return node_type_e::ID; }
    };

    /* ----------------------------------------------------- */

    class node_number_t final : public node_t {
        int number_;

    public:
        node_number_t(int number) : number_(number) {}
        int execute() { return number_; }
        node_type_e get_type() const { return node_type_e::NUMBER; }
    };

    /* ----------------------------------------------------- */

    enum class binary_operators_e {
        EQ,
        NE,
        LE,
        GE,
        LT,
        GT,

        ASSIGN,
        ADD,
        SUB,
        MUL,
        DIV
    };
    class node_bin_op_t final : public node_t {
        binary_operators_e type_;
        node_t* left_;
        node_t* right_;

    public:
        node_bin_op_t(binary_operators_e type, node_t* left, node_t* right)
        : type_(type), left_(left), right_(right) {}

        int execute() {
            switch (type_) {
                case binary_operators_e::EQ: return left_->execute() == right_->execute(); 
                case binary_operators_e::NE: return left_->execute() != right_->execute(); 
                case binary_operators_e::LE: return left_->execute() <= right_->execute(); 
                case binary_operators_e::GE: return left_->execute() >= right_->execute(); 
                case binary_operators_e::LT: return left_->execute() <  right_->execute(); 
                case binary_operators_e::GT: return left_->execute() >  right_->execute(); 

                case binary_operators_e::ASSIGN: { return left_->set_value(right_->execute()); }

                case binary_operators_e::ADD: return left_->execute() + right_->execute(); 
                case binary_operators_e::SUB: return left_->execute() - right_->execute(); 
                case binary_operators_e::MUL: return left_->execute() * right_->execute(); 
                case binary_operators_e::DIV: return left_->execute() / right_->execute(); 
            }
            throw error_t{"attempt to execute unknown binary operator"};
        }

        node_type_e get_type() const { return node_type_e::BIN_OP; }
    };

    /* ----------------------------------------------------- */

    class node_scope_t final : public node_t {
        std::list<node_t*> statements_;
        node_scope_t* parent_;

        using vars_container = std::unordered_map<std::string, node_t*>;
        vars_container variables_;

    public:
        node_scope_t(node_scope_t* parent) : parent_(parent) {}
        void    add_statement(node_t* node) { statements_.push_back(node); }
        void    add_variable (const std::string& name, node_t* node) { variables_.emplace(name, node); }
        bool    find_variable(const std::string& name) { return variables_.find(name) != variables_.end(); }
        node_t* get_node     (const std::string& name) { return variables_.find(name)->second; }

        const vars_container& get_variables() { return variables_; }
        void copy_variables(const vars_container& variables) { variables_ = variables;  }

        int execute() {
            int result;
            for (auto node : statements_)
                result = node->execute();
            return result;
        }

        node_type_e get_type() const { return node_type_e::SCOPE; }
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

        node_type_e get_type() const { return node_type_e::PRINT; }
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

        node_type_e get_type() const { return node_type_e::INPUT; }
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

        node_type_e get_type() const { return node_type_e::LOOP; }
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

        node_type_e get_type() const { return node_type_e::FORK; }
    };

    struct buffer_t final {
        std::list<std::unique_ptr<node::node_t>> nodes_;

        template <typename NodeT>
        NodeT* add_node(NodeT node) {
            return static_cast<NodeT*>((nodes_.emplace_back(std::make_unique<NodeT>(node))).get());
        }
    };
}