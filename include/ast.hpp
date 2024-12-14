#pragma once

#include "node.hpp"

namespace ast {
    struct ast_t final {
        node::node_scope_t* root_ = nullptr;
        node::buffer_t buffer_;

        void execute(environments::environments_t& env) {
            if (root_) {
                node::buffer_t execution_buffer;
                root_->execute(execution_buffer, env);
            } else {
                throw common::error_t{str_red("execute by nullptr")};
            }
        }

        void analyze(environments::environments_t& env) {
            if (root_) {
                node::buffer_t copy_buffer;
                node::node_scope_t* copy_root =
                        static_cast<node::node_scope_t*>(root_->copy(copy_buffer, nullptr));

                node::buffer_t execution_buffer;
                copy_root->analyze(execution_buffer, env);
            } else {
                throw common::error_t{str_red("analyze by nullptr")};
            }
        }
    };
}