#pragma once

#include "common.hpp"
#include "environments.hpp"
#include <algorithm>
#include <cassert>
#include <memory>
#include <vector>
#include <unordered_map>

namespace node {
    struct location_t final {
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
                       << line.substr(loc + length, line_length) << '\n';

            for (int i : view::iota(0, line_length)) {
                if (i >= loc && i < loc + length)
                    error_line << print_red('^');
                else
                    error_line << ' ';
            }
            error_line << '\n';
            error_line << print_red("at location: (" << loc_.row << ", " << loc_.col << ")\n");
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

    struct analyze_params_t final {
        buffer_t* buf;
        std::string_view program_str;
    };

    /* ----------------------------------------------------- */

    struct execute_params_t final {
        buffer_t* buf;
        std::ostream* os;
        std::istream* is;
        std::string_view program_str;
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

    class value_t;
    class analyze_t;
    class node_scope_t;

    class node_expression_t : public node_t,
                              public node_loc_t {
    public:
        node_expression_t(const location_t& loc) : node_loc_t(loc) {}
        virtual value_t   execute(execute_params_t& params) = 0;
        virtual analyze_t analyze(analyze_params_t& params) = 0;
        virtual void set_unpredict() = 0;
        virtual node_expression_t* copy(buffer_t* buf, node_scope_t* parent) const = 0;
    };

    /* ----------------------------------------------------- */

    class node_type_t : public node_expression_t {
    public:
        node_type_t(const location_t& loc) : node_expression_t(loc) {}
        virtual void print(execute_params_t& params) = 0;
        virtual int  level() const = 0;
        void set_unpredict() override {}
    };

    class node_simple_type_t : public node_type_t {
    public:
        node_simple_type_t(const location_t& loc) : node_type_t(loc) {}
        int level() const override { return 0; };
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
            case node_type_e::INPUT:  return "number";
            default:                  return "unknown type";
        }
    }

    struct value_t final {
        node_type_e  type;
        node_type_t* value;

        void print() const {
            std::cout << "value type : " << type2str(type) << '\n';
            std::cout << "value value: " << value << '\n';
        }
    };

    /* ----------------------------------------------------- */

    struct analyze_t final {
        value_t result;
        bool    is_constexpr = true;

        analyze_t() {}
        analyze_t(bool is_constexpr_) : is_constexpr(is_constexpr_) {}
        analyze_t(const value_t& value_) : result(value_) {}
        analyze_t(node_type_e type_, node_type_t* value_) : result(type_, value_) {}
    };

    /* ----------------------------------------------------- */

    inline void expect_types_ne(node_type_e result,    node_type_e expected,
                                const location_t& loc, analyze_params_t& params) {
        if (result == expected)
            throw error_analyze_t{loc, params.program_str, "wrong type: " + type2str(result)};
    }

    /* ----------------------------------------------------- */

    class node_statement_t : public node_t,
                             public node_loc_t {
    public:
        node_statement_t(const location_t& loc) : node_loc_t(loc) {}
        virtual void execute(execute_params_t& params) = 0;
        virtual void analyze(analyze_params_t& params) = 0;
        virtual void set_unpredict() = 0;
        virtual node_statement_t* copy(buffer_t* buf, node_scope_t* parent) const = 0;
    };

    /* ----------------------------------------------------- */
    
    class node_instruction_t final : public node_statement_t {
        node_expression_t* expr_ = nullptr;

    public:
        node_instruction_t(const location_t& loc, node_expression_t* expr)
        : node_statement_t(loc), expr_(expr) {}

        void execute(execute_params_t& params) override {
            assert(expr_);
            expr_->execute(params);
        }

        void analyze(analyze_params_t& params) override {
            expr_->analyze(params);
        }

        node_statement_t* copy(buffer_t* buf, node_scope_t* parent) const override {
            return buf->add_node<node_instruction_t>(node_loc_t::loc(), expr_->copy(buf, parent));
        }

        void set_unpredict() override { expr_->set_unpredict(); };
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
        virtual void clear(buffer_t* buf) = 0;
        virtual ~node_memory_t() = default;
    };

    /* ----------------------------------------------------- */

    class memory_table_t {
        std::vector<node_memory_t*> arrays_;

    protected:
        void clear_memory(buffer_t* buf) {
            for (auto iter : arrays_)
                iter->clear(buf);
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
        node_scope_t(const location_t& loc, node_scope_t* parent)
        : node_statement_t(loc), parent_(parent) {} 

        void add_statement(node_statement_t* node) { statements_.push_back(node); }

        id_t* get_node(std::string_view name) const {
            for (auto scope = this; scope; scope = scope->parent_) {
                id_t* var_node = scope->get_var_node(name);
                if (var_node)
                    return static_cast<id_t*>(var_node);
            }
            return nullptr;
        }

        void execute(execute_params_t& params) override {
            for (auto statement : statements_)
                statement->execute(params);
            memory_table_t::clear_memory(params.buf);
        }

        void analyze(analyze_params_t& params) override {
            for (auto statement : statements_)
                statement->analyze(params);
            memory_table_t::clear_memory(params.buf);
        }

        node_statement_t* copy(buffer_t* buf, node_scope_t* parent) const override {
            node_scope_t* scope = buf->add_node<node_scope_t>(node_loc_t::loc(), parent);
            for (auto statement : statements_)
                scope->add_statement(statement->copy(buf, scope));
            return scope;
        }

        void set_unpredict() override {
            for (auto statement : statements_)
                statement->set_unpredict();
        }
    };

    /* ----------------------------------------------------- */

    class node_number_t final : public node_simple_type_t {
        int number_;

    public:
        node_number_t(const location_t& loc, int number) : node_simple_type_t(loc), number_(number) {}

        value_t execute(execute_params_t& params) override {
            return {node_type_e::NUMBER, this};
        }

        void print(execute_params_t& params) override { *(params.os) << number_ << '\n'; }

        int get_value() const noexcept { return number_; }

        analyze_t analyze(analyze_params_t& params) override {
            return {node_type_e::NUMBER, this};
        }
    
        node_expression_t* copy(buffer_t* buf, node_scope_t* parent) const override {
            return buf->add_node<node_number_t>(node_loc_t::loc(), number_);
        }
    };

    /* ----------------------------------------------------- */

    class node_undef_t final : public node_simple_type_t {
    public:
        node_undef_t(const location_t& loc) : node_simple_type_t(loc) {}

        value_t execute(execute_params_t& params) override {
            return {node_type_e::UNDEF, this};
        }

        void print(execute_params_t& params) override { *(params.os) << "undef\n"; }

        analyze_t analyze(analyze_params_t& params) override {
            return {node_type_e::UNDEF, this};
        }

        node_expression_t* copy(buffer_t* buf, node_scope_t* parent) const override {
            return buf->add_node<node_undef_t>(node_loc_t::loc());
        }
    };

    /* ----------------------------------------------------- */

    class node_input_t final : public node_simple_type_t {
    public:
        node_input_t(const location_t& loc) : node_simple_type_t(loc) {}

        value_t execute(execute_params_t& params) override {
            int value;
            *(params.is) >> value;
            if (!params.is->good())
                throw error_execute_t{node_loc_t::loc(), params.program_str, "invalid input: need integer"};
            
            return {node_type_e::NUMBER, params.buf->add_node<node_number_t>(node_loc_t::loc(), value)};
        }

        analyze_t analyze(analyze_params_t& params) override {
            return {node_type_e::INPUT, params.buf->add_node<node_input_t>(node_loc_t::loc())};
        }

        void print(execute_params_t& params) override { *(params.os) << "?\n"; }

        node_expression_t* copy(buffer_t* buf, node_scope_t* parent) const override {
            return buf->add_node<node_input_t>(node_loc_t::loc());
        }
    };

    /* ----------------------------------------------------- */

    class node_indexes_t final : public node_t,
                                 public node_loc_t {
        std::vector<node_expression_t*> indexes_;

    public:
        node_indexes_t(const location_t& loc) : node_loc_t(loc) {}

        std::vector<value_t> execute(execute_params_t& params) const {
            std::vector<value_t> indexes;
            const int size = indexes_.size();
            for (int i : view::iota(0, size)) {
                value_t index = indexes_[i]->execute(params);
                indexes.push_back(index);
            }
            reverse(indexes.begin(), indexes.end());
            return indexes;
        }

        std::vector<int> execute2ints(execute_params_t& params) const {
            std::vector<int> indexes;
            const int size = indexes_.size();
            for (int i : view::iota(0, size)) {
                value_t index = indexes_[i]->execute(params);
                int value = static_cast<node_number_t*>(index.value)->get_value();
                indexes.push_back(value);
            }
            reverse(indexes.begin(), indexes.end());
            return indexes;
        }

        void add_index(node_expression_t* index) { indexes_.push_back(index); }

        std::vector<analyze_t> analyze(analyze_params_t& params) {
            std::vector<analyze_t> indexes;
            for (auto index : indexes_) {
                analyze_t result = index->analyze(params);

                expect_types_ne(result.result.type, node_type_e::ARRAY, index->loc(), params);
                expect_types_ne(result.result.type, node_type_e::UNDEF, index->loc(), params);

                indexes.push_back(result);
            }
            reverse(indexes.begin(), indexes.end());
            return indexes;
        }

        node_indexes_t* copy(buffer_t* buf, node_scope_t* parent) const {
            node_indexes_t* node_indexes = buf->add_node<node_indexes_t>(node_loc_t::loc());
            for (auto index : indexes_)
                node_indexes->add_index(index->copy(buf, parent));
            return node_indexes;
        }

        location_t get_index_loc(int index) const {
            return indexes_[index]->loc();
        }
    };

    /* ----------------------------------------------------- */

    using array_execute_data_t = std::pair<std::vector<value_t>, bool>; // vals, is_in_heap
    using array_analyze_data_t = std::pair<std::vector<analyze_t>, bool>; // vals, is_in_heap
    class node_array_values_t {
    public:
        virtual array_execute_data_t execute(execute_params_t& params) const = 0;
        virtual array_analyze_data_t analyze(analyze_params_t& params) = 0;
        virtual int get_level() const = 0;
        virtual node_array_values_t* copy_vals(buffer_t* buf, node_scope_t* parent) const = 0;
        virtual ~node_array_values_t() = default;
    };

    /* ----------------------------------------------------- */

    class node_array_value_t : public node_t,
                               public node_loc_t {
    public:
        node_array_value_t(const location_t& loc) : node_loc_t(loc) {}
        virtual void add_value(std::vector<value_t>& values,
                               execute_params_t& params) const = 0;
        virtual void add_value_analyze(std::vector<analyze_t>& values, analyze_params_t& params) = 0;
        virtual node_array_value_t* copy_val(buffer_t* buf, node_scope_t* parent) const = 0;
    };

    /* ----------------------------------------------------- */

    class node_expression_value_t : public node_array_value_t {
        node_expression_t* value_;

    public:
        node_expression_value_t(const location_t& loc, node_expression_t* value)
        : node_array_value_t(loc), value_(value) {}

        void add_value(std::vector<value_t>& values, execute_params_t& params) const override {
            value_t result = value_->execute(params);
            values.push_back(result);
        }

        void add_value_analyze(std::vector<analyze_t>& values, analyze_params_t& params) override {
            analyze_t result = value_->analyze(params);
            values.push_back(result);
        }

        node_array_value_t* copy_val(buffer_t* buf, node_scope_t* parent) const override {
            return buf->add_node<node_expression_value_t>(node_loc_t::loc(), value_->copy(buf, parent));
        }
    };

    /* ----------------------------------------------------- */

    class node_repeat_values_t : public node_array_value_t,
                                 public node_array_values_t {
        node_expression_t* value_;
        node_expression_t* count_;
        int level_ = 0;

    private:
        node_repeat_values_t(const location_t& loc) : node_array_value_t(loc) {}

        void check_size_out(int size, std::string_view program_str) const {
            if (size <= 0)
                throw error_execute_t{count_->loc(), program_str,
                                        "wrong input size of repeat: \"" + std::to_string(size) + '\"'
                                      + ", less then 0"};
        }

    public:
        node_repeat_values_t(const location_t& loc, node_expression_t* value, node_expression_t* count)
        : node_array_value_t(loc), value_(value), count_(count) {}

        void add_value(std::vector<value_t>& values, execute_params_t& params) const override {
            std::vector<value_t> result = execute(params).first;
            values.insert(values.end(), result.begin(), result.end());
        }

        void add_value_analyze(std::vector<analyze_t>& values, analyze_params_t& params) override {
            std::vector<analyze_t> result = analyze(params).first;
            values.insert(values.end(), result.begin(), result.end());
        }

        array_execute_data_t execute(execute_params_t& params) const override {
            value_t value = value_->execute(params);
            value_t count = count_->execute(params);
            int real_count = static_cast<node_number_t*>(count.value)->get_value();

            check_size_out(real_count, params.program_str);

            std::vector<value_t> values(real_count);
            for (int i : view::iota(0, real_count)) {
                node_type_t* copy_val = static_cast<node_type_t*>(value.value->copy(params.buf, nullptr));
                values[i] = {value.type, copy_val};
            }
            
            if (count.type == node_type_e::INPUT)
                return {values, true};
            return {values, false};
        }

        array_analyze_data_t analyze(analyze_params_t& params) override {
            analyze_t count      = count_->analyze(params);
            analyze_t init_value = value_->analyze(params);

            level_ = init_value.result.value->level();

            value_t count_result = count.result;
            if (count_result.type == node_type_e::INPUT)
                return {{init_value.result}, true};

            expect_types_ne(count_result.type, node_type_e::UNDEF, count_->loc(), params);
            expect_types_ne(count_result.type, node_type_e::ARRAY, count_->loc(), params);

            size_t real_count = static_cast<node_number_t*>(count_result.value)->get_value();
            check_size_out(real_count, params.program_str);

            std::vector<analyze_t> values;
            values.assign(real_count, init_value.result);
            return {values, false};
        }

        node_array_values_t* copy_vals(buffer_t* buf, node_scope_t* parent) const override {
            return buf->add_node<node_repeat_values_t>(node_loc_t::loc(), value_->copy(buf, parent),
                                                       count_->copy(buf, parent));
        }

        node_array_value_t* copy_val(buffer_t* buf, node_scope_t* parent) const override {
            return buf->add_node<node_repeat_values_t>(node_loc_t::loc(), value_->copy(buf, parent),
                                                       count_->copy(buf, parent));
        }

        int get_level() const override { return level_; }
    };

    /* ----------------------------------------------------- */

    class node_list_values_t : public node_t,
                               public node_loc_t,
                               public node_array_values_t {
        std::vector<node_array_value_t*> values_;
        int level_ = 0;

    private:
        void level_analyze(const std::vector<analyze_t>& values, analyze_params_t& params) {
            bool is_setted = false;
            const int size = values.size();
            for (int i : view::iota(0, size)) {
                int elem_level = values[i].result.value->level();

                if (!is_setted) {
                    level_ = elem_level;
                    is_setted = true;
                    continue;
                }

                if (level_ != elem_level)
                    throw error_analyze_t{values_[i]->loc(), params.program_str,
                                          "different type in array"};
            }
        }

    public:
        node_list_values_t(const location_t& loc) : node_loc_t(loc) {}

        array_execute_data_t execute(execute_params_t& params) const override {
            std::vector<value_t> values;
            const int size = values_.size();
            for (int i : view::iota(0, size))
                values_[i]->add_value(values, params);
            return {values, false};
        }

        void add_value(node_array_value_t* value) { values_.push_back(value); }

        array_analyze_data_t analyze(analyze_params_t& params) override {
            std::vector<analyze_t> values;

            const int size = values_.size();
            for (int i : view::iota(0, size))
                values_[i]->add_value_analyze(values, params);

            level_analyze(values, params);
            return {values, false};
        }

        node_array_values_t* copy_vals(buffer_t* buf, node_scope_t* parent) const override {
            node_list_values_t* node_values = buf->add_node<node_list_values_t>(node_loc_t::loc());
            for (auto value : values_)
                node_values->add_value(value->copy_val(buf, parent));
            return node_values;
        }

        int get_level() const override { return level_; }
    };

    /* ----------------------------------------------------- */

    class node_array_t final : public node_type_t,
                               public node_memory_t {
        bool is_inited_ = false;
        node_array_values_t* init_values_;
        node_indexes_t*      init_indexes_;
        std::vector<value_t>   e_values_;
                                            // need to split this stuff, SRP:(
        std::vector<analyze_t> a_values_;
        std::vector<value_t>   e_indexes_;
        std::vector<analyze_t> a_indexes_;

        bool is_in_heap_ = false;
        bool is_freed_ = false;

    private:
        static analyze_t& shift_analyze_step(analyze_t& value, std::vector<analyze_t>& indexes,
                                             analyze_params_t& params,
                                             const std::vector<analyze_t>& all_indexes, int depth) {
            value_t result = value.result;
            if (result.type == node_type_e::ARRAY) {
                if (!indexes.empty())
                    return static_cast<node_array_t*>(result.value)->shift_analyze_(indexes, params,
                                                                                    all_indexes, depth);
                else
                    return value;
            } else {
                if (!indexes.empty())
                    throw error_analyze_t{indexes[0].result.value->loc(), params.program_str,
                                          "indexing in depth has gone beyond boundary of array"};
                return value;
            }
        }

        static void set_unpredict_below(analyze_t& value, std::vector<analyze_t>& indexes,
                                        analyze_params_t& params,
                                        const std::vector<analyze_t>& all_indexes, int depth) {
            std::vector<analyze_t> tmp_indexes{indexes};
            analyze_t& result = shift_analyze_step(value, tmp_indexes, params, all_indexes, depth);
            result.is_constexpr = false;
        }

    private:
        void analyze_check_freed(const location_t& loc, analyze_params_t& params) const {
            if (is_freed_)
                throw error_analyze_t{loc, params.program_str,
                                      "attempt to use freed array"};
        }

        location_t analyze_get_index_location(size_t depth, const std::vector<analyze_t>& all_indexes) const {
            if (depth >= a_indexes_.size())
                return all_indexes[all_indexes.size() - depth - 1].result.value->loc();

            return init_indexes_->get_index_loc(a_indexes_.size() - depth - 1);
        }

        location_t execute_get_index_location(size_t depth, const std::vector<value_t>& all_indexes) const {
            if (depth >= e_indexes_.size())
                return all_indexes[all_indexes.size() - depth - 1].value->loc();

            return init_indexes_->get_index_loc(e_indexes_.size() - depth - 1);
        }

        void analyze_check_index_out(int index, size_t depth, const std::vector<analyze_t>& all_indexes,
                                     analyze_params_t params) const {
            if (index < 0) {
                location_t loc = analyze_get_index_location(depth, all_indexes);
                throw error_analyze_t{loc, params.program_str,
                                        "wrong index in array: \"" + std::to_string(index) + '\"'
                                      + ", less then 0"};
            }

            int array_size = a_values_.size();
            if (index >= array_size && !is_in_heap_) {
                location_t loc = analyze_get_index_location(depth, all_indexes);
                throw error_analyze_t{loc, params.program_str,
                                        "wrong index in array: \"" + std::to_string(index)      + '\"'
                                      + ", when array size: \""    + std::to_string(array_size) + '\"'};
            }
        }

        void execute_check_index_out(int index, size_t depth, const std::vector<value_t>& all_indexes,
                                     execute_params_t params) const {
            if (index < 0) {
                location_t loc = execute_get_index_location(depth, all_indexes);
                throw error_execute_t{loc, params.program_str,
                                        "wrong index in array: \"" + std::to_string(index) + '\"'
                                      + ", less then 0"};
            }

            int array_size = e_values_.size();
            if (index >= array_size) {
                location_t loc = execute_get_index_location(depth, all_indexes);
                throw error_execute_t{loc, params.program_str,
                                        "wrong index in array: \"" + std::to_string(index)      + '\"'
                                      + ", when array size: \""    + std::to_string(array_size) + '\"'};
            }
        }

        std::string transform_print_str(const std::string& str) const {
            std::string result = str.substr(0, str.rfind('\n'));
            size_t pos = result.find('\n');
            while (pos != std::string::npos) {
                result.replace(pos, 1, ", ");
                pos = result.find('\n', pos + 2);
            }
            return result;
        }

        value_t& shift_(std::vector<value_t>& indexes, execute_params_t& params,
                        const std::vector<value_t>& all_indexes, int depth) {
            value_t index_value = indexes.back().value->execute(params);
            node_number_t* node_index = static_cast<node_number_t*>(index_value.value);
            int index = node_index->get_value();
            indexes.pop_back();

            execute_check_index_out(index, depth, all_indexes, params);

            value_t& result = e_values_[index];

            if (!indexes.empty() && result.type == node_type_e::ARRAY)
                return static_cast<node_array_t*>(result.value)->shift_(indexes, params,
                                                                        all_indexes, depth + 1);
            else
                return result;
        }

        analyze_t& shift_analyze_size_type_input(std::vector<analyze_t>& indexes, analyze_params_t& params,
                                                 const std::vector<analyze_t>& all_indexes, int depth) {
            analyze_t& result = a_values_[0];
            indexes.pop_back();
            return shift_analyze_step(result, indexes, params, all_indexes, depth + 1);
        }

        analyze_t& shift_analyze_number(std::vector<analyze_t>& indexes, node_number_t* index_node,
                                        analyze_params_t& params,
                                        const std::vector<analyze_t>& all_indexes, int depth) {
            int index = index_node->get_value();
            indexes.pop_back();
            analyze_t& result = a_values_[index];
            return shift_analyze_step(result, indexes, params, all_indexes, depth + 1);
        }

        analyze_t& shift_analyze_unpredict(std::vector<analyze_t>& indexes, analyze_params_t& params,
                                           const std::vector<analyze_t>& all_indexes, int depth) {
            indexes.pop_back();

            const int size = a_values_.size();
            for (int i : view::iota(0, size)) {
                analyze_t& result = a_values_[i];
                set_unpredict_below(result, indexes, params, all_indexes, depth + 1);
            }

            return shift_analyze_step(a_values_[0], indexes, params, all_indexes, depth);
        }

        analyze_t& shift_analyze_(std::vector<analyze_t>& indexes, analyze_params_t& params,
                                  const std::vector<analyze_t>& all_indexes, int depth) {
            if (is_in_heap_)
                return shift_analyze_size_type_input(indexes, params, all_indexes, depth);

            analyze_t a_index = indexes.back();
            value_t     index = a_index.result;
            if (a_index.is_constexpr &&
                index.type == node_type_e::NUMBER) {

                node_number_t* node_index = static_cast<node_number_t*>(index.value);

                if (a_index.is_constexpr)
                    analyze_check_index_out(node_index->get_value(), depth, all_indexes, params);

                return shift_analyze_number(indexes, node_index, params, all_indexes, depth);
            }

            return shift_analyze_unpredict(indexes, params, all_indexes, depth);
        }

        void init(execute_params_t& params) {
            array_execute_data_t values_analyze_res = init_values_->execute(params);
            e_values_   = values_analyze_res.first;
            e_indexes_  = init_indexes_->execute(params);
            is_in_heap_ = values_analyze_res.second;
            is_inited_  = true;
        }

        void init_analyze(analyze_params_t& params) {
            array_analyze_data_t values_analyze_res = init_values_->analyze(params);
            a_values_     = values_analyze_res.first;
            a_indexes_    = init_indexes_->analyze(params);
            is_in_heap_   = values_analyze_res.second;
            is_inited_    = true;
        }

    public:
        node_array_t(const location_t& loc, node_array_values_t* init_values, node_indexes_t* indexes)
        : node_type_t(loc), init_values_(init_values), init_indexes_(indexes) {}

        value_t execute(execute_params_t& params) override {
            if (!is_inited_)
                init(params);
            
            if (!e_indexes_.empty())
                return shift({}, params);
            return {node_type_e::ARRAY, this};
        }

        value_t& shift(const std::vector<value_t>& ext_indexes, execute_params_t& params) {
            std::vector<value_t> all_indexes = ext_indexes;
            all_indexes.insert(all_indexes.end(), e_indexes_.begin(), e_indexes_.end());
            return shift_(all_indexes, params, std::vector<value_t>{all_indexes}, 0);
        }

        analyze_t analyze(analyze_params_t& params) override {
            analyze_check_freed(node_loc_t::loc(), params);
            if (!is_inited_)
                init_analyze(params);

            if (a_indexes_.size() > 0)
                shift_analyze({}, params);

            return {node_type_e::ARRAY, this};
        }

        analyze_t& shift_analyze(const std::vector<analyze_t>& ext_indexes, analyze_params_t& params) {
            std::vector<analyze_t> all_indexes = ext_indexes;
            all_indexes.insert(all_indexes.end(), a_indexes_.begin(), a_indexes_.end());
            analyze_check_freed(all_indexes[0].result.value->loc(), params);
            return shift_analyze_(all_indexes, params, std::vector<analyze_t>{all_indexes}, 0);
        }

        void print(execute_params_t& params) override {
            if (!e_indexes_.empty()) {
                shift({}, params).value->print(params);
                return;
            }
            execute(params);
            
            std::stringstream print_stream;
            execute_params_t print_params{params.buf, &print_stream, params.is, params.program_str};

            const int size = e_values_.size();
            for (int i : view::iota(0, size - 1))
                e_values_[i].value->print(print_params);
            e_values_[size - 1].value->print(print_params);

            *(params.os) << '[' << transform_print_str(print_stream.str()) << "]\n";
        }

        void clear(buffer_t* buf) {
            is_inited_ = false;
            if (is_in_heap_) {
                is_freed_ = true;
                e_values_.clear();
                a_values_.clear();
                e_indexes_.clear();
                a_indexes_.clear();
            }
        }

        node_expression_t* copy(buffer_t* buf, node_scope_t* parent) const override {
            node_array_t* node_array = buf->add_node<node_array_t>(node_loc_t::loc(),
                                                                   init_values_->copy_vals(buf, parent),
                                                                   init_indexes_->copy(buf, parent));
            if (parent)
                parent->add_array(node_array);
            return node_array;
        }

        int level() const override { return 1 + init_values_->get_level(); };
    };

    /* ----------------------------------------------------- */

    class settable_value_t : public node_t,
                             public node_loc_t {
        bool is_setted = false;
        value_t   e_value_;
        analyze_t a_value_;

    private:
        static void expect_types_assignable(const analyze_t& a_lvalue, const analyze_t& a_rvalue,
                                            const location_t& assign_loc, analyze_params_t& params) {
            value_t lvalue = a_lvalue.result;
            value_t rvalue = a_rvalue.result;
            
            node_type_e l_type = lvalue.type;
            node_type_e r_type = rvalue.type;

            if (l_type != node_type_e::ARRAY &&
                r_type != node_type_e::ARRAY)
                return;
            
            if (l_type == node_type_e::ARRAY &&
                r_type == node_type_e::ARRAY) {
                int l_level = lvalue.value->level();
                int r_level = rvalue.value->level();
                if (l_level != r_level) {
                    std::string error_msg = "wrong levels of arrays in assign: "
                                            + std::to_string(r_level) + " levels of array nesting "
                                            + "cannot be assigned to "
                                            + std::to_string(l_level) + " levels of array nesting";
                    throw error_analyze_t{assign_loc, params.program_str, error_msg};
                }
                return;
            }
                
            std::string error_msg =   "wrong types in assign: " + type2str(r_type)
                                    + " cannot be assigned to " + type2str(l_type);
            throw error_analyze_t{assign_loc, params.program_str, error_msg};
        }

        value_t& shift(const std::vector<value_t>& indexes, execute_params_t& params) {
            if (indexes.size() == 0)
                return e_value_;

            node_array_t* array = static_cast<node_array_t*>(e_value_.value);
            return array->shift(indexes, params);
        }

        analyze_t& shift_analyze(const std::vector<analyze_t>& indexes, analyze_params_t& params) {
            if (indexes.size() == 0)
                return a_value_;
            
            value_t value = a_value_.result;
            expect_types_ne(value.type, node_type_e::UNDEF,  node_loc_t::loc(), params);
            expect_types_ne(value.type, node_type_e::INPUT,  node_loc_t::loc(), params);
            expect_types_ne(value.type, node_type_e::NUMBER, node_loc_t::loc(), params);

            node_array_t* array = static_cast<node_array_t*>(value.value);
            return array->shift_analyze(indexes, params);
        }

    public:
        settable_value_t(const location_t& loc) : node_loc_t(loc) {}

        value_t execute(node_indexes_t* indexes, execute_params_t& params) {
            value_t& real_value = shift(indexes->execute(params), params);
            return real_value;
        }

        value_t set_value(node_indexes_t* indexes, value_t new_value,
                          execute_params_t& params) {
            value_t& real_value = shift(indexes->execute(params), params);
            is_setted = true;
            return real_value = new_value;
        }

        analyze_t analyze(node_indexes_t* ext_indexes, analyze_params_t& params) {
            std::vector<analyze_t> indexes = ext_indexes->analyze(params);
            if (indexes.size() > 0 && !is_setted)
                throw error_analyze_t{node_loc_t::loc(), params.program_str,
                                      "attempt to indexing by not init variable"};

            return shift_analyze(indexes, params);
        }

        analyze_t set_value_analyze(node_indexes_t* ext_indexes, analyze_t new_value,
                                    analyze_params_t& params, const location_t& assign_loc) {
            std::vector<analyze_t> indexes = ext_indexes->analyze(params);
            if (indexes.size() > 0 && !is_setted)
                throw error_analyze_t{node_loc_t::loc(), params.program_str,
                                      "attempt to indexing by not init variable"};

            analyze_t& shift_result = shift_analyze(indexes, params);

            if (is_setted)
                expect_types_assignable(shift_result, new_value, assign_loc, params);

            is_setted = true;
            shift_result.result = new_value.result;
            shift_result.is_constexpr &= new_value.is_constexpr;
            return shift_result;
        }

        void set_unpredict() { a_value_.is_constexpr = false; }

        virtual ~settable_value_t() = default;
    };

    /* ----------------------------------------------------- */

    class node_variable_t final : public id_t,
                                  public settable_value_t {
    public:
        node_variable_t(const location_t& loc, std::string_view id)
        : id_t(id), settable_value_t(loc) {}

        node_variable_t* copy(buffer_t* buf) const {
            return buf->add_node<node_variable_t>(node_loc_t::loc(), id_t::get_name());
        }

        void set_loc(const location_t& loc) { node_loc_t::set_loc(loc); }
    };

    /* ----------------------------------------------------- */

    class node_lvalue_t final : public node_expression_t {
        node_variable_t* variable_ = nullptr;
        node_indexes_t*  indexes_;

    public:
        node_lvalue_t(const location_t& loc, node_variable_t* variable, node_indexes_t* indexes)
        : node_expression_t(loc), variable_(variable), indexes_(indexes) {}

        value_t execute(execute_params_t& params) override {
            return variable_->execute(indexes_, params);
        }

        value_t set_value(value_t new_value, execute_params_t& params) {
            return variable_->set_value(indexes_, new_value, params);
        }

        analyze_t set_value_analyze(analyze_t new_value, analyze_params_t& params,
                                    const location_t& assign_loc) {
            return variable_->set_value_analyze(indexes_, new_value, params, assign_loc);
        }

        analyze_t analyze(analyze_params_t& params) override {
            if (!variable_)
                throw error_analyze_t{node_loc_t::loc(), params.program_str,
                                      "declaration error: undeclared variable"};
            return variable_->analyze(indexes_, params);
        }

        node_expression_t* copy(buffer_t* buf, node_scope_t* parent) const override {
            node_variable_t* var_node = nullptr;
            if (variable_) {
                if (parent)
                    var_node = static_cast<node_variable_t*>(parent->get_node(variable_->get_name()));
                
                if (!var_node)
                    var_node = variable_->copy(buf);
                else
                    var_node->set_loc(node_loc_t::loc());

                parent->add_variable(var_node);
            }

            return buf->add_node<node_lvalue_t>(node_loc_t::loc(), var_node, indexes_->copy(buf, parent));
        }

        void set_unpredict() override { if (variable_) variable_->set_unpredict(); }
    };

    /* ----------------------------------------------------- */

    class node_assign_t final : public node_expression_t {
        node_lvalue_t*     lvalue_;
        node_expression_t* rvalue_;

    public:
        node_assign_t(const location_t& loc, node_lvalue_t* lvalue, node_expression_t* rvalue)
        : node_expression_t(loc), lvalue_(lvalue), rvalue_(rvalue) {}
        value_t execute(execute_params_t& params) override {
            return lvalue_->set_value(rvalue_->execute(params), params);
        }

        analyze_t analyze(analyze_params_t& params) override {
            return lvalue_->set_value_analyze(rvalue_->analyze(params), params, node_loc_t::loc());
        }

        node_expression_t* copy(buffer_t* buf, node_scope_t* parent) const override {
            return buf->add_node<node_assign_t>(node_loc_t::loc(),
                                                static_cast<node_lvalue_t*>(lvalue_->copy(buf, parent)),
                                                rvalue_->copy(buf, parent));
        }

        void set_unpredict() override { lvalue_->set_unpredict(); }
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

        int evaluate(node_number_t* l_result, node_number_t* r_result) {
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
                default: throw common::error_t{"attempt to execute unknown binary operator"};
            }
        }

    public:
        node_bin_op_t(const location_t& loc, binary_operators_e type,
                      node_expression_t* left, node_expression_t* right)
        : node_expression_t(loc), type_(type), left_(left), right_(right) {}

        value_t execute(execute_params_t& params) override {
            value_t l_result = left_ ->execute(params);
            value_t r_result = right_->execute(params);

            if (l_result.type == node_type_e::UNDEF ||
                r_result.type == node_type_e::UNDEF)
                return {node_type_e::UNDEF, params.buf->add_node<node_undef_t>(node_loc_t::loc())};
            
            node_number_t* l_result_number = static_cast<node_number_t*>(l_result.value);
            node_number_t* r_result_number = static_cast<node_number_t*>(r_result.value);
            int result = evaluate(l_result_number, r_result_number);
            return {node_type_e::NUMBER, params.buf->add_node<node_number_t>(node_loc_t::loc(), result)};
        }

        analyze_t analyze(analyze_params_t& params) override {
            analyze_t a_l_result = left_->analyze(params);
            analyze_t a_r_result = right_->analyze(params);

            value_t l_result = a_l_result.result;
            value_t r_result = a_r_result.result;

            if (l_result.type == node_type_e::UNDEF ||
                l_result.type == node_type_e::INPUT)
                return a_l_result;

            if (r_result.type == node_type_e::UNDEF ||
                r_result.type == node_type_e::INPUT)
                return a_r_result;

            expect_types_ne(l_result.type, node_type_e::ARRAY, node_loc_t::loc(), params);
            expect_types_ne(r_result.type, node_type_e::ARRAY, node_loc_t::loc(), params);
        
            node_number_t* l_result_number = static_cast<node_number_t*>(l_result.value);
            node_number_t* r_result_number = static_cast<node_number_t*>(r_result.value);
            int result = evaluate(l_result_number, r_result_number);

            analyze_t a_result{node_type_e::NUMBER, params.buf->add_node<node_number_t>(node_loc_t::loc(), result)};
            a_result.is_constexpr = a_l_result.is_constexpr & a_r_result.is_constexpr;
            return a_result;
        }

        node_expression_t* copy(buffer_t* buf, node_scope_t* parent) const override {
            return buf->add_node<node_bin_op_t>(node_loc_t::loc(), type_,
                                                left_->copy(buf, parent), right_->copy(buf, parent));
        }

        void set_unpredict() override {
            left_->set_unpredict();
            right_->set_unpredict();
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
        int evaluate(node_number_t* result) const {
            int value = result->get_value();
            switch (type_) {
                case unary_operators_e::ADD: return  value;
                case unary_operators_e::SUB: return -value;
                case unary_operators_e::NOT: return !value;
                default: throw common::error_t{"attempt to execute unknown unary operator"};
            }
        }

    public:
        node_un_op_t(const location_t& loc, unary_operators_e type, node_expression_t* node)
        : node_expression_t(loc), type_(type), node_(node) {}

        value_t execute(execute_params_t& params) override {
            value_t res_exec = node_->execute(params);

            if (res_exec.type == node_type_e::UNDEF)
                return {node_type_e::UNDEF, params.buf->add_node<node_undef_t>(node_loc_t::loc())};

            int result = evaluate(static_cast<node_number_t*>(res_exec.value));
            return {node_type_e::NUMBER, params.buf->add_node<node_number_t>(node_loc_t::loc(), result)};
        }

        analyze_t analyze(analyze_params_t& params) override {
            analyze_t a_res_exec = node_->analyze(params);
            value_t     res_exec = a_res_exec.result;

            if (res_exec.type == node_type_e::UNDEF ||
                res_exec.type == node_type_e::INPUT)
                return a_res_exec;

            expect_types_ne(res_exec.type, node_type_e::ARRAY, node_loc_t::loc(), params);

            int result = evaluate(static_cast<node_number_t*>(res_exec.value));
            analyze_t a_result{node_type_e::NUMBER, params.buf->add_node<node_number_t>(node_loc_t::loc(), result)};
            a_result.is_constexpr = a_res_exec.is_constexpr;
            return a_result;
        }

        node_expression_t* copy(buffer_t* buf, node_scope_t* parent) const override {
            return buf->add_node<node_un_op_t>(node_loc_t::loc(), type_, node_->copy(buf, parent));
        }

        void set_unpredict() override { node_->set_unpredict(); }
    };

    /* ----------------------------------------------------- */

    class node_print_t final : public node_expression_t {
        node_expression_t* argument_;

    public:
        node_print_t(const location_t& loc, node_expression_t* argument)
        : node_expression_t(loc), argument_(argument) {}

        value_t execute(execute_params_t& params) override {
            value_t result = argument_->execute(params);
            result.value->print(params);
            return result;
        }

        analyze_t analyze(analyze_params_t& params) override {
            return argument_->analyze(params);
        }

        node_expression_t* copy(buffer_t* buf, node_scope_t* parent) const override {
            return buf->add_node<node_print_t>(node_loc_t::loc(), argument_->copy(buf, parent));
        }

        void set_unpredict() override { argument_->set_unpredict(); }
    };

    /* ----------------------------------------------------- */

    class node_loop_t final : public node_statement_t {
        node_expression_t* condition_;
        node_scope_t* body_;

    private:
        inline void check_step_type(node_type_e type, execute_params_t& params) const {
            if (type == node_type_e::UNDEF)
                throw error_execute_t{node_loc_t::loc(), params.program_str,
                                      "wrong type: undef, excpected int"};
        }

        inline int step(execute_params_t& params) {
            value_t result = condition_->execute(params);

            check_step_type(result.type, params);

            return static_cast<node_number_t*>(result.value)->get_value();
        }

        inline void check_condition(analyze_params_t& params) {
            analyze_t a_result = condition_->analyze(params);
            value_t     result = a_result.result;

            expect_types_ne(result.type, node_type_e::ARRAY, node_loc_t::loc(), params);
            expect_types_ne(result.type, node_type_e::UNDEF, node_loc_t::loc(), params);
        }

    public:
        node_loop_t(const location_t& loc, node_expression_t* condition, node_scope_t* body)
        : node_statement_t(loc), condition_(condition), body_(body) {}

        void execute(execute_params_t& params) override {
            while (step(params))
                body_->execute(params);
        }

        void analyze(analyze_params_t& params) override {
            check_condition(params);
            body_->set_unpredict();
            body_->analyze(params);
        }

        node_statement_t* copy(buffer_t* buf, node_scope_t* parent) const override {
            return buf->add_node<node_loop_t>(node_loc_t::loc(), condition_->copy(buf, parent),
                                             static_cast<node_scope_t*>(body_->copy(buf, parent)));
        }

        void set_unpredict() override {
            body_->set_unpredict();
        };
    };

    /* ----------------------------------------------------- */

    class node_fork_t final : public node_statement_t {
        node_expression_t* condition_;
        node_scope_t* body1_;
        node_scope_t* body2_;

    public:
        node_fork_t(const location_t& loc, node_expression_t* condition,
                    node_scope_t* body1, node_scope_t* body2)
        : node_statement_t(loc), condition_(condition), body1_(body1), body2_(body2) {}

        void execute(execute_params_t& params) override {
            value_t result = condition_->execute(params);

            int value = static_cast<node_number_t*>(result.value)->get_value();
            if (value)
                body1_->execute(params);
            else
                body2_->execute(params);
        }

        void analyze(analyze_params_t& params) override {
            value_t result = condition_->analyze(params).result;

            expect_types_ne(result.type, node_type_e::ARRAY, node_loc_t::loc(), params);
            expect_types_ne(result.type, node_type_e::UNDEF, node_loc_t::loc(), params);

            body1_->set_unpredict();
            body2_->set_unpredict();

            body1_->analyze(params);
            body2_->analyze(params);
        }

        node_statement_t* copy(buffer_t* buf, node_scope_t* parent) const override {
            return buf->add_node<node_fork_t>(node_loc_t::loc(), condition_->copy(buf, parent),
                                              static_cast<node_scope_t*>(body1_->copy(buf, parent)),
                                              static_cast<node_scope_t*>(body2_->copy(buf, parent)));
        }

        void set_unpredict() override {
            body1_->set_unpredict();
            body2_->set_unpredict();
        };
    };
}