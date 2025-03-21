#pragma once

#include "ParaCL/common.hpp"
#include "ParaCL/environments.hpp"

#include <cassert>
#include <memory>
#include <ranges>
#include <sstream>
#include <stack>
#include <unordered_map>
#include <vector>

namespace paracl {
    struct location_t final {
        int row = -1;
        int col = -1;
        int len = -1;
    };

    inline std::ostream& operator<<(std::ostream& os, const location_t& location) {
        os << "location:\n";
        os << "\trow:" << location.row + 1 << "\n";
        os << "\tcol:" << location.col + 1 << "\n";
        os << "\tlen:" << location.len;
        return os;
    }

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

    class error_declaration_t : public error_location_t {
    public:
        error_declaration_t(const location_t& loc, std::string_view program_str, const std::string& msg)
        : error_location_t(loc, program_str, str_red("declaration failed: " + msg)) {}
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

    class  value_t;
    class  analyze_t;
    class  scope_base_t;
    class  execute_params_t;
    class  analyze_params_t;
    struct copy_params_t;

    class node_expression_t : public node_t,
                              public node_loc_t {
    public:
        node_expression_t(const location_t& loc) : node_loc_t(loc) {}
        virtual value_t   execute(execute_params_t& params) = 0;
        virtual analyze_t analyze(analyze_params_t& params) = 0;
        virtual void set_predict(bool value) = 0;
        virtual node_expression_t* copy(copy_params_t& params, scope_base_t* parent) const = 0;
    };

    /* ----------------------------------------------------- */

    enum class general_type_e {
        INTEGER,
        ARRAY,
        FUNCTION
    };

    inline std::string type2str(general_type_e type) {
        switch (type) {
            case general_type_e::INTEGER:  return "integer";
            case general_type_e::ARRAY:    return "array";
            case general_type_e::FUNCTION: return "function";
            default:                       return "unknown type";
        }
    }

    class node_type_t : public node_expression_t {
    public:
        node_type_t(const location_t& loc) : node_expression_t(loc) {}
        virtual void print(execute_params_t& params) = 0;
        virtual int  level() const = 0;
        void set_predict(bool value) override {}
        virtual general_type_e get_general_type() const noexcept = 0;
    };

    /* ----------------------------------------------------- */

    class node_simple_type_t : public node_type_t {
    public:
        node_simple_type_t(const location_t& loc) : node_type_t(loc) {}
        int level() const override { return 0; };
    };

    /* ----------------------------------------------------- */

    enum class node_type_e {
        INTEGER,
        UNDEF,
        ARRAY,
        INPUT,
        FUNCTION
    };

    inline std::string type2str(node_type_e type) {
        switch (type) {
            case node_type_e::INTEGER:  return "integer";
            case node_type_e::UNDEF:    return "undef";
            case node_type_e::ARRAY:    return "array";
            case node_type_e::INPUT:    return "number";
            case node_type_e::FUNCTION: return "function";
            default:                    return "unknown type";
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

    template <typename ElemT>
    class stack_t final {
        std::stack<ElemT> stack;

    public:
        void push_value(const ElemT& value) {
            stack.push(value);
        }

        ElemT pop_value() {
            if (stack.empty())
                throw error_t{str_red("stack_t: pop_value() failed: stack is empty")};

            ElemT value = stack.top();
            stack.pop();
            return value;
        }

        template <typename IterT>
        void push_values(IterT begin, IterT end) {
            for (auto it = begin; it != end; ++it)
                stack.push(*it);
        }

        std::vector<ElemT> pop_values(size_t count) {
            std::vector<ElemT> result;
            result.reserve(count);

            while (count-- > 0) {
                if (stack.empty())
                    throw error_t{str_red("stack_t: pop_value() failed: stack is empty")};

                result.push_back(stack.top());
                stack.pop();
            }

            return result;
        }

        size_t size() const {
            return stack.size();
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

    class node_statement_t : public node_t,
                             public node_loc_t {
    public:
        node_statement_t(const location_t& loc) : node_loc_t(loc) {}
        virtual void execute(execute_params_t& params) = 0;
        virtual void analyze(analyze_params_t& params) = 0;
        virtual void set_predict(bool value) = 0;
        virtual node_statement_t* copy(copy_params_t& params, scope_base_t* parent) const = 0;
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

        template <typename IterT>
        requires std::is_base_of_v<id_t,
                                   std::remove_pointer_t<typename std::iterator_traits<IterT>::value_type>>
        void add_variables(IterT begin, IterT end) {
            for (auto it = begin; it != end; ++it) {
                assert(*it);
                variables_.emplace((*it)->get_name(), *it);
            }
        }

        id_t* get_var_node(std::string_view name) const {
            auto var_iter = variables_.find(name);
            if (var_iter != variables_.end())
                return var_iter->second;
            return nullptr;
        }

        virtual ~name_table_t() = default;
    };

    /* ----------------------------------------------------- */

    struct copy_params_t final {
        buffer_t* buf = nullptr;
        name_table_t global_scope;

        copy_params_t() = default;
        copy_params_t(buffer_t* buf_) : buf(buf_) { assert(buf); }
    };

    /* ----------------------------------------------------- */
    
    class node_scope_t;

    struct execute_params_t final {
        copy_params_t copy_params;
        stack_t<value_t> stack;

        std::ostream* os = nullptr;
        std::istream* is = nullptr;
        std::string_view program_str = {};

        execute_params_t(buffer_t* buf_, std::ostream* os_,
                         std::istream* is_, std::string_view program_str_)
        : os(os_), is(is_), program_str(program_str_) {
            assert(buf_);
            assert(os);
            assert(is);
            copy_params.buf = buf_;
        }

        buffer_t* buf() { return copy_params.buf; }
    };

    /* ----------------------------------------------------- */

    struct analyze_params_t final {
        copy_params_t copy_params;
        stack_t<analyze_t> stack;

        std::string_view program_str = {};

        analyze_params_t(buffer_t* buf_, std::string_view program_str_ = {})
        : program_str(program_str_) {
            assert(buf_);
            copy_params.buf = buf_;
        }

        buffer_t* buf() { return copy_params.buf; }
    };

    /* ----------------------------------------------------- */

    inline void expect_types_eq(node_type_e result,    node_type_e expected,
                                const location_t& loc, analyze_params_t& params) {
        if (result != expected)
            throw error_analyze_t{loc, params.program_str, "wrong type: " + type2str(result)};
    }

    /* ----------------------------------------------------- */

    inline void expect_types_ne(node_type_e result,    node_type_e expected,
                                const location_t& loc, analyze_params_t& params) {
        if (result == expected)
            throw error_analyze_t{loc, params.program_str, "wrong type: " + type2str(result)};
    }

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

        node_statement_t* copy(copy_params_t& params, scope_base_t* parent) const override {
            return params.buf->add_node<node_instruction_t>(node_loc_t::loc(), expr_->copy(params, parent));
        }

        void set_predict(bool value) override { expr_->set_predict(value); };
    };
}