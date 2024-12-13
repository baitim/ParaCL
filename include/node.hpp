#pragma once

#include "common.hpp"
#include "environments.hpp"
#include <algorithm>
#include <cassert>
#include <memory>
#include <vector>
#include <unordered_map>

namespace node {
    using namespace environments;

    struct location_t {
        int row;
        int col;
        int len;
    };

    /* ----------------------------------------------------- */

    class error_location_t : public common::error_t {
    private:
        static std::pair<std::string_view, int> get_current_line(const location_t& loc,
                                                                 std::string_view program_str) {
            int line = 0;
            for ([[maybe_unused]]int _ : view::iota(0, loc.row))
                line = program_str.find('\n', line + 1);

            if (line > 0)
                line++;

            int end_of_line = program_str.find('\n', line);
            if (end_of_line == -1)
                end_of_line = program_str.length();

            return std::make_pair(program_str.substr(line, end_of_line - line), loc.col - 2);
        };

        static std::string get_error_line(const location_t& loc_, std::string_view program_str) {
            std::stringstream error_line;

            std::pair<std::string_view, int> line_info = get_current_line(loc_, program_str);
            std::string_view line = line_info.first;
            int length = loc_.len;
            int loc = line_info.second - length + 1;
            const int line_length = line.length();

            error_line << line.substr(0, loc)
                       << print_red(line.substr(loc, length))
                       << line.substr(loc + length, line_length) << "\n";

            for (int i : view::iota(0, line_length)) {
                if (i >= loc && i < loc + length)
                    error_line << print_red("^");
                else
                    error_line << " ";
            }
            error_line << "\n";
            return error_line.str();
        }

    public:
        error_location_t(const location_t& loc, std::string_view program_str, const std::string& msg)
        : common::error_t(get_error_line(loc, program_str) + str_red(msg)) {}
    };

    /* ----------------------------------------------------- */

    class error_execute_t : public error_location_t {
    public:
        error_execute_t(const location_t& loc, std::string_view program_str, const std::string& msg)
        : error_location_t(loc, program_str, str_red("execution failed: " + msg)) {}
    };

    /* ----------------------------------------------------- */

    class error_semantic_analyze_t : public error_location_t {
    public:
        error_semantic_analyze_t(const location_t& loc, std::string_view program_str, const std::string& msg)
        : error_location_t(loc, program_str, str_red("semantic analyze failed: " + msg)) {}
    };

    /* ----------------------------------------------------- */

    class node_loc_t {
    protected:
        location_t loc_;

    public:
        node_loc_t(const location_t& loc) : loc_(loc) {}
        virtual ~node_loc_t() = default;
    };

    /* ----------------------------------------------------- */

    class node_t : virtual public node_loc_t {
    protected:
        using node_loc_t::loc_;

    public:
        virtual void analyze(environments_t& env) = 0;
        virtual ~node_t() = default;
    };

    /* ----------------------------------------------------- */

    class buffer_t final {
        std::vector<std::unique_ptr<node_t>> nodes_;

    public:
        template <typename NodeT, typename ...ArgsT>
        NodeT* add_node(ArgsT&&... args) {
            std::unique_ptr new_node = std::make_unique<NodeT>(std::forward<ArgsT>(args)...);
            nodes_.emplace_back(std::move(new_node));
            return static_cast<NodeT*>(nodes_.back().get());
        }
    };

    /* ----------------------------------------------------- */

    class value_t;

    class node_expression_t : public node_t {
    public:
        virtual value_t execute(buffer_t& buf, environments_t& env) = 0;
    };

    /* ----------------------------------------------------- */

    class node_type_t : public node_expression_t {
    public:
        virtual void print(buffer_t& buf, environments_t& env) = 0;
    };

    /* ----------------------------------------------------- */

    enum class node_type_e {
        NUMBER,
        UNDEF,
        ARRAY
    };

    struct value_t final {
        node_type_e  type;
        node_type_t* value;
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
        node_instruction_t(const location_t& loc, node_expression_t* expr) : node_loc_t(loc), expr_(expr) {}

        void execute(buffer_t& buf, environments_t& env) override {
            assert(expr_);
            expr_->execute(buf, env);
        };

        void analyze(environments_t& env) override {
            expr_->analyze(env);
        }
    };

    /* ----------------------------------------------------- */

    class node_number_t final : public node_type_t {
        int number_;

    public:
        node_number_t(const location_t& loc, int number) : node_loc_t(loc), number_(number) {}

        value_t execute(buffer_t& buf, environments_t& env) override {
            return {node_type_e::NUMBER, this};
        }

        void print(buffer_t& buf, environments_t& env) override { env.os << number_ << "\n"; }

        int get_value() const noexcept { return number_; }

        void analyze(environments_t& env) override {}
    };

    /* ----------------------------------------------------- */

    class node_undef_t final : public node_type_t {
    public:
        node_undef_t(const location_t& loc) : node_loc_t(loc) {}

        value_t execute(buffer_t& buf, environments_t& env) override {
            return {node_type_e::UNDEF, this};
        }

        void print(buffer_t& buf, environments_t& env) override { env.os << "undef\n"; }

        void analyze(environments_t& env) override {}
    };

    /* ----------------------------------------------------- */

    class node_indexes_t final : public node_t {
        using node_loc_t::loc_;
        std::vector<node_expression_t*> indexes_;

    private:
        void dynamic_analyze(node_type_e type, environments_t& env) const {
            if (type != node_type_e::NUMBER)
                throw error_execute_t{loc_, env.program_str, "indexing by not number"};
        }

    public:
        node_indexes_t(const location_t& loc) : node_loc_t(loc) {}

        std::vector<int> execute(buffer_t& buf, environments_t& env) const {
            std::vector<int> indexes;
            const int size = indexes_.size();
            for (int i : view::iota(0, size)) {
                value_t result = indexes_[i]->execute(buf, env);

                if (env.is_analyzing)
                    dynamic_analyze(result.type, env);

                int index = static_cast<node_number_t*>(result.value)->get_value();
                indexes.push_back(index);
            }
            reverse(indexes.begin(), indexes.end());
            return indexes;
        }

        void add_index(node_expression_t* index) { indexes_.push_back(index); }

        void analyze(environments_t& env) override {
            for (auto index : indexes_)
                index->analyze(env);
        }
    };

    /* ----------------------------------------------------- */

    class node_array_values_t {
    public:
        virtual std::vector<value_t> execute(buffer_t& buf, environments_t& env) const = 0;
        virtual void analyze(environments_t& env) = 0;
    };

    /* ----------------------------------------------------- */

    class node_array_value_t : public node_t {
    public:
        virtual void add_value(std::vector<value_t>& values,
                               buffer_t& buf, environments_t& env) const = 0;
    };

    /* ----------------------------------------------------- */

    class node_expression_value_t : public node_array_value_t {
        node_expression_t* value_;

    public:
        node_expression_value_t(const location_t& loc, node_expression_t* value)
        : node_loc_t(loc), value_(value) {}

        void add_value(std::vector<value_t>& values, buffer_t& buf, environments_t& env) const {
            value_t result = value_->execute(buf, env);
            values.push_back(result);
        }

        void analyze(environments_t& env) override {
            value_->analyze(env);
        }
    };

    /* ----------------------------------------------------- */

    class node_repeat_values_t : public node_array_value_t,
                                 public node_array_values_t {
        node_expression_t* value_;
        node_expression_t* count_;

    public:
        node_repeat_values_t(const location_t& loc, node_expression_t* value, node_expression_t* count)
        : node_loc_t(loc), value_(value), count_(count) {}

        void add_value(std::vector<value_t>& values, buffer_t& buf, environments_t& env) const {
            std::vector<value_t> result = execute(buf, env);
            values.insert(values.end(), result.begin(), result.end());
        }

        std::vector<value_t> execute(buffer_t& buf, environments_t& env) const {
            value_t value = value_->execute(buf, env);
            value_t count = count_->execute(buf, env);
            size_t real_count = static_cast<node_number_t*>(count.value)->get_value(); // static check type
            std::vector<value_t> values{real_count, value};
            return values;
        }

        void analyze(environments_t& env) override {
            value_->analyze(env);
            count_->analyze(env);
        }
    };

    /* ----------------------------------------------------- */

    class node_list_values_t : public node_t,
                               public node_array_values_t {
        std::vector<node_array_value_t*> values_;

    public:
        node_list_values_t(const location_t& loc) : node_loc_t(loc) {}

        std::vector<value_t> execute(buffer_t& buf, environments_t& env) const {
            std::vector<value_t> values;
            const int size = values_.size();
            for (int i : view::iota(0, size)) {
                values_[i]->add_value(values, buf, env);
            }
            return values;
        }
        
        void add_value(node_array_value_t* value) { values_.push_back(value); }

        void analyze(environments_t& env) override {
            for (auto value : values_)
                value->analyze(env);
        }
    };

    /* ----------------------------------------------------- */

    class node_array_t final : public node_type_t {
        bool is_inited_ = false;
        node_array_values_t* init_values_;
        node_indexes_t* indexes_;
        std::vector<value_t> values_;
        std::vector<int> real_indexes_;

    private:
        std::string transform_print_str(const std::string& str) const {
            std::string result = str.substr(0, str.rfind('\n'));
            size_t pos = result.find('\n');
            while (pos != std::string::npos) {
                result.replace(pos, 1, ", ");
                pos = result.find('\n', pos + 2);
            }
            return result;
        }

        value_t& shift_(std::vector<int>& indexes, buffer_t& buf, environments_t& env) {
            assert(!indexes.empty());

            int index = indexes.back();
            indexes.pop_back();
            value_t& result = values_[index];

            if (!indexes.empty() && result.type == node_type_e::ARRAY) // + check level error
                return static_cast<node_array_t*>(result.value)->shift_(indexes, buf, env);
            else
                return result;
        }

        void init(buffer_t& buf, environments_t& env) {
            values_       = init_values_->execute(buf, env);
            real_indexes_ = indexes_->execute(buf, env);
            is_inited_    = true;
        }

    public:
        node_array_t(const location_t& loc, node_array_values_t* array_values, node_indexes_t* indexes)
        : node_loc_t(loc), init_values_(array_values), indexes_(indexes) {}

        value_t execute(buffer_t& buf, environments_t& env) override {
            if (!is_inited_)
                init(buf, env);
            
            return {node_type_e::ARRAY, this};
        }

        value_t& shift(const std::vector<int>& ext_indexes, buffer_t& buf, environments_t& env) {
            std::vector<int> final_indexes = ext_indexes;
            final_indexes.insert(final_indexes.end(), real_indexes_.begin(), real_indexes_.end());
            return shift_(final_indexes, buf, env);
        }

        void print(buffer_t& buf, environments_t& env) override {
            if (!real_indexes_.empty()) {
                std::vector<int> tmp_indexes = real_indexes_;
                shift_(tmp_indexes, buf, env).value->print(buf, env);
                return;
            }
            
            std::stringstream print_stream;
            environments_t print_env{print_stream, env.is};
            const int size = values_.size();
            for (int i : view::iota(0, size - 1))
                values_[i].value->print(buf, print_env);
            values_[size - 1].value->print(buf, print_env);

            env.os << "[" << transform_print_str(print_stream.str()) << "]\n";
        }

        void clear() {
            is_inited_ = false;
            values_.clear();
            real_indexes_.clear();
        }

        void analyze(environments_t& env) override {
            init_values_->analyze(env);
            indexes_->analyze(env);
        }
    };

    /* ----------------------------------------------------- */

    class id_t {
        std::string id_;

    public:
        id_t(std::string_view id) : id_(id) {}
        std::string_view get_name() const { return id_; }
        virtual ~id_t() = default;
    };

    /* ----------------------------------------------------- */

    class settable_value_t : public node_t {
        using node_loc_t::loc_;
        bool is_setted = false;
        value_t value_;

    private:
        void dynamic_analyze(int size, environments_t& env) const {
            if (size > 0 && !is_setted)
                throw error_execute_t{loc_, env.program_str, "attempt to indexing by non init variable"};
        }

        value_t& shift(const std::vector<int>& indexes, buffer_t& buf, environments_t& env) {
            if (env.is_analyzing)
                dynamic_analyze(indexes.size(), env);

            if (indexes.size() == 0)
                return value_;

            node_array_t* array = static_cast<node_array_t*>(value_.value);
            return array->shift(indexes, buf, env);
        };

    public:
        value_t execute(const node_indexes_t* indexes, buffer_t& buf, environments_t& env) {
            assert(indexes);
            std::vector<int> real_indexes = indexes->execute(buf, env);
            node_type_t*& real_value = shift(real_indexes, buf, env).value;
            return real_value->execute(buf, env);
        }

        value_t set_value(const node_indexes_t* indexes, value_t new_value,
                          buffer_t& buf, environments_t& env) {
            assert(indexes);
            std::vector<int> real_indexes = indexes->execute(buf, env);
            node_type_t*& real_value = shift(real_indexes, buf, env).value; // +check if types are different
            real_value = new_value.value;
            is_setted = true;
            return value_;
        }

        virtual ~settable_value_t() = default;
    };

    /* ----------------------------------------------------- */

    class node_variable_t final : public id_t,
                                  public settable_value_t {
    public:
        node_variable_t(const location_t& loc, std::string_view id) : node_loc_t(loc), id_t(id) {}
        void analyze(environments_t& env) override {}
    };

    /* ----------------------------------------------------- */

    class node_lvalue_t final : public node_expression_t {
        using node_loc_t::loc_;
        node_variable_t* variable_ = nullptr;
        node_indexes_t*  indexes_;

    public:
        node_lvalue_t(const location_t& loc, node_variable_t* variable, node_indexes_t* indexes)
        : node_loc_t(loc), variable_(variable), indexes_(indexes) {}

        value_t execute(buffer_t& buf, environments_t& env) override {
            return variable_->execute(indexes_, buf, env);
        }

        value_t set_value(value_t new_value, buffer_t& buf, environments_t& env) {
            return variable_->set_value(indexes_, new_value, buf, env);
        }

        void analyze(environments_t& env) override {
            if (!variable_)
                throw error_semantic_analyze_t{loc_, env.program_str,
                                               "declaration error: undeclared variable"};
            indexes_->analyze(env);
        }
    };

    /* ----------------------------------------------------- */

    class node_assign_t final : public node_expression_t {
        using node_loc_t::loc_;
        node_lvalue_t*     lvalue_;
        node_expression_t* rvalue_;

    public:
        node_assign_t(const location_t& loc, node_lvalue_t* lvalue, node_expression_t* rvalue)
        : node_loc_t(loc), lvalue_(lvalue), rvalue_(rvalue) {}
        value_t execute(buffer_t& buf, environments_t& env) override {
            return lvalue_->set_value(rvalue_->execute(buf, env), buf, env);
        }

        void analyze(environments_t& env) override {
            lvalue_->analyze(env);
            rvalue_->analyze(env);
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
        using node_loc_t::loc_;
        binary_operators_e type_;
        node_expression_t* left_;
        node_expression_t* right_;

        void dynamic_analyze(const value_t& l_result, const value_t& r_result, environments_t& env) {
            if (l_result.type != node_type_e::NUMBER &&
                l_result.type != node_type_e::UNDEF)
                throw error_execute_t{loc_, env.program_str,
                                      "attempt to execute binary operator with left not integer type"};

            if (r_result.type != node_type_e::NUMBER &&
                r_result.type != node_type_e::UNDEF)
                throw error_execute_t{loc_, env.program_str,
                                      "attempt to execute binary operator with right not integer type"};
        }

    public:
        node_bin_op_t(const location_t& loc, binary_operators_e type,
                      node_expression_t* left, node_expression_t* right)
        : node_loc_t(loc), type_(type), left_(left), right_(right) {}

        value_t execute(buffer_t& buf, environments_t& env) override {
            value_t l_result = left_ ->execute(buf, env);
            value_t r_result = right_->execute(buf, env);

            if (env.is_analyzing)
                dynamic_analyze(l_result, r_result, env);

            if (l_result.type == node_type_e::UNDEF ||
                r_result.type == node_type_e::UNDEF)
                return {node_type_e::UNDEF, buf.add_node<node_undef_t>(loc_)};
            
            int LHS = static_cast<node_number_t*>(l_result.value)->get_value();
            int RHS = static_cast<node_number_t*>(r_result.value)->get_value();
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
                default: throw error_execute_t{loc_, env.program_str,
                                               "attempt to execute unknown binary operator"};
            }
            return {node_type_e::NUMBER, buf.add_node<node_number_t>(loc_, result)};
        }

        void analyze(environments_t& env) override {
            left_->analyze(env);
            right_->analyze(env);
        }
    };

    /* ----------------------------------------------------- */

    enum class unary_operators_e {
        ADD,
        SUB,
        NOT
    };
    class node_un_op_t final : public node_expression_t {
        using node_loc_t::loc_;
        unary_operators_e  type_;
        node_expression_t* node_;

        void dynamic_analyze(const value_t& result, environments_t& env) {
            if (result.type != node_type_e::NUMBER &&
                result.type != node_type_e::UNDEF)
                throw error_execute_t{loc_, env.program_str,
                                      "attempt to execute unary operator with not integer type"};
        }

    public:
        node_un_op_t(const location_t& loc, unary_operators_e type, node_expression_t* node)
        : node_loc_t(loc), type_(type), node_(node) {}

        value_t execute(buffer_t& buf, environments_t& env) override {
            value_t res_exec = node_->execute(buf, env);

            if (env.is_analyzing)
                dynamic_analyze(res_exec, env);

            if (res_exec.type == node_type_e::UNDEF)
                return {node_type_e::UNDEF, buf.add_node<node_undef_t>(loc_)};

            int value = static_cast<node_number_t*>(res_exec.value)->get_value();
            int result;
            switch (type_) {
                case unary_operators_e::ADD: result =  value; break;
                case unary_operators_e::SUB: result = -value; break;
                case unary_operators_e::NOT: result = !value; break;
                default: throw error_execute_t{loc_, env.program_str,
                                               "attempt to execute unknown unary operator"};
            }
            return {node_type_e::NUMBER, buf.add_node<node_number_t>(loc_, result)};
        }

        void analyze(environments_t& env) override {
            node_->analyze(env);
        }
    };

    /* ----------------------------------------------------- */
    
    class name_table_t {
        std::unordered_map<std::string_view, id_t*> variables_;

    public:
        void add_variable(id_t* node) { variables_.emplace(node->get_name(), node); }

        id_t* get_var_node(std::string_view name) const {
            auto var_iter = variables_.find(name);
            if (var_iter != variables_.end())
                return var_iter->second;
            return nullptr;
        }

        virtual ~name_table_t() = default;
    };

    /* ----------------------------------------------------- */

    class memory_table_t {
        std::vector<node_array_t*> arrays_;

    protected:
        void clear_memory() {
            for (auto iter : arrays_)
                iter->clear();
        }

    public:
        void add_array(node_array_t* node) { arrays_.push_back(node); }
        virtual ~memory_table_t() = default;
    };

    /* ----------------------------------------------------- */

    class node_scope_t final : public node_statement_t,
                               public name_table_t,
                               public memory_table_t {
        node_scope_t* parent_;
        std::vector<node_statement_t*> statements_;

    public:
        node_scope_t(const location_t& loc, node_scope_t* parent) : node_loc_t(loc), parent_(parent) {} 

        void add_statement(node_statement_t* node) { statements_.push_back(node); }

        node_variable_t* get_node(std::string_view name) const {
            for (auto scope = this; scope; scope = scope->parent_) {
                id_t* var_node = scope->get_var_node(name);
                if (var_node)
                    return static_cast<node_variable_t*>(var_node);
            }
            return nullptr;
        }

        void execute(buffer_t& buf, environments_t& env) override {
            for (auto node : statements_)
                node->execute(buf, env);
            memory_table_t::clear_memory();
        }

        void analyze(environments_t& env) override {
            for (auto node : statements_)
                node->analyze(env);
        }
    };

    /* ----------------------------------------------------- */

    class node_print_t final : public node_expression_t {
        node_expression_t* argument_;

    public:
        node_print_t(const location_t& loc, node_expression_t* argument)
        : node_loc_t(loc), argument_(argument) {}

        value_t execute(buffer_t& buf, environments_t& env) override {
            value_t result = argument_->execute(buf, env);
            result.value->print(buf, env);
            return result;
        }

        void analyze(environments_t& env) override {
            argument_->analyze(env);
        }
    };

    /* ----------------------------------------------------- */

    class node_input_t final : public node_expression_t {
        using node_loc_t::loc_;

    public:
        node_input_t(const location_t& loc) : node_loc_t(loc) {}

        value_t execute(buffer_t& buf, environments_t& env) override {
            int value;
            env.is >> value;
            if (!env.is.good())
                throw error_execute_t{loc_, env.program_str, "invalid input: need integer"};
            return {node_type_e::NUMBER, buf.add_node<node_number_t>(loc_, value)};
        }

        void analyze(environments_t& env) override {}
    };

    /* ----------------------------------------------------- */

    class node_loop_t final : public node_statement_t {
        using node_loc_t::loc_;
        node_expression_t* condition_;
        node_scope_t* body_;

    private:
        inline void dynamic_analyze(const value_t& result, environments_t& env) {
            if (result.type != node_type_e::NUMBER)
                throw error_execute_t{loc_, env.program_str, "wrong type of loop condition"};
        }

        inline int step(buffer_t& buf, environments_t& env) {
            value_t result = condition_->execute(buf, env);

            if (env.is_analyzing)
                dynamic_analyze(result, env);

            return static_cast<node_number_t*>(result.value)->get_value();
        }

    public:
        node_loop_t(const location_t& loc, node_expression_t* condition, node_scope_t* body)
        : node_loc_t(loc), condition_(condition), body_(body) {}

        void execute(buffer_t& buf, environments_t& env) override {
            while (step(buf, env))
                body_->execute(buf, env);
        }

        void analyze(environments_t& env) override {
            condition_->analyze(env);
            body_->analyze(env);
        }
    };

    /* ----------------------------------------------------- */

    class node_fork_t final : public node_statement_t {
        using node_loc_t::loc_;
        node_expression_t* condition_;
        node_scope_t* body1_;
        node_scope_t* body2_;

    private:
        inline void dynamic_analyze(const value_t& result, environments_t& env) {
            if (result.type != node_type_e::NUMBER)
                throw error_execute_t{loc_, env.program_str, "wrong type of fork condition"};
        }

    public:
        node_fork_t(const location_t& loc, node_expression_t* condition,
                    node_scope_t* body1, node_scope_t* body2)
        : node_loc_t(loc), condition_(condition), body1_(body1), body2_(body2) {}

        void execute(buffer_t& buf, environments_t& env) override {
            value_t result = condition_->execute(buf, env);

            if (env.is_analyzing)
                dynamic_analyze(result, env);

            int value = static_cast<node_number_t*>(result.value)->get_value();
            if (value)
                body1_->execute(buf, env);
            else
                body2_->execute(buf, env);
        }

        void analyze(environments_t& env) override {
            condition_->analyze(env);
            body1_->analyze(env);
            body2_->analyze(env);
        }
    };
}