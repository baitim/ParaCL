#pragma once

#include <memory>
#include <string>

namespace node {

    class node_t {
    public:
        virtual ~node_t() {};
    };

    //////////////////////////////////////////////////////

    class node_id_t final : public node_t {
        std::string name_;

    public:
        node_id_t(std::string name) : name_(name) {}
        ~node_id_t() {}
    };

    //////////////////////////////////////////////////////

    class node_number_t final : public node_t {
        int number_;

    public:
        node_number_t(int number) : number_(number) {}
        ~node_number_t() {}
    };

    //////////////////////////////////////////////////////

    enum class binary_operators_e {
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
        ~node_bin_op_t() {
            delete left_;
            delete right_;
        }
    };

    //////////////////////////////////////////////////////

    class node_statement_t final : public node_t {
        node_t* left_;
        node_t* right_;

    public:
        node_statement_t(node_t* left, node_t* right) : left_(left), right_(right) {}
        ~node_statement_t() {
            delete left_;
            delete right_;
        }
    };
}