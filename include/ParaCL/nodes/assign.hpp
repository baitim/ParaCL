#pragma once

#include "ParaCL/nodes/lvalue.hpp"

namespace paracl {
    class node_assign_t final : public node_expression_t {
        node_lvalue_t*     lvalue_;
        node_expression_t* rvalue_;

    public:
        node_assign_t(const location_t& loc, node_lvalue_t* lvalue, node_expression_t* rvalue)
        : node_expression_t(loc), lvalue_(lvalue), rvalue_(rvalue) {
            assert(lvalue_);
            assert(rvalue_);
        }

        execute_t execute(execute_params_t& params) override {
            execute_t rvalue_result = rvalue_->execute(params);
            if (!params.is_executed())
                return {};
            
            return lvalue_->set_value(rvalue_result, params);
        }

        analyze_t analyze(analyze_params_t& params) override {
            return lvalue_->set_value_analyze(rvalue_->analyze(params), params, node_loc_t::loc());
        }

        node_expression_t* copy(copy_params_t& params, scope_base_t* parent) const override {
            auto& buf = params.buf;
            return buf->add_node<node_assign_t>(node_loc_t::loc(),
                                                static_cast<node_lvalue_t*>(lvalue_->copy(params, parent)),
                                                rvalue_->copy(params, parent));
        }

        void set_predict(bool value) override { lvalue_->set_predict(value); }
    };
}