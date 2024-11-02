#pragma once

#include <iostream>
#include <exception>
#include <memory>
#include <list>
#include <unordered_map>
#include <string>

namespace node {

    class node_t {
    public:
        virtual ~node_t() {}
        virtual int set_value(int value) { throw "attempt to set value to base node"; }
        virtual int execute()            { throw "attempt to execute base node"; }
    };

    /* ----------------------------------------------------- */

    class node_id_t final : public node_t {
        int value_;

    public:
        int set_value(int value) { return value_ = value; }
        int execute()            { return value_; }
    };

    /* ----------------------------------------------------- */

    class node_number_t final : public node_t {
        int number_;

    public:
        node_number_t(int number) : number_(number) {}
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
            throw "attempt to execute unknown binary operator";
        }
    };

    /* ----------------------------------------------------- */

    class node_scope_t final : public node_t {
        std::list<node_t*> statements_;

        using vars_container = std::unordered_map<std::string, node_t*>;
        vars_container variables_;

    public:
        void    add_statement(node_t* node) { statements_.push_back(node); }
        void    add_variable (std::string name, node_t* node) { variables_.emplace(name, node); }
        bool    find_variable(std::string name) { return variables_.find(name) != variables_.end(); }
        node_t* get_variable (std::string name) { return variables_.find(name)->second; }

        const vars_container& get_variables() { return variables_; }
        void copy_variables(const vars_container& variables) { variables_ = variables;  }

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
                throw "invalid input: need integer";
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
}