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

    class error_analyze_t : public error_location_t {
    public:
        error_analyze_t(const location_t& loc, std::string_view program_str, const std::string& msg)
        : error_location_t(loc, program_str, str_red("semantic analyze failed: " + msg)) {}
    };

    /* ----------------------------------------------------- */

    class node_t {
    public:
        virtual ~node_t() = default;
    };

    /* ----------------------------------------------------- */

    class node_loc_t {
        location_t loc_;

    protected:
        void set_loc(const location_t& loc) { loc_ = loc; }

    public:
        node_loc_t() {}
        node_loc_t(const location_t& loc) : loc_(loc) {}
        location_t loc() const { return loc_; }
        virtual ~node_loc_t() = default;
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
    class node_scope_t;

    class node_expression_t : public node_t,
                              virtual public node_loc_t {
    public:
        virtual value_t execute(buffer_t& buf, environments_t& env) = 0;
        virtual value_t analyze(buffer_t& buf, environments_t& env) = 0;
        virtual node_expression_t* copy(buffer_t& buf, node_scope_t* parent) const = 0;
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
        ARRAY,
        INPUT
    };

    inline std::string type2str(node_type_e type) {
        switch (type) {
            case node_type_e::NUMBER: return "number";
            case node_type_e::UNDEF:  return "undef";
            case node_type_e::ARRAY:  return "array";
            case node_type_e::INPUT:  return "input";
            default:                  return "unknown type";
        }
    }

    struct value_t final {
        node_type_e  type;
        node_type_t* value;
    };

    /* ----------------------------------------------------- */

    inline void expect_types_ne(node_type_e result,    node_type_e expected,
                                const location_t& loc, environments_t& env) {
        if (result == expected)
            throw error_analyze_t{loc, env.program_str, "wrong type: " + type2str(result)};
    }

    /* ----------------------------------------------------- */

    class node_statement_t : public node_t,
                             virtual public node_loc_t {
    public:
        virtual void execute(buffer_t& buf, environments_t& env) = 0;
        virtual void analyze(buffer_t& buf, environments_t& env) = 0;
        virtual node_statement_t* copy(buffer_t& buf, node_scope_t* parent) const = 0;
    };

    /* ----------------------------------------------------- */
    
    class node_instruction_t final : public node_statement_t {
        node_expression_t* expr_ = nullptr;

    public:
        node_instruction_t(const location_t& loc, node_expression_t* expr) : node_loc_t(loc), expr_(expr) {}

        void execute(buffer_t& buf, environments_t& env) override {
            assert(expr_);
            expr_->execute(buf, env);
        }

        void analyze(buffer_t& buf, environments_t& env) override {
            expr_->analyze(buf, env);
        }

        node_statement_t* copy(buffer_t& buf, node_scope_t* parent) const override {
            return buf.add_node<node_instruction_t>(node_loc_t::loc(), expr_->copy(buf, parent));
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

    class node_memory_t {
    public:
        virtual void clear() = 0;
        virtual ~node_memory_t() = default;
    };

    /* ----------------------------------------------------- */

    class memory_table_t {
        std::vector<node_memory_t*> arrays_;

    protected:
        void clear_memory() {
            for (auto iter : arrays_)
                iter->clear();
        }

    public:
        void add_array(node_memory_t* node) { arrays_.push_back(node); }
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

        id_t* get_node(std::string_view name) const {
            for (auto scope = this; scope; scope = scope->parent_) {
                id_t* var_node = scope->get_var_node(name);
                if (var_node)
                    return static_cast<id_t*>(var_node);
            }
            return nullptr;
        }

        void execute(buffer_t& buf, environments_t& env) override {
            for (auto statement : statements_)
                statement->execute(buf, env);
            memory_table_t::clear_memory();
        }

        void analyze(buffer_t& buf, environments_t& env) override {
            for (auto statement : statements_)
                statement->analyze(buf, env);
        }

        node_statement_t* copy(buffer_t& buf, node_scope_t* parent) const override {
            node_scope_t* scope = buf.add_node<node_scope_t>(node_loc_t::loc(), parent);
            for (auto statement : statements_)
                scope->add_statement(statement->copy(buf, scope));
            return scope;
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

        value_t analyze(buffer_t& buf, environments_t& env) override {
            return {node_type_e::NUMBER, this};
        }
    
        node_expression_t* copy(buffer_t& buf, node_scope_t* parent) const override {
            return buf.add_node<node_number_t>(node_loc_t::loc(), number_);
        }
    };

    /* ----------------------------------------------------- */

    class node_undef_t final : public node_type_t {
    public:
        node_undef_t(const location_t& loc) : node_loc_t(loc) {}

        value_t execute(buffer_t& buf, environments_t& env) override {
            return {node_type_e::UNDEF, this};
        }

        void print(buffer_t& buf, environments_t& env) override { env.os << "undef\n"; }

        value_t analyze(buffer_t& buf, environments_t& env) override {
            return {node_type_e::UNDEF, this};
        }

        node_expression_t* copy(buffer_t& buf, node_scope_t* parent) const override {
            return buf.add_node<node_undef_t>(node_loc_t::loc());
        }
    };

    /* ----------------------------------------------------- */

    class node_input_t final : public node_type_t {
    public:
        node_input_t(const location_t& loc) : node_loc_t(loc) {}

        value_t execute(buffer_t& buf, environments_t& env) override {
            int value;
            env.is >> value;
            if (env.is_analyzing && !env.is.good())
                throw error_execute_t{node_loc_t::loc(), env.program_str, "invalid input: need integer"};
            return {node_type_e::NUMBER, buf.add_node<node_number_t>(node_loc_t::loc(), value)};
        }

        value_t analyze(buffer_t& buf, environments_t& env) override {
            return {node_type_e::INPUT, buf.add_node<node_input_t>(node_loc_t::loc())};
        }

        void print(buffer_t& buf, environments_t& env) override { env.os << "?\n"; }

        node_expression_t* copy(buffer_t& buf, node_scope_t* parent) const override {
            return buf.add_node<node_input_t>(node_loc_t::loc());
        }
    };

    /* ----------------------------------------------------- */

    class node_indexes_t final : public node_t,
                                 virtual public node_loc_t {
        std::vector<node_expression_t*> indexes_;

    public:
        node_indexes_t(const location_t& loc) : node_loc_t(loc) {}

        std::vector<int> execute(buffer_t& buf, environments_t& env) const {
            std::vector<int> indexes;
            const int size = indexes_.size();
            for (int i : view::iota(0, size)) {
                value_t result = indexes_[i]->execute(buf, env);

                int index = static_cast<node_number_t*>(result.value)->get_value();
                indexes.push_back(index);
            }
            reverse(indexes.begin(), indexes.end());
            return indexes;
        }

        void add_index(node_expression_t* index) { indexes_.push_back(index); }

        std::vector<int> analyze(buffer_t& buf, environments_t& env) {
            std::vector<int> indexes;
            for (auto index : indexes_) {
                value_t result = index->analyze(buf, env);

                expect_types_ne(result.type, node_type_e::ARRAY, index->loc(), env);
                expect_types_ne(result.type, node_type_e::UNDEF, index->loc(), env);

                int index_value = static_cast<node_number_t*>(result.value)->get_value();
                indexes.push_back(index_value);
            }
            reverse(indexes.begin(), indexes.end());
            return indexes;
        }

        node_indexes_t* copy(buffer_t& buf, node_scope_t* parent) const {
            node_indexes_t* node_indexes = buf.add_node<node_indexes_t>(node_loc_t::loc());
            for (auto index : indexes_)
                node_indexes->add_index(index->copy(buf, parent));
            return node_indexes;
        }
    };

    /* ----------------------------------------------------- */

    class node_array_values_t {
    public:
        virtual std::vector<value_t> execute(buffer_t& buf, environments_t& env) const = 0;
        virtual std::pair<std::vector<value_t>, value_t> analyze(buffer_t& buf, environments_t& env) const = 0;
        virtual node_array_values_t* copy_vals(buffer_t& buf, node_scope_t* parent) const = 0;
    };

    /* ----------------------------------------------------- */

    class node_array_value_t : public node_t,
                               virtual public node_loc_t {
    public:
        virtual void add_value(std::vector<value_t>& values,
                               buffer_t& buf, environments_t& env) const = 0;
        virtual void add_value_analyze(std::vector<value_t>& values,
                                       buffer_t& buf, environments_t& env) const = 0;
        virtual node_array_value_t* copy_val   (buffer_t& buf, node_scope_t* parent) const = 0;
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

        void add_value_analyze(std::vector<value_t>& values, buffer_t& buf,
                               environments_t& env) const override {
            value_t result = value_->analyze(buf, env);
            values.push_back(result);
        }

        node_array_value_t* copy_val(buffer_t& buf, node_scope_t* parent) const override {
            return buf.add_node<node_expression_value_t>(node_loc_t::loc(), value_->copy(buf, parent));
        }
    };

    /* ----------------------------------------------------- */

    class node_input_t;

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

        void add_value_analyze(std::vector<value_t>& values, buffer_t& buf,
                               environments_t& env) const override {
            std::vector<value_t> result = analyze(buf, env).first;
            values.insert(values.end(), result.begin(), result.end());
        }

        std::vector<value_t> execute(buffer_t& buf, environments_t& env) const {
            value_t value = value_->execute(buf, env);
            value_t count = count_->execute(buf, env);
            size_t real_count = static_cast<node_number_t*>(count.value)->get_value();
            std::vector<value_t> values{real_count, value};
            return values;
        }

        std::pair<std::vector<value_t>, value_t> analyze(buffer_t& buf, environments_t& env) const override {
            value_t count = count_->analyze(buf, env);
            value_t value = value_->analyze(buf, env);

            if (count.type == node_type_e::INPUT)
                return {{}, {node_type_e::INPUT, buf.add_node<node_input_t>(count_->loc())}};

            expect_types_ne(count.type, node_type_e::UNDEF, count_->loc(), env);
            expect_types_ne(count.type, node_type_e::ARRAY, count_->loc(), env);

            size_t real_count = static_cast<node_number_t*>(count.value)->get_value(); // static check type
            std::vector<value_t> values{real_count, value};
            return {values, {node_type_e::NUMBER, buf.add_node<node_number_t>(count_->loc(), real_count)}};
        }

        node_array_values_t* copy_vals(buffer_t& buf, node_scope_t* parent) const override {
            return buf.add_node<node_repeat_values_t>(node_loc_t::loc(), value_->copy(buf, parent),
                                                      count_->copy(buf, parent));
        }

        node_array_value_t* copy_val(buffer_t& buf, node_scope_t* parent) const override {
            return buf.add_node<node_repeat_values_t>(node_loc_t::loc(), value_->copy(buf, parent),
                                                      count_->copy(buf, parent));
        }
    };

    /* ----------------------------------------------------- */

    class node_list_values_t : public node_t,
                               virtual public node_loc_t,
                               public node_array_values_t {
        std::vector<node_array_value_t*> values_;

    public:
        node_list_values_t(const location_t& loc) : node_loc_t(loc) {}

        std::vector<value_t> execute(buffer_t& buf, environments_t& env) const {
            std::vector<value_t> values;
            const int size = values_.size();
            for (int i : view::iota(0, size))
                values_[i]->add_value(values, buf, env);
            return values;
        }

        void add_value(node_array_value_t* value) { values_.push_back(value); }

        std::pair<std::vector<value_t>, value_t> analyze(buffer_t& buf, environments_t& env) const override {
            std::vector<value_t> values;
            const int size = values_.size();
            for (int i : view::iota(0, size))
                values_[i]->add_value_analyze(values, buf, env);
            return {values, {node_type_e::NUMBER, buf.add_node<node_number_t>(node_loc_t::loc(), size)}};
        }

        node_array_values_t* copy_vals(buffer_t& buf, node_scope_t* parent) const override {
            node_list_values_t* node_values = buf.add_node<node_list_values_t>(node_loc_t::loc());
            for (auto value : values_)
                node_values->add_value(value->copy_val(buf, parent));
            return node_values;
        }
    };

    /* ----------------------------------------------------- */

    class node_array_t final : public node_type_t,
                               public node_memory_t {
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

            if (!indexes.empty() && result.type == node_type_e::ARRAY)
                return static_cast<node_array_t*>(result.value)->shift_(indexes, buf, env);
            else
                return result;
        }

        value_t& shift_analyze_(std::vector<int>& indexes, buffer_t& buf, environments_t& env) {
            assert(!indexes.empty());

            int index = indexes.back();
            indexes.pop_back();
            value_t& result = values_[index];

            if (!indexes.empty() && result.type == node_type_e::ARRAY) // + check level error
                return static_cast<node_array_t*>(result.value)->shift_analyze_(indexes, buf, env);
            else
                return result;
        }

        void init(buffer_t& buf, environments_t& env) {
            values_       = init_values_->execute(buf, env);
            real_indexes_ = indexes_->execute(buf, env);
            is_inited_    = true;
        }

        void init_analyze(buffer_t& buf, environments_t& env) {
            values_       = init_values_->analyze(buf, env).first;
            real_indexes_ = indexes_->analyze(buf, env);
            is_inited_    = true;
        }

    public:
        node_array_t(const location_t& loc, node_array_values_t* init_values, node_indexes_t* indexes)
        : node_loc_t(loc), init_values_(init_values), indexes_(indexes) {}

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

        value_t& shift_analyze(const std::vector<int>& ext_indexes, buffer_t& buf, environments_t& env) {
            std::vector<int> final_indexes = ext_indexes;
            final_indexes.insert(final_indexes.end(), real_indexes_.begin(), real_indexes_.end());
            return shift_analyze_(final_indexes, buf, env);
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

        value_t analyze(buffer_t& buf, environments_t& env) override {
            if (!is_inited_)
                init_analyze(buf, env);

            return {node_type_e::ARRAY, this};
        }

        node_expression_t* copy(buffer_t& buf, node_scope_t* parent) const override {
            node_array_t* node_array = buf.add_node<node_array_t>(node_loc_t::loc(),
                                                                  init_values_->copy_vals(buf, parent),
                                                                  indexes_->copy(buf, parent));
            parent->add_array(node_array);
            return node_array;
        }
    };

    /* ----------------------------------------------------- */

    class settable_value_t : public node_t,
                             virtual public node_loc_t {
        bool is_setted = false;
        value_t value_;

    private:
        value_t& shift(const std::vector<int>& indexes, buffer_t& buf, environments_t& env) {
            if (indexes.size() == 0)
                return value_;

            node_array_t* array = static_cast<node_array_t*>(value_.value);
            return array->shift(indexes, buf, env);
        }

        value_t& shift_analyze(const std::vector<int>& indexes, buffer_t& buf, environments_t& env) {
            if (indexes.size() == 0) 
                return value_;

            node_array_t* array = static_cast<node_array_t*>(value_.value);
            return array->shift_analyze(indexes, buf, env);
        }

    public:
        value_t execute(node_indexes_t* indexes, buffer_t& buf, environments_t& env) {
            assert(indexes);
            std::vector<int> real_indexes = indexes->execute(buf, env);
            value_t& real_value = shift(real_indexes, buf, env);
            return real_value.value->execute(buf, env);
        }

        value_t set_value(node_indexes_t* indexes, value_t new_value,
                          buffer_t& buf, environments_t& env) {
            assert(indexes);
            std::vector<int> real_indexes = indexes->execute(buf, env);
            value_t& real_value = shift(real_indexes, buf, env);
            real_value = new_value;
            is_setted = true;
            return value_;
        }

        value_t analyze(node_indexes_t* indexes, buffer_t& buf, environments_t& env) {
            std::vector<int> real_indexes = indexes->analyze(buf, env);
            if (real_indexes.size() > 0 && !is_setted)
                throw error_analyze_t{node_loc_t::loc(), env.program_str,
                                      "attempt to indexing by not init variable"};

            value_t& real_value = shift_analyze(real_indexes, buf, env);
            return real_value.value->analyze(buf, env);
        }

        value_t set_value_analyze(node_indexes_t* indexes, value_t new_value,
                                  buffer_t& buf, environments_t& env) {
            assert(indexes);
            std::vector<int> real_indexes = indexes->analyze(buf, env);
            if (real_indexes.size() > 0 && !is_setted)
                throw error_analyze_t{node_loc_t::loc(), env.program_str,
                                      "attempt to indexing by not init variable"};

            value_t& real_value = shift_analyze(real_indexes, buf, env);
            real_value = new_value; // +check if types are different
            is_setted = true;
            return real_value.value->analyze(buf, env);;
        }

        virtual ~settable_value_t() = default;
    };

    /* ----------------------------------------------------- */

    class node_variable_t final : public id_t,
                                  public settable_value_t {
    public:
        node_variable_t(const location_t& loc, std::string_view id) : node_loc_t(loc), id_t(id) {}
        node_variable_t* copy(buffer_t& buf) const {
            return buf.add_node<node_variable_t>(node_loc_t::loc(), id_t::get_name());
        }
        void set_loc(const location_t& loc) { node_loc_t::set_loc(loc); }
    };

    /* ----------------------------------------------------- */

    class node_lvalue_t final : public node_expression_t {
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

        value_t set_value_analyze(value_t new_value, buffer_t& buf, environments_t& env) {
            return variable_->set_value_analyze(indexes_, new_value, buf, env);
        }

        value_t analyze(buffer_t& buf, environments_t& env) override {
            if (!variable_)
                throw error_analyze_t{node_loc_t::loc(), env.program_str,
                                      "declaration error: undeclared variable"};
            return variable_->analyze(indexes_, buf, env);
        }

        node_expression_t* copy(buffer_t& buf, node_scope_t* parent) const override {
            node_variable_t* var_node = nullptr;
            if (variable_) {
                var_node = static_cast<node_variable_t*>(parent->get_node(variable_->get_name()));
                if (!var_node)
                    var_node = variable_->copy(buf);
                else
                    var_node->set_loc(node_loc_t::loc());

                parent->add_variable(var_node);
            }

            return buf.add_node<node_lvalue_t>(node_loc_t::loc(), var_node, indexes_->copy(buf, parent));
        }
    };

    /* ----------------------------------------------------- */

    class node_assign_t final : public node_expression_t {
        node_lvalue_t*     lvalue_;
        node_expression_t* rvalue_;

    public:
        node_assign_t(const location_t& loc, node_lvalue_t* lvalue, node_expression_t* rvalue)
        : node_loc_t(loc), lvalue_(lvalue), rvalue_(rvalue) {}
        value_t execute(buffer_t& buf, environments_t& env) override {
            return lvalue_->set_value(rvalue_->execute(buf, env), buf, env);
        }

        value_t analyze(buffer_t& buf, environments_t& env) override {
            return lvalue_->set_value_analyze(rvalue_->analyze(buf, env), buf, env);
        }

        node_expression_t* copy(buffer_t& buf, node_scope_t* parent) const override {
            return buf.add_node<node_assign_t>(node_loc_t::loc(),
                                               static_cast<node_lvalue_t*>(lvalue_->copy(buf, parent)),
                                               rvalue_->copy(buf, parent));
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

        int evaluate(node_number_t* l_result, node_number_t* r_result, environments_t& env) {
            int LHS = l_result->get_value();
            int RHS = r_result->get_value();
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
                default: throw error_execute_t{node_loc_t::loc(), env.program_str,
                                               "attempt to execute unknown binary operator"};
            }
        }

    public:
        node_bin_op_t(const location_t& loc, binary_operators_e type,
                      node_expression_t* left, node_expression_t* right)
        : node_loc_t(loc), type_(type), left_(left), right_(right) {}

        value_t execute(buffer_t& buf, environments_t& env) override {
            value_t l_result = left_ ->execute(buf, env);
            value_t r_result = right_->execute(buf, env);

            if (l_result.type == node_type_e::UNDEF ||
                r_result.type == node_type_e::UNDEF)
                return {node_type_e::UNDEF, buf.add_node<node_undef_t>(node_loc_t::loc())};
            
            node_number_t* l_result_number = static_cast<node_number_t*>(l_result.value);
            node_number_t* r_result_number = static_cast<node_number_t*>(r_result.value);
            int result = evaluate(l_result_number, r_result_number, env);
            return {node_type_e::NUMBER, buf.add_node<node_number_t>(node_loc_t::loc(), result)};
        }

        value_t analyze(buffer_t& buf, environments_t& env) override {
            value_t l_result = left_->analyze(buf, env);
            value_t r_result = right_->analyze(buf, env);

            if (l_result.type == node_type_e::UNDEF ||
                l_result.type == node_type_e::INPUT)
                return l_result;

            if (r_result.type == node_type_e::UNDEF ||
                r_result.type == node_type_e::INPUT)
                return r_result;

            expect_types_ne(l_result.type, node_type_e::ARRAY, node_loc_t::loc(), env);
            expect_types_ne(r_result.type, node_type_e::ARRAY, node_loc_t::loc(), env);
        
            node_number_t* l_result_number = static_cast<node_number_t*>(l_result.value);
            node_number_t* r_result_number = static_cast<node_number_t*>(r_result.value);
            int result = evaluate(l_result_number, r_result_number, env);
            return {node_type_e::NUMBER, buf.add_node<node_number_t>(node_loc_t::loc(), result)};
        }

        node_expression_t* copy(buffer_t& buf, node_scope_t* parent) const override {
            return buf.add_node<node_bin_op_t>(node_loc_t::loc(), type_,
                                               left_->copy(buf, parent), right_->copy(buf, parent));
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

    private:
        int evaluate(node_number_t* result, environments_t& env) const {
            int value = result->get_value();
            switch (type_) {
                case unary_operators_e::ADD: return  value;
                case unary_operators_e::SUB: return -value;
                case unary_operators_e::NOT: return !value;
                default: throw error_execute_t{node_loc_t::loc(), env.program_str,
                                               "attempt to execute unknown unary operator"};
            }
        }

    public:
        node_un_op_t(const location_t& loc, unary_operators_e type, node_expression_t* node)
        : node_loc_t(loc), type_(type), node_(node) {}

        value_t execute(buffer_t& buf, environments_t& env) override {
            value_t res_exec = node_->execute(buf, env);

            if (res_exec.type == node_type_e::UNDEF)
                return {node_type_e::UNDEF, buf.add_node<node_undef_t>(node_loc_t::loc())};

            int result = evaluate(static_cast<node_number_t*>(res_exec.value), env);
            return {node_type_e::NUMBER, buf.add_node<node_number_t>(node_loc_t::loc(), result)};
        }

        value_t analyze(buffer_t& buf, environments_t& env) override {
            value_t res_exec = node_->analyze(buf, env);

            if (res_exec.type == node_type_e::UNDEF ||
                res_exec.type == node_type_e::INPUT)
                return res_exec;

            expect_types_ne(res_exec.type, node_type_e::ARRAY, node_loc_t::loc(), env);

            int result = evaluate(static_cast<node_number_t*>(res_exec.value), env);
            return {node_type_e::NUMBER, buf.add_node<node_number_t>(node_loc_t::loc(), result)};
        }

        node_expression_t* copy(buffer_t& buf, node_scope_t* parent) const override {
            return buf.add_node<node_un_op_t>(node_loc_t::loc(), type_, node_->copy(buf, parent));
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

        value_t analyze(buffer_t& buf, environments_t& env) override {
            return argument_->analyze(buf, env);
        }

        node_expression_t* copy(buffer_t& buf, node_scope_t* parent) const override {
            return buf.add_node<node_print_t>(node_loc_t::loc(), argument_->copy(buf, parent));
        }
    };

    /* ----------------------------------------------------- */

    class node_loop_t final : public node_statement_t {
        node_expression_t* condition_;
        node_scope_t* body_;

    private:
        inline int step(buffer_t& buf, environments_t& env) {
            value_t result = condition_->execute(buf, env);
            return static_cast<node_number_t*>(result.value)->get_value();
        }

        inline int step_analyze(buffer_t& buf, environments_t& env) {
            value_t result = condition_->analyze(buf, env);

            expect_types_ne(result.type, node_type_e::ARRAY, node_loc_t::loc(), env);
            expect_types_ne(result.type, node_type_e::UNDEF, node_loc_t::loc(), env);

            if (result.type == node_type_e::INPUT)
                return 0;

            return static_cast<node_number_t*>(result.value)->get_value();
        }

    public:
        node_loop_t(const location_t& loc, node_expression_t* condition, node_scope_t* body)
        : node_loc_t(loc), condition_(condition), body_(body) {}

        void execute(buffer_t& buf, environments_t& env) override {
            while (step(buf, env))
                body_->execute(buf, env);
        }

        void analyze(buffer_t& buf, environments_t& env) override {
            while (step_analyze(buf, env))
                body_->analyze(buf, env);
        }

        node_statement_t* copy(buffer_t& buf, node_scope_t* parent) const override {
            return buf.add_node<node_loop_t>(node_loc_t::loc(), condition_->copy(buf, parent),
                                             static_cast<node_scope_t*>(body_->copy(buf, parent)));
        }
    };

    /* ----------------------------------------------------- */

    class node_fork_t final : public node_statement_t {
        node_expression_t* condition_;
        node_scope_t* body1_;
        node_scope_t* body2_;

    public:
        node_fork_t(const location_t& loc, node_expression_t* condition,
                    node_scope_t* body1, node_scope_t* body2)
        : node_loc_t(loc), condition_(condition), body1_(body1), body2_(body2) {}

        void execute(buffer_t& buf, environments_t& env) override {
            value_t result = condition_->execute(buf, env);

            int value = static_cast<node_number_t*>(result.value)->get_value();
            if (value)
                body1_->execute(buf, env);
            else
                body2_->execute(buf, env);
        }

        void analyze(buffer_t& buf, environments_t& env) override {
            value_t result = condition_->analyze(buf, env);

            expect_types_ne(result.type, node_type_e::ARRAY, node_loc_t::loc(), env);
            expect_types_ne(result.type, node_type_e::UNDEF, node_loc_t::loc(), env);

            body1_->analyze(buf, env);
            body2_->analyze(buf, env);
        }

        node_statement_t* copy(buffer_t& buf, node_scope_t* parent) const override {
            return buf.add_node<node_fork_t>(node_loc_t::loc(), condition_->copy(buf, parent),
                                             static_cast<node_scope_t*>(body1_->copy(buf, parent)),
                                             static_cast<node_scope_t*>(body2_->copy(buf, parent)));
        }
    };
}