#pragma once

#include "ParaCL/nodes/common.hpp"

#include <algorithm>

namespace paracl {
    class node_indexes_t final : public node_t,
                                 public node_loc_t {
        std::vector<node_expression_t*> indexes_;

    private:
        template <typename ElemT, typename FuncT, typename ParamsT>
        std::vector<ElemT> process_indexes(FuncT&& func, ParamsT& params) const {
            std::vector<ElemT> indexes(indexes_.size());
            std::transform(indexes_.begin(), indexes_.end(), indexes.begin(),
                [&func, &params](const auto& index_) {
                   return std::invoke(func, index_, params);
                }
            );
            reverse(indexes.begin(), indexes.end());
            return indexes;
        }

    public:
        node_indexes_t(const location_t& loc) : node_loc_t(loc) {}

        void add_index(node_expression_t* index) { assert(index); indexes_.push_back(index); }

        std::vector<execute_t> execute(execute_params_t& params) const {
            return process_indexes<execute_t>(
                [](auto index_, execute_params_t& params) {
                    return index_->execute(params);
                },
                params
            );
        }

        std::vector<int> execute2ints(execute_params_t& params) const {
            return process_indexes<int>(
                [](auto index_, execute_params_t& params) {
                    execute_t index = index_->execute(params);
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

        node_indexes_t* copy(copy_params_t& params, scope_base_t* parent) const {
            node_indexes_t* node_indexes = params.buf->add_node<node_indexes_t>(node_loc_t::loc());
            std::ranges::for_each(indexes_, [node_indexes, &params, parent](auto index) {
                node_indexes->add_index(index->copy(params, parent));
            });
            return node_indexes;
        }

        location_t get_index_loc(int index) const {
            return indexes_[index]->loc();
        }

        bool empty() const noexcept { return indexes_.empty(); }
    };

    /* ----------------------------------------------------- */

    using array_execute_data_t = std::pair<std::vector<execute_t>, bool>; // vals, is_in_heap
    using array_analyze_data_t = std::pair<std::vector<analyze_t>, bool>; // vals, is_in_heap
    class node_array_values_t {
    public:
        virtual array_execute_data_t execute(execute_params_t& params) const = 0;
        virtual array_analyze_data_t analyze(analyze_params_t& params) = 0;
        virtual int get_level() const = 0;
        virtual node_array_values_t* copy_vals(copy_params_t& params, scope_base_t* parent) const = 0;
        virtual ~node_array_values_t() = default;
    };

    /* ----------------------------------------------------- */

    class node_array_value_t : public node_t,
                               public node_loc_t {
    public:
        node_array_value_t(const location_t& loc) : node_loc_t(loc) {}
        virtual void add_value_execute(std::vector<execute_t>& values, execute_params_t& params) const = 0;
        virtual void add_value_analyze(std::vector<analyze_t>& values, analyze_params_t& params) = 0;
        virtual node_array_value_t* copy_val(copy_params_t& params, scope_base_t* parent) const = 0;
    };

    /* ----------------------------------------------------- */

    class node_expression_value_t : public node_array_value_t {
        node_expression_t* value_;

    private:
        template <typename ElemT, typename FuncT, typename ParamsT>
        void process_value(std::vector<ElemT>& values, FuncT&& func, ParamsT& params) const {
            values.push_back(std::invoke(func, value_, params));
        }

    public:
        node_expression_value_t(const location_t& loc, node_expression_t* value)
        : node_array_value_t(loc), value_(value) { assert(value_); }

        void add_value_execute(std::vector<execute_t>& values, execute_params_t& params) const override {
            process_value(values, [](auto value, auto& params) { return value->execute(params); }, params);
        }

        void add_value_analyze(std::vector<analyze_t>& values, analyze_params_t& params) override {
            process_value(values, [](auto value, auto& params) { return value->analyze(params); }, params);
        }

        node_array_value_t* copy_val(copy_params_t& params, scope_base_t* parent) const override {
            return params.buf->add_node<node_expression_value_t>(node_loc_t::loc(), value_->copy(params, parent));
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
            std::vector<ElemT> result = std::invoke(func, params).first;
            values.insert(values.end(), result.begin(), result.end());
        }

        template <typename DataT, typename FuncT, typename ParamsT, typename EvalFuncT>
        DataT process_array(FuncT&& func, ParamsT& params, EvalFuncT&& eval_func) const {
            auto  count = std::invoke(eval_func, count_, params);
            auto& count_value = [&]() -> auto& {
                if constexpr (std::is_same_v<DataT, array_execute_data_t>)
                    return count;
                else
                    return count.result;
            }();

            if constexpr (std::is_same_v<DataT, array_analyze_data_t>) {
                if (count_value.type == node_type_e::INPUT) {
                    auto init_value = std::invoke(eval_func, value_, params);
                    return {{init_value.result}, true};
                }
                expect_types_ne(count_value.type, node_type_e::UNDEF, count_->loc(), params);
                expect_types_ne(count_value.type, node_type_e::ARRAY, count_->loc(), params);
            }

            int real_count = static_cast<node_number_t*>(count_value.value)->get_value();
            check_size_out(real_count, params.program_str);

            std::vector<typename DataT::first_type::value_type> values;
            values.reserve(real_count);
            std::invoke(func,
                values, real_count, params, std::invoke(eval_func, value_, params)
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

        void add_value_execute(std::vector<execute_t>& values, execute_params_t& params) const override {
            process_add_value(values, [&](auto& params) { return execute(params); }, params);
        }

        void add_value_analyze(std::vector<analyze_t>& values, analyze_params_t& params) override {
            process_add_value(values, [&](auto& params) { return analyze(params); }, params);
        }

        array_execute_data_t execute(execute_params_t& params) const override {
            return process_array<array_execute_data_t>(
                [&](auto& values, int real_count, execute_params_t& params, execute_t value) {
                    std::generate_n(std::back_inserter(values), real_count, [&]() {
                        auto* copy_val = static_cast<node_type_t*>(
                            value.value->copy(params.copy_params, nullptr)
                        );
                        return execute_t{value.type, copy_val};
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

        node_array_values_t* copy_vals(copy_params_t& params, scope_base_t* parent) const override {
            auto& buf = params.buf;
            return buf->add_node<node_repeat_values_t>(node_loc_t::loc(), value_->copy(params, parent),
                                                       count_->copy(params, parent));
        }

        node_array_value_t* copy_val(copy_params_t& params, scope_base_t* parent) const override {
            auto& buf = params.buf;
            return buf->add_node<node_repeat_values_t>(node_loc_t::loc(), value_->copy(params, parent),
                                                       count_->copy(params, parent));
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
                std::invoke(func, value, values, params);
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

        node_array_values_t* copy_vals(copy_params_t& params, scope_base_t* parent) const override {
            node_list_values_t* node_values = params.buf->add_node<node_list_values_t>(node_loc_t::loc());
            std::ranges::for_each(values_, [node_values, &params, parent](auto value) {
                node_values->add_value(value->copy_val(params, parent));
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

        std::vector<execute_t> e_values_;
        std::vector<analyze_t> a_values_;

        std::vector<execute_t> e_indexes_;
        std::vector<analyze_t> a_indexes_;

        bool is_in_heap_ = false;
        bool is_freed_   = false;

    private:
        template <typename DataT, typename FuncT, typename ParamsT>
        void init(FuncT&& eval_func, ParamsT& params,
                  std::vector<typename DataT::first_type::value_type>& values,
                  std::vector<typename DataT::first_type::value_type>& indexes) {
            auto values_res = std::invoke(eval_func, init_values_, params);
            values          = values_res.first;
            is_in_heap_     = values_res.second;
            indexes         = std::invoke(eval_func, init_indexes_, params);
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
                if constexpr (std::is_same_v<ElemT, execute_t>) 
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

            int array_size = std::is_same_v<ElemT, execute_t> ? e_values_.size() : a_values_.size();
            if (index >= array_size && (!is_in_heap_ || !std::is_same_v<ElemT, analyze_t>)) {
                location_t loc = get_index_location<ElemT>(depth, all_indexes);
                throw ErrorT{loc, params.program_str,
                               "wrong index in array: \"" + std::to_string(index)
                             + "\", when array size: \""  + std::to_string(array_size) + "\""};
            }
        }

        execute_t& shift_(std::vector<execute_t>& indexes, execute_params_t& params,
                          const std::vector<execute_t>& all_indexes, int depth) {
            execute_t index_value = indexes.back().value->execute(params);
            node_number_t* node_index = static_cast<node_number_t*>(index_value.value);
            int index = node_index->get_value();
            indexes.pop_back();

            check_index_out<error_execute_t>(index, depth, all_indexes, params);

            execute_t& result = e_values_[index];

            if (!indexes.empty() && result.type == node_type_e::ARRAY)
                return static_cast<node_array_t*>(result.value)->shift_(indexes, params,
                                                                        all_indexes, depth + 1);
            else
                return result;
        }

        static analyze_t& shift_analyze_step(analyze_t& value, std::vector<analyze_t>& indexes,
                                             analyze_params_t& params,
                                             const std::vector<analyze_t>& all_indexes, int depth) {
            execute_t result = value.result;
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
            execute_t   index = a_index.result;
            if (a_index.is_constexpr &&
                index.type == node_type_e::INTEGER) {

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
            constexpr bool is_array_execute = std::is_same_v<ElemT, execute_t>;
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

        execute_t execute(execute_params_t& params) override {
            return process<array_execute_data_t>(params);
        }

        analyze_t analyze(analyze_params_t& params) override {
            return process<array_analyze_data_t>(params);
        }

        template <typename ElemT, typename ParamsT>
        ElemT& shift(const std::vector<ElemT>& ext_indexes, ParamsT& params) {
            constexpr bool is_array_execute = std::is_same_v<ElemT, execute_t>;
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
                return shift_(all_indexes, params, std::vector<execute_t>{all_indexes}, 0);
            else
                return shift_analyze_(all_indexes, params, std::vector<analyze_t>{all_indexes}, 0);
        }

        void print(execute_params_t& params) override {
            if (!e_indexes_.empty()) {
                shift(std::vector<execute_t>{}, params).value->print(params);
                return;
            }
            execute(params);

            std::stringstream print_stream;
            execute_params_t print_params = params;
            print_params.os = &print_stream;

            std::ranges::for_each(e_values_, [&print_params](auto e_value) {
                e_value.value->print(print_params);
            });

            *(params.os) << '[' << transform_print_str(print_stream.str()) << "]\n";
        }

        void clear() {
            is_inited_ = false;
            if (is_in_heap_) {
                is_freed_ = true;
                e_values_.clear();
                a_values_.clear();
                e_indexes_.clear();
                a_indexes_.clear();
            }
        }

        node_expression_t* copy(copy_params_t& params, scope_base_t* parent) const override {
            auto& buf = params.buf;
            node_array_t* node_array = buf->add_node<node_array_t>(node_loc_t::loc(),
                                                                   init_values_->copy_vals(params, parent),
                                                                   init_indexes_->copy(params, parent));
            if (parent)
                parent->add_array(node_array);
            return node_array;
        }

        int level() const override { return 1 + init_values_->get_level(); }
    };
}