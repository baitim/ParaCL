#pragma once

#include "node.hpp"

namespace ast {
    struct ast_t final {
        node::node_t* root_ = nullptr;
        ~ast_t() {
            delete root_;
        }
    };
}