#pragma once

#include "ParaCL/common.hpp"
#include "ParaCL/environments.hpp"

#include <algorithm>
#include <cassert>
#include <memory>
#include <ranges>
#include <sstream>
#include <stack>
#include <unordered_map>
#include <unordered_set>
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
        int line = -1;
        for ([[maybe_unused]]int _ : std::views::iota(0, loc.row))
            line = program_str.find('\n', line + 1);

        if (line > 0)
            line++;
        else
            line = 0;

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

    class  execute_t;
    class  analyze_t;
    class  scope_base_t;
    class  execute_params_t;
    class  analyze_params_t;
    struct copy_params_t;

    class node_expression_t : public node_t,
                              public node_loc_t {
    public:
        node_expression_t(const location_t& loc) : node_loc_t(loc) {}
        virtual execute_t execute(execute_params_t& params) = 0;
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
            case node_type_e::INPUT:    return "number";
            case node_type_e::ARRAY:    return "array";
            case node_type_e::FUNCTION: return "function";
            default:                    return "unknown type";
        }
    }

    inline general_type_e to_general_type(node_type_e type) {
        switch (type) {
            case node_type_e::INTEGER:  return general_type_e::INTEGER;
            case node_type_e::UNDEF:    return general_type_e::INTEGER;
            case node_type_e::INPUT:    return general_type_e::INTEGER;
            case node_type_e::ARRAY:    return general_type_e::ARRAY;
            case node_type_e::FUNCTION: return general_type_e::FUNCTION;
            default:                    throw error_t{str_red("failed to_general_type(): unknown type")};
        }
    }

    struct execute_t final {
        node_type_e  type;
        node_type_t* value = nullptr;
    };

    /* ----------------------------------------------------- */

    template <typename ElemT>
    class stack_t final : public std::stack<ElemT> {
    public:
        using std::stack<ElemT>::top;
        using std::stack<ElemT>::pop;
        using std::stack<ElemT>::emplace;
        using std::stack<ElemT>::size;
        using std::stack<ElemT>::empty;

        template <std::input_iterator IterT>
        void push_values(IterT begin, IterT end) {
            for (auto it = begin; it != end; ++it)
                emplace(*it);
        }

        ElemT pop_value() {
            if (empty())
                    throw error_t{str_red("stack_t: pop_value() failed: stack is empty")};

            auto&& result = std::move(top());
            pop();
            return result;
        }

        std::vector<ElemT> pop_values(size_t count) {
            std::vector<ElemT> result;
            result.reserve(count);

            while (count-- > 0) {
                if (empty())
                    throw error_t{str_red("stack_t: pop_values() failed: stack is empty")};

                result.push_back(top());
                pop();
            }

            return result;
        }
    };

    /* ----------------------------------------------------- */

    struct analyze_t final {
        node_type_e  type;
        node_type_t* value = nullptr;
        bool         is_constexpr = true;

    public:
        analyze_t() {}
        analyze_t(bool is_constexpr_) : is_constexpr(is_constexpr_) {}
        analyze_t(node_type_e type_, node_type_t* value_) : type(type_), value(value_) { assert(value); }
        analyze_t(node_type_e type_, node_type_t* value_, int is_constexpr_)
        : type(type_), value(value_), is_constexpr(is_constexpr_) { assert(value); }

        explicit analyze_t(execute_t e_value, int is_constexpr_)
        : type(e_value.type), value(e_value.value), is_constexpr(is_constexpr_) { assert(value); }
    };

    /* ----------------------------------------------------- */

    class node_interpretable_t : public node_t,
                                 public node_loc_t {
    public:
        node_interpretable_t(const location_t& loc) : node_loc_t(loc) {}
        virtual void execute(execute_params_t& params) = 0;
    };

    /* ----------------------------------------------------- */

    class node_statement_t : public node_interpretable_t {
    public:
        node_statement_t(const location_t& loc) : node_interpretable_t(loc) {}
        virtual void analyze(analyze_params_t& params) = 0;
        virtual void set_predict(bool value) = 0;
        virtual node_statement_t* copy(copy_params_t& params, scope_base_t* parent) const = 0;
    };

    /* ----------------------------------------------------- */

    class node_settable_t {
    public:
        virtual execute_t execute(execute_params_t& params) = 0;
        virtual execute_t set_value(execute_t new_value, execute_params_t& params) = 0;
        virtual ~node_settable_t() = default;
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

    class node_empty_interpretable_t : public node_interpretable_t {
    public:
        node_empty_interpretable_t(const location_t& loc) : node_interpretable_t(loc) {}
        void execute(execute_params_t& params) override {}
    };

    inline node_interpretable_t* make_empty_interpretable(const location_t& loc, copy_params_t& params) {
        return params.buf->add_node<node_empty_interpretable_t>(loc);
    }

    /* ----------------------------------------------------- */

    class names_visitor_t {
        static constexpr const int default_key_value = -1;
        std::unordered_map<std::string_view, std::pair<id_t*, int>> names;

    public:
        bool is_name_visited(id_t* id) const {
            return (names.find(id->get_name()) != names.end());
        }

        void visit_name(id_t* id, int key = default_key_value) {
            names.emplace(id->get_name(), std::make_pair(id, key));
        }

        void unvisit_name(id_t* id, int key = default_key_value) {
            auto iter = names.find(id->get_name());
            assert(iter != names.end());
            if (iter->second.second == key)
                names.erase(iter);
        }

        virtual ~names_visitor_t() = default;
    };

    /* ----------------------------------------------------- */

    class node_scope_t;

    enum class execute_state_e {
        PROCESS,
        RETURN,
        ADDED_STATEMENTS
    };

    class execute_params_t final : public names_visitor_t {
        using     values_container_t = std::unordered_map<int, std::unordered_map<node_t*, execute_t>>;
        using    visited_container_t = std::unordered_map<int, std::unordered_map<node_t*, int>>;
        using  variables_container_t = std::unordered_map<int, std::unordered_map<node_settable_t*, execute_t>>;
            values_container_t values;
           visited_container_t visits;
         variables_container_t variables;
        std::vector<int> return_receivers;
        int step = 0;

    public:
        std::ostream* os = nullptr;
        std::istream* is = nullptr;
        std::string_view program_str = {};

        copy_params_t copy_params;

        execute_state_e execute_state = execute_state_e::PROCESS;

        stack_t<execute_t> stack;
        stack_t<node_interpretable_t*> statements;

        bool is_visiting_prev = false;

    private:
        void update_step() {
            step = statements.size();

            if (!return_receivers.empty() && return_receivers.back() == step)
                return_receivers.pop_back();
        }

    public:
        execute_params_t(buffer_t* buf_, std::ostream* os_, std::istream* is_, std::string_view program_str_)
        : os(os_), is(is_), program_str(program_str_) {
            assert(buf_);
            assert(os);
            assert(is);
            copy_params.buf = buf_;
        }

        int get_step() const noexcept { return step; }

        bool is_executed() const noexcept { return execute_state == execute_state_e::PROCESS; }

        template <typename IterT>
        void upload_variables(IterT begin, IterT end) {
            auto& step_iter = variables[step];
            std::ranges::for_each(begin, end, [&](auto arg) {
                step_iter.emplace(arg, arg->execute(*this));
            });
        }

        template <typename IterT>
        void load_variables(IterT begin, IterT end) {
            auto& step_iter = variables[step - 1];
            std::ranges::for_each(begin, end, [&](auto arg) {
                auto variable_iter = step_iter.find(arg);
                arg->set_value(variable_iter->second, *this);
            });
        }

        std::optional<execute_t> get_evaluated(node_t* node) const {
            if (auto step_it = values.find(step); step_it != values.end())
                if (auto loc_it = step_it->second.find(node); loc_it != step_it->second.end())
                    return loc_it->second;
            return std::nullopt;
        }

        template <typename... ArgsT>
        execute_t add_value(node_t* node, ArgsT&&... args) {
            auto result = values[step].emplace(node, execute_t(std::forward<ArgsT>(args)...));
            return result.first->second;
        }

        void add_return(execute_t value) {
            stack.emplace(value);
            execute_state = execute_state_e::RETURN;
        }

        void visit(node_t* node) {
            auto& step_map = visits[step - is_visiting_prev];
            auto [it, inserted] = step_map.emplace(node, 1);
            if (!inserted)
                it->second++;
        }

        bool is_visited(node_t* node) const {
            if (auto step_it = visits.find(step - is_visiting_prev); step_it != visits.end())
                if (step_it->second.find(node) != step_it->second.end())
                    return true;
            return false;
        }     

        int number_of_visit(node_t* node) const {
            auto step_it = visits.find(step);
            if (step_it != visits.end()) {
                auto var_it = step_it->second.find(node);
                if (var_it != step_it->second.end())
                    return var_it->second;
            }
            return 0;
        }

        template <std::input_iterator IterT>
        void insert_statements(IterT begin, IterT end) {
            statements.push_values(begin, end);
            execute_state = execute_state_e::ADDED_STATEMENTS;
            update_step();
        }

        void insert_statement(node_interpretable_t* statement) {
            statements.emplace(statement);
            execute_state = execute_state_e::ADDED_STATEMENTS;
            update_step();
        }     

        void erase_statement() {
            statements.pop();
            values.erase(step);
            visits.erase(step);
            variables.erase(step);
            update_step();
        }

        template <typename MapT>
        void shift_step(MapT& map, int old_step, int new_step) {
            if (auto it = map.find(old_step); it != map.end()) {
                map[new_step] = std::move(it->second);
                map.erase(it);
            }
        }

        void insert_statement_before(node_interpretable_t* statement) {
            auto* last_statement = statements.pop_value();
            statements.emplace(statement);
            statements.emplace(last_statement);
            int old_step = step;
            update_step();
            int new_step = step;

            shift_step(values,    old_step, new_step);
            shift_step(variables, old_step, new_step);

            if (!return_receivers.empty())
                if (auto& last = return_receivers.back(); last == old_step)
                    last = new_step;

            execute_state = execute_state_e::ADDED_STATEMENTS;
        }

        void erase_statement_before() {
            auto* last_statement = statements.pop_value();
            statements.pop();
            statements.emplace(last_statement);
            int old_step = step;
            update_step();
            int new_step = step;
            values.erase(new_step);
            visits.erase(new_step);
            variables.erase(new_step);

            shift_step(values,    old_step, new_step);
            shift_step(visits,    old_step, new_step);
            shift_step(variables, old_step, new_step);

            if (!return_receivers.empty())
                if (auto& last = return_receivers.back(); last == old_step)
                    last = new_step;
        }

        void add_return_receiver() {
            return_receivers.push_back(step);
        }

        void on_return() {
            assert(!return_receivers.empty());
            int last_return_receiver = return_receivers.back();
            while (step > last_return_receiver)
                erase_statement();
        }

        buffer_t* buf() { return copy_params.buf; }
    };

    /* ----------------------------------------------------- */

    enum class analyze_state_e {
        PROCESS,
        RETURN
    };

    struct analyze_params_t final : public names_visitor_t {
        std::string_view program_str = {};

        copy_params_t copy_params;

        analyze_state_e analyze_state = analyze_state_e::PROCESS;

        stack_t<analyze_t> stack;

    public:
        analyze_params_t(buffer_t* buf_, std::string_view program_str_ = {})
        : program_str(program_str_) {
            assert(buf_);
            copy_params.buf = buf_;
        }

        buffer_t* buf() { return copy_params.buf; }
    };

    /* ----------------------------------------------------- */

    template <typename T>
    concept existed_types = std::same_as<T, node_type_e> || std::same_as<T, general_type_e>;

    template <typename T>
    concept existed_params = std::same_as<T, execute_params_t> || std::same_as<T, analyze_params_t>;

    template <existed_types TypeT, existed_params ParamsT>
    inline void expect_types_eq(TypeT result, TypeT expected, const location_t& loc, ParamsT& params) {
        if (result == expected)
            return;

        if constexpr (std::is_same_v<ParamsT, execute_params_t>)
            throw error_execute_t{loc, params.program_str, "wrong type: " + type2str(result)};
        else
            throw error_analyze_t{loc, params.program_str, "wrong type: " + type2str(result)};
    }

    template <existed_types TypeT, existed_params ParamsT>
    inline void expect_types_ne(TypeT result, TypeT expected, const location_t& loc, ParamsT& params) {
        if (result != expected)
            return;

        if constexpr (std::is_same_v<ParamsT, execute_params_t>)
            throw error_execute_t{loc, params.program_str, "wrong type: " + type2str(result)};
        else
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