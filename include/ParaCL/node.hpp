#pragma once

#include "common.hpp"
#include "environments.hpp"
#include <algorithm>
#include <cassert>
#include <memory>
#include <ranges>
#include <unordered_map>
#include <variant>
#include <vector>

namespace paracl {
    struct location_t final {
        int row = -1;
        int col = -1;
        int len = -1;
    };

    /* ----------------------------------------------------- */

    inline std::pair<std::string_view, int> get_current_line(const location_t& loc,
                                                             std::string_view program_str) {
        int line = 0;
        for ([[maybe_unused]]int _ : std::views::iota(0, loc.row))
            line = program_str.find('\n', line + 1);

        if (line > 0)
            line++;

        int end_of_line = program_str.find('\n', line);
        if (end_of_line == -1)
            end_of_line = program_str.length();

        return std::make_pair(program_str.substr(line, end_of_line - line), loc.col - 2);
    };

    inline std::string get_error_line(const location_t& loc_, std::string_view program_str) {
        std::stringstream error_line;

        std::pair<std::string_view, int> line_info = get_current_line(loc_, program_str);
        std::string_view line = line_info.first;
        int length = loc_.len;
        int loc = line_info.second - length + 1;
        const int line_length = line.length();

        error_line << line.substr(0, loc)
        << print_red(line.substr(loc, length))
        << line.substr(loc + length, line_length) << '\n';

        for (int i : std::views::iota(0, line_length)) {
            if (i >= loc && i < loc + length)
                error_line << print_red('^');
                else
                error_line << ' ';
        }
        error_line << '\n';
        error_line << print_red("at location: (" << loc_.row << ", " << loc_.col << ")\n");
        return error_line.str();
    }

    /* ----------------------------------------------------- */

    class error_location_t : public error_t {
    public:
        error_location_t(const location_t& loc, std::string_view program_str, const std::string& msg)
        : error_t(get_error_line(loc, program_str) + str_red(msg)) {}
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

    class error_type_deduction_t : public error_location_t {
    public:
        error_type_deduction_t(const location_t& loc, std::string_view program_str, const std::string& msg)
        : error_location_t(loc, program_str, str_red("type deduction failed: " + msg)) {}
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
    class scope_base_t;
    class execute_params_t;
    class analyze_params_t;

    class node_expression_t : public node_t,
                              public node_loc_t {
    public:
        node_expression_t(const location_t& loc) : node_loc_t(loc) {}
        virtual value_t   execute(execute_params_t& params) = 0;
        virtual analyze_t analyze(analyze_params_t& params) = 0;
        virtual void set_unpredict() = 0;
        virtual node_expression_t* copy(buffer_t* buf, scope_base_t* parent) const = 0;
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
        node_type_t* value = nullptr;

        void print() const {
            assert(value);
            std::cout << "value type : " << type2str(type) << '\n';
            std::cout << "value value: " << value << '\n';
        }
    };

    /* ----------------------------------------------------- */

    struct execute_params_t final {
        buffer_t* buf;
        std::ostream* os = nullptr;
        std::istream* is = nullptr;
        std::string_view program_str = {};

        bool is_return = false;
        value_t return_result;

        execute_params_t(buffer_t* buf_,   std::ostream* os_,
                         std::istream* is_, std::string_view program_str_)
        : buf(buf_), os(os_), is(is_), program_str(program_str_) {
            assert(buf);
            assert(os);
            assert(is);
        }
    };

    /* ----------------------------------------------------- */

    struct analyze_t final {
        value_t result;
        bool    is_constexpr = true;

        analyze_t() {}
        analyze_t(bool is_constexpr_) : is_constexpr(is_constexpr_) {}
        analyze_t(const value_t& value_) : result(value_) {}
        analyze_t(node_type_e type_, node_type_t* value_) : result(type_, value_) { assert(value_); }
    };

    /* ----------------------------------------------------- */

    struct analyze_params_t final {
        buffer_t* buf;
        std::string_view program_str = {};

        bool is_return = false;
        analyze_t return_result;

        analyze_params_t(buffer_t* buf_, std::string_view program_str_ = {})
        : buf(buf_), program_str(program_str_) {
            assert(buf);
        }
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
        virtual node_statement_t* copy(buffer_t* buf, scope_base_t* parent) const = 0;
    };

    /* ----------------------------------------------------- */

    class node_instruction_t final : public node_statement_t {
        node_expression_t* expr_;

    public:
        node_instruction_t(const location_t& loc, node_expression_t* expr)
        : node_statement_t(loc), expr_(expr) { assert(expr_); }

        void execute(execute_params_t& params) override {
            expr_->execute(params);
        }

        void analyze(analyze_params_t& params) override {
            expr_->analyze(params);
        }

        node_statement_t* copy(buffer_t* buf, scope_base_t* parent) const override {
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
        void add_variable(id_t* node) { assert(node); variables_.emplace(node->get_name(), node); }

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
            std::ranges::for_each(arrays_, [buf](auto iter) {
                iter->clear(buf);
            });
        }

    public:
        void add_array(node_memory_t* node) { assert(node); arrays_.push_back(node); }
        virtual ~memory_table_t() = default;
    };

    /* ----------------------------------------------------- */

    class scope_base_t : public name_table_t,
                         public memory_table_t {
        scope_base_t* parent_;

    protected:
        std::vector<node_statement_t*> statements_;
        node_expression_t* return_expr_ = nullptr;

    protected:
        template <typename FuncT, typename ParamsT>
        void process_statements(FuncT&& func, ParamsT& params) const {
            for (auto statement : statements_) {
                std::forward<FuncT>(func)(statement, params);
                if (params.is_return)
                    break;
            }
        }

        template <typename FuncT>
        void through_statements(FuncT&& func) const {
            std::ranges::for_each(statements_, [&func](auto statement) {
                std::forward<FuncT>(func)(statement);
            });
        }

        template <typename ScopeT, typename ResT>
        ResT* copy_impl(const location_t& loc, buffer_t* buf, scope_base_t* parent) const {
            ScopeT* scope = buf->add_node<ScopeT>(loc, parent);
            through_statements([&](auto statement) { scope->add_statement(statement->copy(buf, scope)); });
            if (return_expr_)
                scope->add_return(return_expr_->copy(buf, scope));
            return scope;
        }

        void set_unpredict_impl() {
            through_statements([](auto statement) { statement->set_unpredict(); });
            if (return_expr_)
                return_expr_->set_unpredict();
        }

    public:
        scope_base_t(scope_base_t* parent) : parent_(parent) {} 

        void add_statement(node_statement_t* node) {
            assert(node);
            if (!return_expr_)
                statements_.push_back(node);
        }

        void add_return(node_expression_t* node) {
            assert(node);
            if (!return_expr_)
                return_expr_ = node;
        }

        id_t* get_node(std::string_view name) const {
            for (auto scope = this; scope; scope = scope->parent_) {
                id_t* var_node = scope->get_var_node(name);
                if (var_node)
                    return static_cast<id_t*>(var_node);
            }
            return nullptr;
        }
    };

    /* ----------------------------------------------------- */

    class node_scope_t final : public node_statement_t,
                               public scope_base_t {

    public:
        node_scope_t(const location_t& loc, scope_base_t* parent)
        : node_statement_t(loc), scope_base_t(parent) {} 

        void execute(execute_params_t& params) override {
            process_statements([](auto statements, auto& params) { statements->execute(params); }, params);

            if (return_expr_) {
                params.return_result = return_expr_->execute(params);
                params.is_return = true;
            }

            memory_table_t::clear_memory(params.buf);
        }

        void analyze(analyze_params_t& params) override {
            through_statements([&params](auto statement) { statement->analyze(params); });
            process_statements([](auto statements, auto& params) { statements->analyze(params); }, params);

            if (return_expr_) {
                params.return_result = return_expr_->analyze(params);
                params.is_return = true;
            }

            memory_table_t::clear_memory(params.buf);
        }

        node_statement_t* copy(buffer_t* buf, scope_base_t* parent) const override {
            return copy_impl<node_scope_t, node_statement_t>(node_loc_t::loc(), buf, parent);
        }

        void set_unpredict() override {
            set_unpredict_impl();
        }
    };

    /* ----------------------------------------------------- */

    class node_scope_r_t final : public node_expression_t,
                                 public scope_base_t {

    public:
        node_scope_r_t(const location_t& loc, scope_base_t* parent)
        : node_expression_t(loc), scope_base_t(parent) {}

        value_t execute(execute_params_t& params) override {
            process_statements([](auto statement, auto& params) { statement->execute(params); }, params);

            if (params.is_return) {
                params.is_return = false;
                return params.return_result;
            }

            value_t result = return_expr_->execute(params);
            memory_table_t::clear_memory(params.buf);
            return result;
        }

        analyze_t analyze(analyze_params_t& params) override {
            through_statements([&params](auto statement) { statement->analyze(params); });
            process_statements([](auto statement, auto& params) { statement->analyze(params); }, params);

            if (params.is_return) {
                params.is_return = false;
                return params.return_result;
            }

            if (!return_expr_)
                throw error_type_deduction_t{node_loc_t::loc(), params.program_str,
                                             "missing required return statement"};;

            analyze_t result = return_expr_->analyze(params);
            memory_table_t::clear_memory(params.buf);
            return result;
        }

        node_expression_t* copy(buffer_t* buf, scope_base_t* parent) const override {
            return copy_impl<node_scope_r_t, node_expression_t>(node_loc_t::loc(), buf, parent);
        }

        void set_unpredict() override {
            set_unpredict_impl();
        }
    };

    class node_number_t final : public node_simple_type_t {
        int number_;

    public:
        node_number_t(const location_t& loc, int number) : node_simple_type_t(loc), number_(number) {}

        value_t execute(execute_params_t& params) override {
            return {node_type_e::NUMBER, this};
        }

        analyze_t analyze(analyze_params_t& params) override {
            return {node_type_e::NUMBER, this};
        }

        void print(execute_params_t& params) override { *(params.os) << number_ << '\n'; }

        int get_value() const noexcept { return number_; }
    
        node_expression_t* copy(buffer_t* buf, scope_base_t* parent) const override {
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

        node_expression_t* copy(buffer_t* buf, scope_base_t* parent) const override {
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

        node_expression_t* copy(buffer_t* buf, scope_base_t* parent) const override {
            return buf->add_node<node_input_t>(node_loc_t::loc());
        }
    };

    /* ----------------------------------------------------- */

    class node_indexes_t final : public node_t,
                                 public node_loc_t {
        std::vector<node_expression_t*> indexes_;

    private:
        template <typename ElemT, typename FuncT, typename ParamsT>
        std::vector<ElemT> process_indexes(FuncT&& func, ParamsT& params) const {
            std::vector<ElemT> indexes(indexes_.size());
            std::transform(indexes_.begin(), indexes_.end(), indexes.begin(),
                [&func, &params](const auto& index_) {
                   return std::forward<FuncT>(func)(index_, params);
                }
            );
            reverse(indexes.begin(), indexes.end());
            return indexes;
        }

    public:
        node_indexes_t(const location_t& loc) : node_loc_t(loc) {}

        void add_index(node_expression_t* index) { assert(index); indexes_.push_back(index); }

        std::vector<value_t> execute(execute_params_t& params) const {
            return process_indexes<value_t>(
                [](auto index_, execute_params_t& params) {
                    return index_->execute(params);
                },
                params
            );
        }

        std::vector<int> execute2ints(execute_params_t& params) const {
            return process_indexes<int>(
                [](auto index_, execute_params_t& params) {
                    value_t index = index_->execute(params);
                    return static_cast<node_number_t*>(index.value)->get_value();
                },
                params
            );
        }

        std::vector<analyze_t> analyze(analyze_params_t& params) const {
            return process_indexes<analyze_t>(
                [](auto index_, analyze_params_t& params) {
                    analyze_t result = index_->analyze(params);
                    expect_types_ne(result.result.type, node_type_e::ARRAY, index_->loc(), params);
                    expect_types_ne(result.result.type, node_type_e::UNDEF, index_->loc(), params);
                    return result;
                },
                params
            );
        }

        node_indexes_t* copy(buffer_t* buf, scope_base_t* parent) const {
            node_indexes_t* node_indexes = buf->add_node<node_indexes_t>(node_loc_t::loc());
            std::ranges::for_each(indexes_, [node_indexes, buf, parent](auto index) {
                node_indexes->add_index(index->copy(buf, parent));
            });
            return node_indexes;
        }

        location_t get_index_loc(int index) const {
            return indexes_[index]->loc();
        }
    };

    /* ----------------------------------------------------- */

    using array_execute_data_t = std::pair<std::vector<value_t  >, bool>; // vals, is_in_heap
    using array_analyze_data_t = std::pair<std::vector<analyze_t>, bool>; // vals, is_in_heap
    class node_array_values_t {
    public:
        virtual array_execute_data_t execute(execute_params_t& params) const = 0;
        virtual array_analyze_data_t analyze(analyze_params_t& params) = 0;
        virtual int get_level() const = 0;
        virtual node_array_values_t* copy_vals(buffer_t* buf, scope_base_t* parent) const = 0;
        virtual ~node_array_values_t() = default;
    };

    /* ----------------------------------------------------- */

    class node_array_value_t : public node_t,
                               public node_loc_t {
    public:
        node_array_value_t(const location_t& loc) : node_loc_t(loc) {}
        virtual void add_value_execute(std::vector<value_t  >& values, execute_params_t& params) const = 0;
        virtual void add_value_analyze(std::vector<analyze_t>& values, analyze_params_t& params) = 0;
        virtual node_array_value_t* copy_val(buffer_t* buf, scope_base_t* parent) const = 0;
    };

    /* ----------------------------------------------------- */

    class node_expression_value_t : public node_array_value_t {
        node_expression_t* value_;

    private:
        template <typename ElemT, typename FuncT, typename ParamsT>
        void process_value(std::vector<ElemT>& values, FuncT&& func, ParamsT& params) const {
            values.push_back(std::forward<FuncT>(func)(value_, params));
        }

    public:
        node_expression_value_t(const location_t& loc, node_expression_t* value)
        : node_array_value_t(loc), value_(value) { assert(value_); }

        void add_value_execute(std::vector<value_t>& values, execute_params_t& params) const override {
            process_value(values, [](auto value, auto& params) { return value->execute(params); }, params);
        }

        void add_value_analyze(std::vector<analyze_t>& values, analyze_params_t& params) override {
            process_value(values, [](auto value, auto& params) { return value->analyze(params); }, params);
        }

        node_array_value_t* copy_val(buffer_t* buf, scope_base_t* parent) const override {
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
        template <typename ElemT, typename FuncT, typename ParamsT>
        void process_add_value(std::vector<ElemT>& values, FuncT&& func, ParamsT& params) const {
            std::vector<ElemT> result = std::forward<FuncT>(func)(params).first;
            values.insert(values.end(), result.begin(), result.end());
        }

        template <typename DataT, typename FuncT, typename ParamsT, typename EvalFuncT>
        DataT process_array(FuncT&& func, ParamsT& params, EvalFuncT&& eval_func) const {
            auto  count = std::forward<EvalFuncT>(eval_func)(count_, params);
            auto& count_value = [&]() -> auto& {
                if constexpr (std::is_same_v<DataT, array_execute_data_t>)
                    return count;
                else
                    return count.result;
            }();

            if constexpr (std::is_same_v<DataT, array_analyze_data_t>) {
                if (count_value.type == node_type_e::INPUT) {
                    auto init_value = std::forward<EvalFuncT>(eval_func)(value_, params);
                    return {{init_value.result}, true};
                }
                expect_types_ne(count_value.type, node_type_e::UNDEF, count_->loc(), params);
                expect_types_ne(count_value.type, node_type_e::ARRAY, count_->loc(), params);
            }

            int real_count = static_cast<node_number_t*>(count_value.value)->get_value();
            check_size_out(real_count, params.program_str);

            std::vector<typename DataT::first_type::value_type> values;
            values.reserve(real_count);
            std::forward<FuncT>(func)(
                values, real_count, params, std::forward<EvalFuncT>(eval_func)(value_, params)
            );

            return {values, count_value.type == node_type_e::INPUT};
        }

        void check_size_out(int size, std::string_view program_str) const {
            if (size <= 0)
                throw error_execute_t{count_->loc(), program_str,
                                        "wrong input size of repeat: \"" + std::to_string(size) + '\"'
                                      + ", less then 0"};
        }

    public:
        node_repeat_values_t(const location_t& loc, node_expression_t* value, node_expression_t* count)
        : node_array_value_t(loc), value_(value), count_(count) {
            assert(value_);
            assert(count_);
        }

        void add_value_execute(std::vector<value_t>& values, execute_params_t& params) const override {
            process_add_value(values, [&](auto& params) { return execute(params); }, params);
        }

        void add_value_analyze(std::vector<analyze_t>& values, analyze_params_t& params) override {
            process_add_value(values, [&](auto& params) { return analyze(params); }, params);
        }

        array_execute_data_t execute(execute_params_t& params) const override {
            return process_array<array_execute_data_t>(
                [&](auto& values, int real_count, execute_params_t& params, value_t value) {
                    std::generate_n(std::back_inserter(values), real_count, [&]() {
                        auto* copy_val = static_cast<node_type_t*>(value.value->copy(params.buf, nullptr));
                        return value_t{value.type, copy_val};
                    });
                },
                params,
                [](auto expr, auto& params) { return expr->execute(params); }
            );
        }
        
        array_analyze_data_t analyze(analyze_params_t& params) override {
            return process_array<array_analyze_data_t>(
                [&](auto& values, int real_count, analyze_params_t&, analyze_t init_value) {
                    level_ = init_value.result.value->level();
                    values.assign(real_count, init_value.result);
                },
                params,
                [](auto expr, auto& params) { return expr->analyze(params); }
            );
        }

        node_array_values_t* copy_vals(buffer_t* buf, scope_base_t* parent) const override {
            return buf->add_node<node_repeat_values_t>(node_loc_t::loc(), value_->copy(buf, parent),
                                                       count_->copy(buf, parent));
        }

        node_array_value_t* copy_val(buffer_t* buf, scope_base_t* parent) const override {
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
        template <typename DataT, typename FuncT, typename ParamsT>
        DataT process_values(FuncT&& func, ParamsT& params) const {
            std::vector<typename DataT::first_type::value_type> values;
            std::ranges::for_each(values_, [&](auto value) {
                std::forward<FuncT>(func)(value, values, params);
            });
            return {values, false};
        }

        void level_analyze(const std::vector<analyze_t>& a_values, analyze_params_t& params) {
            bool is_setted = false;
            std::ranges::for_each(a_values, [&](auto a_value) {
                node_type_t* value = a_value.result.value;
                int elem_level = value->level();

                if (!is_setted) {
                    level_ = elem_level;
                    is_setted = true;
                    return;
                }

                if (level_ != elem_level)
                    throw error_analyze_t{value->loc(), params.program_str,
                                          "different type in array"};
            });
        }

    public:
        node_list_values_t(const location_t& loc) : node_loc_t(loc) {}

        array_execute_data_t execute(execute_params_t& params) const override {
            return process_values<array_execute_data_t>(
                [](auto value, auto& values, auto& params) { value->add_value_execute(values, params); },
                params
            );
        }

        void add_value(node_array_value_t* value) { assert(value); values_.push_back(value); }

        array_analyze_data_t analyze(analyze_params_t& params) override {
            auto data = process_values<array_analyze_data_t>(
                [](auto value, auto& values, auto& params) { value->add_value_analyze(values, params); },
                params
            );
            level_analyze(data.first, params);
            return data;
        }

        node_array_values_t* copy_vals(buffer_t* buf, scope_base_t* parent) const override {
            node_list_values_t* node_values = buf->add_node<node_list_values_t>(node_loc_t::loc());
            std::ranges::for_each(values_, [node_values, buf, parent](auto value) {
                node_values->add_value(value->copy_val(buf, parent));
            });
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
        std::vector<analyze_t> a_values_;

        std::vector<value_t>   e_indexes_;
        std::vector<analyze_t> a_indexes_;

        bool is_in_heap_ = false;
        bool is_freed_   = false;

    private:
        template <typename DataT, typename FuncT, typename ParamsT>
        void init(FuncT&& eval_func, ParamsT& params,
                  std::vector<typename DataT::first_type::value_type>& values,
                  std::vector<typename DataT::first_type::value_type>& indexes) {
            auto values_res = std::forward<FuncT>(eval_func)(init_values_, params);
            values          = values_res.first;
            is_in_heap_     = values_res.second;
            indexes         = std::forward<FuncT>(eval_func)(init_indexes_, params);
            is_inited_      = true;
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

        static void set_unpredict_below(analyze_t& value, std::vector<analyze_t>& indexes,
                                        analyze_params_t& params,
                                        const std::vector<analyze_t>& all_indexes, int depth) {
            std::vector<analyze_t> tmp_indexes{indexes};
            analyze_t& result = shift_analyze_step(value, tmp_indexes, params, all_indexes, depth);
            result.is_constexpr = false;
        }

        void analyze_check_freed(const location_t& loc, analyze_params_t& params) const {
            if (is_freed_)
                throw error_analyze_t{loc, params.program_str,
                                      "attempt to use freed array"};
        }

        template <typename ElemT>
        location_t get_index_location(size_t depth, const std::vector<ElemT>& all_indexes) const {
            const auto& index = all_indexes[all_indexes.size() - depth - 1];

            auto [indexes, value] = [&]() {
                if constexpr (std::is_same_v<ElemT, value_t>) 
                    return std::make_tuple(e_indexes_, index.value);
                else 
                    return std::make_tuple(a_indexes_, index.result.value);
            }();

            if (depth >= indexes.size())
                return value->loc();

            return init_indexes_->get_index_loc(indexes.size() - depth - 1);
        }

        template <typename ErrorT, typename ElemT, typename ParamsT>
        void check_index_out(int index, size_t depth, 
                             const std::vector<ElemT>& all_indexes,
                             ParamsT& params) const {
            if (index < 0) {
                location_t loc = get_index_location<ElemT>(depth, all_indexes);
                throw ErrorT{loc, params.program_str,
                             "wrong index in array: \"" + std::to_string(index) + "\", less than 0"};
            }

            int array_size = std::is_same_v<ElemT, value_t> ? e_values_.size() : a_values_.size();
            if (index >= array_size && (!is_in_heap_ || !std::is_same_v<ElemT, analyze_t>)) {
                location_t loc = get_index_location<ElemT>(depth, all_indexes);
                throw ErrorT{loc, params.program_str,
                               "wrong index in array: \"" + std::to_string(index)
                             + "\", when array size: \""  + std::to_string(array_size) + "\""};
            }
        }

        value_t& shift_(std::vector<value_t>& indexes, execute_params_t& params,
                        const std::vector<value_t>& all_indexes, int depth) {
            value_t index_value = indexes.back().value->execute(params);
            node_number_t* node_index = static_cast<node_number_t*>(index_value.value);
            int index = node_index->get_value();
            indexes.pop_back();

            check_index_out<error_execute_t>(index, depth, all_indexes, params);

            value_t& result = e_values_[index];

            if (!indexes.empty() && result.type == node_type_e::ARRAY)
                return static_cast<node_array_t*>(result.value)->shift_(indexes, params,
                                                                        all_indexes, depth + 1);
            else
                return result;
        }

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
            std::ranges::for_each(a_values_, [&](auto a_value) {
                set_unpredict_below(a_value, indexes, params, all_indexes, depth + 1);
            });
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

                if (a_index.is_constexpr) {
                    check_index_out<error_analyze_t>(
                        node_index->get_value(), depth, all_indexes, params
                    );
                }
                return shift_analyze_number(indexes, node_index, params, all_indexes, depth);
            }

            return shift_analyze_unpredict(indexes, params, all_indexes, depth);
        }

        template <typename DataT, typename ParamsT>
        DataT::first_type::value_type process(ParamsT& params) {
            using ElemT = DataT::first_type::value_type;
            constexpr bool is_array_execute = std::is_same_v<ElemT, value_t>;
            constexpr bool is_array_analyze = std::is_same_v<ElemT, analyze_t>;

            if constexpr (is_array_analyze)
                analyze_check_freed(node_loc_t::loc(), params);

            auto [values, indexes] = [&]() {
                if constexpr (is_array_execute) 
                    return std::make_tuple(std::ref(e_values_), std::ref(e_indexes_));
                else 
                    return std::make_tuple(std::ref(a_values_), std::ref(a_indexes_));
            }();

            auto func = [](auto* node, auto& params) {
                if constexpr (is_array_execute)
                    return node->execute(params);
                else
                    return node->analyze(params);
            };

            if (!is_inited_)
                init<DataT>(func, params, values, indexes);

            if (!indexes.empty())
                return shift(std::vector<ElemT>{}, params);
            return {node_type_e::ARRAY, this};
        }

    public:
        node_array_t(const location_t& loc, node_array_values_t* init_values, node_indexes_t* indexes)
        : node_type_t(loc), init_values_(init_values), init_indexes_(indexes) {
            assert(init_values_);
            assert(init_indexes_);
        }

        value_t execute(execute_params_t& params) override {
            return process<array_execute_data_t>(params);
        }

        analyze_t analyze(analyze_params_t& params) override {
            return process<array_analyze_data_t>(params);
        }

        template <typename ElemT, typename ParamsT>
        ElemT& shift(const std::vector<ElemT>& ext_indexes, ParamsT& params) {
            constexpr bool is_array_execute = std::is_same_v<ElemT, value_t>;
            constexpr bool is_array_analyze = std::is_same_v<ElemT, analyze_t>;

            const auto& indexes = [&]() -> const auto& {
                if constexpr (is_array_execute)
                    return e_indexes_;
                else
                    return a_indexes_;
            }();

            std::vector<ElemT> all_indexes = ext_indexes;
            all_indexes.insert(all_indexes.end(), indexes.begin(), indexes.end());

            if constexpr (is_array_analyze)
                analyze_check_freed(all_indexes[0].result.value->loc(), params);

            if constexpr (is_array_execute)
                return shift_(all_indexes, params, std::vector<value_t>{all_indexes}, 0);
            else
                return shift_analyze_(all_indexes, params, std::vector<analyze_t>{all_indexes}, 0);
        }

        void print(execute_params_t& params) override {
            if (!e_indexes_.empty()) {
                shift(std::vector<value_t>{}, params).value->print(params);
                return;
            }
            execute(params);

            std::stringstream print_stream;
            execute_params_t print_params{params.buf, &print_stream, params.is, params.program_str};

            std::ranges::for_each(e_values_, [&print_params](auto e_value) {
                e_value.value->print(print_params);
            });

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

        node_expression_t* copy(buffer_t* buf, scope_base_t* parent) const override {
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
            return array->shift(indexes, params);
        }

    public:
        settable_value_t(const location_t& loc) : node_loc_t(loc) {}

        value_t execute(node_indexes_t* indexes, execute_params_t& params) {
            assert(indexes);
            value_t& real_value = shift(indexes->execute(params), params);
            return real_value;
        }

        value_t set_value(node_indexes_t* indexes, value_t new_value,
                          execute_params_t& params) {
            assert(indexes);
            value_t& real_value = shift(indexes->execute(params), params);
            is_setted = true;
            return real_value = new_value;
        }

        analyze_t analyze(node_indexes_t* ext_indexes, analyze_params_t& params) {
            assert(ext_indexes);
            std::vector<analyze_t> indexes = ext_indexes->analyze(params);
            if (indexes.size() > 0 && !is_setted)
                throw error_analyze_t{node_loc_t::loc(), params.program_str,
                                      "attempt to indexing by not init variable"};

            return shift_analyze(indexes, params);
        }

        analyze_t set_value_analyze(node_indexes_t* ext_indexes, analyze_t new_value,
                                    analyze_params_t& params, const location_t& assign_loc) {
            assert(ext_indexes);
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
        node_variable_t* variable_;
        node_indexes_t*  indexes_;

    public:
        node_lvalue_t(const location_t& loc, node_variable_t* variable, node_indexes_t* indexes)
        : node_expression_t(loc), variable_(variable), indexes_(indexes) {
            assert(indexes_);
        }

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

        node_expression_t* copy(buffer_t* buf, scope_base_t* parent) const override {
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
        : node_expression_t(loc), lvalue_(lvalue), rvalue_(rvalue) {
            assert(lvalue_);
            assert(rvalue_);
        }

        value_t execute(execute_params_t& params) override {
            return lvalue_->set_value(rvalue_->execute(params), params);
        }

        analyze_t analyze(analyze_params_t& params) override {
            return lvalue_->set_value_analyze(rvalue_->analyze(params), params, node_loc_t::loc());
        }

        node_expression_t* copy(buffer_t* buf, scope_base_t* parent) const override {
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

    private:
        int evaluate(node_number_t* l_result, node_number_t* r_result) {
            assert(l_result);
            assert(r_result);

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
                default: throw error_t{"attempt to execute unknown binary operator"};
            }
        }

    public:
        node_bin_op_t(const location_t& loc, binary_operators_e type,
                      node_expression_t* left, node_expression_t* right)
        : node_expression_t(loc), type_(type), left_(left), right_(right) {
            assert(left);
            assert(right);
        }

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

        node_expression_t* copy(buffer_t* buf, scope_base_t* parent) const override {
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
            assert(result);
            int value = result->get_value();
            switch (type_) {
                case unary_operators_e::ADD: return  value;
                case unary_operators_e::SUB: return -value;
                case unary_operators_e::NOT: return !value;
                default: throw error_t{"attempt to execute unknown unary operator"};
            }
        }

    public:
        node_un_op_t(const location_t& loc, unary_operators_e type, node_expression_t* node)
        : node_expression_t(loc), type_(type), node_(node) { assert(node_); }

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

        node_expression_t* copy(buffer_t* buf, scope_base_t* parent) const override {
            return buf->add_node<node_un_op_t>(node_loc_t::loc(), type_, node_->copy(buf, parent));
        }

        void set_unpredict() override { node_->set_unpredict(); }
    };

    /* ----------------------------------------------------- */

    class node_print_t final : public node_expression_t {
        node_expression_t* argument_;

    public:
        node_print_t(const location_t& loc, node_expression_t* argument)
        : node_expression_t(loc), argument_(argument) { assert(argument_); }

        value_t execute(execute_params_t& params) override {
            value_t result = argument_->execute(params);
            result.value->print(params);
            return result;
        }

        analyze_t analyze(analyze_params_t& params) override {
            return argument_->analyze(params);
        }

        node_expression_t* copy(buffer_t* buf, scope_base_t* parent) const override {
            return buf->add_node<node_print_t>(node_loc_t::loc(), argument_->copy(buf, parent));
        }

        void set_unpredict() override { argument_->set_unpredict(); }
    };

    /* ----------------------------------------------------- */

    class node_loop_t final : public node_statement_t {
        node_expression_t* condition_;
        node_scope_t* body_;

    private:
        void check_step_type(node_type_e type, execute_params_t& params) const {
            if (type == node_type_e::UNDEF)
                throw error_execute_t{node_loc_t::loc(), params.program_str,
                                      "wrong type: undef, excpected int"};
        }

        int step(execute_params_t& params) {
            value_t result = condition_->execute(params);

            check_step_type(result.type, params);

            return static_cast<node_number_t*>(result.value)->get_value();
        }

        void check_condition(analyze_params_t& params) {
            analyze_t a_result = condition_->analyze(params);
            value_t     result = a_result.result;

            expect_types_ne(result.type, node_type_e::ARRAY, node_loc_t::loc(), params);
            expect_types_ne(result.type, node_type_e::UNDEF, node_loc_t::loc(), params);
        }

    public:
        node_loop_t(const location_t& loc, node_expression_t* condition, node_scope_t* body)
        : node_statement_t(loc), condition_(condition), body_(body) {
            assert(condition_);
            assert(body_);
        }

        void execute(execute_params_t& params) override {
            while (step(params))
                body_->execute(params);
        }

        void analyze(analyze_params_t& params) override {
            check_condition(params);
            body_->set_unpredict();
            body_->analyze(params);
            params.is_return = false;
        }

        node_statement_t* copy(buffer_t* buf, scope_base_t* parent) const override {
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
        : node_statement_t(loc), condition_(condition), body1_(body1), body2_(body2) {
            assert(condition_);
            assert(body1_);
            assert(body2_);
        }

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

            params.is_return = false;
        }

        node_statement_t* copy(buffer_t* buf, scope_base_t* parent) const override {
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