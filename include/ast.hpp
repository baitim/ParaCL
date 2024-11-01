#pragma once

#include "node.hpp"

namespace ast {
    struct ast_t final {
        node::node_t* root_ = nullptr;
        void calculate() { root_->calculate(); }
        void print()     { return root_->print(); }
    };
}