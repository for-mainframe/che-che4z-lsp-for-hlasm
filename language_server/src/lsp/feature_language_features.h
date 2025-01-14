/*
 * Copyright (c) 2019 Broadcom.
 * The term "Broadcom" refers to Broadcom Inc. and/or its subsidiaries.
 *
 * This program and the accompanying materials are made
 * available under the terms of the Eclipse Public License 2.0
 * which is available at https://www.eclipse.org/legal/epl-2.0/
 *
 * SPDX-License-Identifier: EPL-2.0
 *
 * Contributors:
 *   Broadcom, Inc. - initial API and implementation
 */

#ifndef HLASMPLUGIN_LANGUAGESERVER_FEATURE_LANGUAGEFEATURES_H
#define HLASMPLUGIN_LANGUAGESERVER_FEATURE_LANGUAGEFEATURES_H

#include <string>
#include <unordered_map>
#include <vector>

#include "../feature.h"
#include "../logger.h"
#include "protocol.h"
#include "workspace_manager.h"

namespace hlasm_plugin::language_server::lsp {

// a feature that implements definition, references and completion
class feature_language_features : public feature
{
public:
    feature_language_features(parser_library::workspace_manager& ws_mngr, response_provider& response_provider);

    void register_methods(std::map<std::string, method>& methods) override;
    nlohmann::json register_capabilities() override;
    void initialize_feature(const nlohmann::json& initialise_params) override;

    static nlohmann::json convert_tokens_to_num_array(const std::vector<parser_library::token_info>& tokens);

private:
    void definition(const request_id& id, const nlohmann::json& params);
    void references(const request_id& id, const nlohmann::json& params);
    void hover(const request_id& id, const nlohmann::json& params);
    void completion(const request_id& id, const nlohmann::json& params);
    void completion_resolve(const request_id& id, const nlohmann::json& params);
    void semantic_tokens(const request_id& id, const nlohmann::json& params);
    void document_symbol(const request_id& id, const nlohmann::json& params);
    void opcode_suggestion(const request_id& id, const nlohmann::json& params);
    void branch_information(const request_id& id, const nlohmann::json& params);
    void folding(const request_id& id, const nlohmann::json& params);

    nlohmann::json document_symbol_item_json(hlasm_plugin::parser_library::document_symbol_item symbol);
    nlohmann::json document_symbol_list_json(hlasm_plugin::parser_library::document_symbol_list symbol_list);

    parser_library::workspace_manager& ws_mngr_;

    nlohmann::json translate_completion_list_and_save_doc(hlasm_plugin::parser_library::completion_list list);
    std::unordered_map<std::string, std::string> saved_completion_list_doc;
};

} // namespace hlasm_plugin::language_server::lsp

#endif
