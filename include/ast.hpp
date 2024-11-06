#pragma once

#include "node.hpp"

namespace ast {
    struct ast_t final {
        node::node_t* root_ = nullptr;
        node::buffer_t buffer_;

        int execute() {
            if (root_)
                return root_->execute();
            throw "execute by nullptr";
        }
    };
}