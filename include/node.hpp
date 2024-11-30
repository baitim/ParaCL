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

    template <typename VarT>
    class node_array_t;

    template <typename VarT>
    concept paracl_types = std::is_same_v<VarT, int> ||
                           std::is_same_v<VarT, node_array_t>;

    template <paracl_types VarT>
    class node_expression_t : public node_t {
    public:
        virtual VarT execute() = 0;
    };

    /* ----------------------------------------------------- */
    
    class node_statement_t : public node_t {
    public:
        virtual void execute() = 0;
    };

    /* ----------------------------------------------------- */
    
    template <paracl_types VarT>
    class node_instruction_t final : public node_statement_t {
        node_expression_t<VarT>* expr_ = nullptr;

    public:
        node_instruction_t(node_expression_t<VarT>* expr) : expr_(expr) {}

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

    template <paracl_types VarT>
    class node_variable_t final : public node_id_t, public node_expression_t<VarT> {
        VarT value_;

    public:
        node_variable_t(std::string_view id) : node_id_t(id) {}
        VarT set_value(const VarT& value) { return value_ = value; };
        VarT execute() override { return value_; }

        int get_depth() const noexcept {
            if (std::is_same_v<VarT, int>)
                return 0;
            else
                value_.get_depth();
        };
    };

    /* ----------------------------------------------------- */

    class node_number_t final : public node_expression_t<int> {
        int value_;

    public:
        node_number_t(int value) : value_(value) {}
        int execute() override { return value_; }
    };

    /* ----------------------------------------------------- */

    class node_undef_t final : public node_expression_t<int> {
    public:
        int execute() override { return 0; }
    };

    /* ----------------------------------------------------- */

    template <paracl_types VarT>
    class node_array_values_t : public node_t {
        int depth_ = -1;
    
    protected:
        std::vector<node_expression_t<VarT>*> values_;

    private:
        void update_depth(node_expression_t<VarT>* value) {
            int var_depth = value.get_depth();
            if (depth_ == -1)
                depth_ = var_depth;
            else if (depth_ != var_depth)
                ; //throw "parse error";
        }

    protected:
        int get_depth() const noexcept { return depth_ + 1; }

    public:
        void add_value(node_expression_t<VarT>* value) {
            update_depth(value);
            values_.push_back(value);
        }
    };

    /* ----------------------------------------------------- */
    
    class node_indexes_list_t : public node_t {
    protected:
        std::vector<node_expression_t<int>*> indexes_;

    public:
        void add_expr(node_expression_t<int>* index) { indexes_.push_back(index); }
    };

    /* ----------------------------------------------------- */

    template <typename VarT>
    class node_array_t final : public node_array_values_t<VarT>,
                               public node_indexes_list_t,
                               public node_expression_t<node_array_t<VarT>> {
        static_assert(paracl_types<VarT>, "array must consist of paracl type objects");

        using node_array_values_t<VarT>::values_;
        using node_indexes_list_t::indexes_;

    public:
        node_array_t(const node_array_values_t<VarT>& value) : node_array_values_t<VarT>(value) {}

        node_array_t(const node_array_values_t<VarT>& value,
                     const node_indexes_list_t& indexes)
        : node_array_values_t<VarT>(value), node_indexes_list_t(indexes) {}

        paracl_types& execute() override { return 1; }

        int get_depth() const noexcept { return values_.get_depth(); }
    };

    /* ----------------------------------------------------- */

    template <paracl_types VarT>
    class node_assign_t final : public node_expression_t<VarT> {
        node_variable_t  <VarT>* lvalue_;
        node_expression_t<VarT>* rvalue_;

    public:
        node_assign_t(node_variable_t<VarT>* lvalue, node_expression_t<VarT>* rvalue)
        : lvalue_(lvalue), rvalue_(rvalue) {}
        VarT execute() override { return lvalue_->set_value(rvalue_->execute()); }
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
    class node_bin_op_t final : public node_expression_t<int> {
        binary_operators_e      type_;
        node_expression_t<int>* left_;
        node_expression_t<int>* right_;

    public:
        node_bin_op_t(binary_operators_e type, node_expression_t<int>* left, node_expression_t<int>* right)
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
    class node_un_op_t final : public node_expression_t<int> {
        unary_operators_e       type_;
        node_expression_t<int>* node_;

    public:
        node_un_op_t(unary_operators_e type, node_expression_t<int>* node)
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

        template <paracl_types VarT>
        node_variable_t<VarT>* get_node(std::string_view name) const {
            for (auto scope = this; scope; scope = scope->parent_) {
                node_id_t* var_node = scope->get_var_node(name);
                if (var_node)
                    return static_cast<node_variable_t<VarT>*>(var_node);
            }
            return nullptr;
        }

        void execute() override {
            for (auto node : statements_)
                node->execute();
        }
    };

    /* ----------------------------------------------------- */

    class node_print_t final : public node_expression_t<int> {
        node_expression_t<int>* argument_;

    public:
        node_print_t(node_expression_t<int>* argument) : argument_(argument) {}

        int execute() override {
            int value = argument_->execute();
            std::cout << value << "\n";
            return value;
        }
    };

    /* ----------------------------------------------------- */

    class node_input_t final : public node_expression_t<int> {
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
        node_expression_t<int>* condition_;
        node_scope_t* body_;

    public:
        node_loop_t(node_expression_t<int>* condition, node_scope_t* body)
        : condition_(condition), body_(body) {}

        void execute() override {
            while (condition_->execute())
                body_->execute();
        }
    };

    /* ----------------------------------------------------- */

    class node_fork_t final : public node_statement_t {
        node_expression_t<int>* condition_;
        node_scope_t* body1_;
        node_scope_t* body2_;

    public:
        node_fork_t(node_expression_t<int>* condition, node_scope_t* body1, node_scope_t* body2)
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