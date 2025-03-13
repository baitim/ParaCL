#pragma once

#include "node.hpp"

namespace paracl {
    struct ast_t final {
        node_scope_t* root_ = nullptr;
        buffer_t buffer_;

        void execute(environments_t& env) {
            if (root_) {
                buffer_t execution_buffer;
                execute_params_t params{&execution_buffer, env.os, env.is, env.program_str};
                root_->execute(params);
            } else {
                throw error_t{str_red("execute by nullptr")};
            }
        }

        void analyze(environments_t& env) {
            if (root_) {
                buffer_t copy_buffer;
                node_scope_t* copy_root =
                        static_cast<node_scope_t*>(root_->copy(&copy_buffer, nullptr));

                buffer_t execution_buffer;
                analyze_params_t params{&execution_buffer, env.program_str};
                copy_root->analyze(params);
            } else {
                throw error_t{str_red("analyze by nullptr")};
            }
        }
    };
}