#pragma once

#include "node.hpp"

namespace ast {
    struct ast_t final {
        node::node_scope_t* root_ = nullptr;
        node::buffer_t buffer_;
        node::buffer_t execution_buffer_;

        void execute() {
            if (root_)
                root_->execute(execution_buffer_);
            else
                throw node::error_t{"execute by nullptr"};
        }
    };
}