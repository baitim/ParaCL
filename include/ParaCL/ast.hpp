#pragma once

#include "node.hpp"

namespace paracl {
    inline void execute_ast(node_scope_t* root, execute_params_t& params) {
        auto& statements = params.statements;
        auto& state      = params.execute_state;

        statements.emplace(root);

        while (!statements.empty()) {
            state = execute_state_e::PROCESS;
            node_interpretable_t* statement = statements.top();
            statement->execute(params);

            switch (state) {
                case execute_state_e::PROCESS: {
                    params.erase_statement();
                    break;
                }
                case execute_state_e::RETURN: {
                    params.unroll_to_scope_r();
                    break;
                }
                default: break;
            }
        }
    }

    struct ast_t final {
        node_scope_t* root_ = nullptr;
        buffer_t buffer_;

        void execute(environments_t& env) {
            if (root_) {
                buffer_t execution_buffer;
                execute_params_t execute_params{&execution_buffer, env.os, env.is, env.program_str};
                execute_ast(root_, execute_params);
            } else {
                throw error_t{str_red("execute by nullptr")};
            }
        }

        void analyze(environments_t& env) {
            if (root_) {
                buffer_t copy_buffer;
                copy_params_t copy_params{&copy_buffer};
                node_scope_t* copy_root = static_cast<node_scope_t*>(root_->copy(copy_params, nullptr));

                buffer_t execution_buffer;
                analyze_params_t analyze_params{&execution_buffer, env.program_str};
                copy_root->analyze(analyze_params);
            } else {
                throw error_t{str_red("analyze by nullptr")};
            }
        }
    };
}