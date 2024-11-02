#pragma once

#include "node.hpp"

namespace ast {
    struct ast_t final {
        node::node_t* root_ = nullptr;
        int execute() { return root_->execute(); }
    };
}