#pragma once

#include <iostream>
#include <memory>
#include <list>
#include <unordered_map>
#include <string>

namespace node {

    class node_t {
    public:
        virtual ~node_t() {}
        virtual void set_value(int value) {}
        virtual int  get_value() { return 0; }
        virtual int  calculate() { return 0; }
        virtual void print()     {}
    };

    //////////////////////////////////////////////////////

    class node_id_t final : public node_t {
        int value_;

    public:
        void set_value(int value) { value_ = value; }
        int  get_value()          { return value_; }
    };

    //////////////////////////////////////////////////////

    class node_number_t final : public node_t {
        int number_;

    public:
        node_number_t(int number) : number_(number) {}
        int calculate() { return number_; }
    };

    //////////////////////////////////////////////////////

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

        int calculate() {
            switch (type_) {
                case binary_operators_e::EQ: return left_->calculate() == right_->calculate(); 
                case binary_operators_e::NE: return left_->calculate() != right_->calculate(); 
                case binary_operators_e::LE: return left_->calculate() <= right_->calculate(); 
                case binary_operators_e::GE: return left_->calculate() >= right_->calculate(); 
                case binary_operators_e::LT: return left_->calculate() <  right_->calculate(); 
                case binary_operators_e::GT: return left_->calculate() >  right_->calculate(); 

                case binary_operators_e::ASSIGN: {
                    int value = right_->calculate();
                    left_->set_value(value);
                    return value;
                }

                case binary_operators_e::ADD: return left_->calculate() + right_->calculate(); 
                case binary_operators_e::SUB: return left_->calculate() - right_->calculate(); 
                case binary_operators_e::MUL: return left_->calculate() * right_->calculate(); 
                case binary_operators_e::DIV: return left_->calculate() / right_->calculate(); 
            }
            return 0;
        }

        int get_value() { return calculate(); }
    };

    //////////////////////////////////////////////////////

    class node_scope_t final : public node_t {
        std::list<node_t*> statements_;
        std::unordered_map<std::string, node_t*> variables_;

    public:
        void    add_statement(node_t* node) { statements_.push_back(node); }
        void    add_variable (std::string name, node_t* node) { variables_.emplace(name, node); }
        bool    find_variable(std::string name) { return variables_.find(name) != variables_.end(); }
        node_t* get_variable (std::string name) { return variables_.find(name)->second; }

        int calculate() {
            for (auto node : statements_)
                node->calculate();
            return 0;
        }

        void print() {
            for (auto node : statements_)
                node->print();
        }
    };

    //////////////////////////////////////////////////////

    class node_print_t final : public node_t {
        node_t* body_;

    public:
        node_print_t(node_t* body) : body_(body) {}

        void print() {
            std::cout << body_->get_value() << "\n";
        }
    };
}