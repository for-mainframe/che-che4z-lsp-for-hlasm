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

#ifndef HLASMPARSER_PARSERLIBRARY_ANALYZER_H
#define HLASMPARSER_PARSERLIBRARY_ANALYZER_H

#include <atomic>
#include <memory>
#include <optional>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

#include "analyzing_context.h"
#include "compiler_options.h"
#include "diagnosable_ctx.h"
#include "preprocessor_options.h"
#include "processing/preprocessor.h"
#include "protocol.h"
#include "utils/resource_location.h"
#include "workspaces/parse_lib_provider.h"

namespace hlasm_plugin::utils {
class task;
} // namespace hlasm_plugin::utils

namespace hlasm_plugin::parser_library::context {
class hlasm_context;
} // namespace hlasm_plugin::parser_library::context

namespace hlasm_plugin::parser_library::parsing {
class hlasmparser_multiline;
} // namespace hlasm_plugin::parser_library::parsing

namespace hlasm_plugin::parser_library::processing {
class statement_analyzer;
} // namespace hlasm_plugin::parser_library::processing

namespace hlasm_plugin::parser_library::semantics {
class source_info_processor;
} // namespace hlasm_plugin::parser_library::semantics

namespace hlasm_plugin::parser_library {
struct fade_message_s;
class virtual_file_monitor;
class virtual_file_handle;

enum class collect_highlighting_info : bool
{
    no,
    yes,
};

enum class file_is_opencode : bool
{
    no,
    yes,
};

class analyzer_options
{
    utils::resource::resource_location file_loc = utils::resource::resource_location("");
    workspaces::parse_lib_provider* lib_provider = nullptr;
    std::variant<asm_option, analyzing_context> ctx_source;
    workspaces::library_data library_data = { processing::processing_kind::ORDINARY, context::id_index() };
    collect_highlighting_info collect_hl_info = collect_highlighting_info::no;
    file_is_opencode parsing_opencode = file_is_opencode::no;
    std::shared_ptr<context::id_storage> ids_init;
    std::vector<preprocessor_options> preprocessor_args;
    virtual_file_monitor* vf_monitor = nullptr;
    std::shared_ptr<std::vector<fade_message_s>> fade_messages = nullptr;

    void set(utils::resource::resource_location rl) { file_loc = std::move(rl); }
    void set(workspaces::parse_lib_provider* lp) { lib_provider = lp; }
    void set(asm_option ao) { ctx_source = std::move(ao); }
    void set(analyzing_context ac) { ctx_source = std::move(ac); }
    void set(workspaces::library_data ld) { library_data = std::move(ld); }
    void set(collect_highlighting_info hi) { collect_hl_info = hi; }
    void set(file_is_opencode f_oc) { parsing_opencode = f_oc; }
    void set(std::shared_ptr<context::id_storage> ids) { ids_init = std::move(ids); }
    void set(preprocessor_options pp) { preprocessor_args.push_back(std::move(pp)); }
    void set(std::vector<preprocessor_options> pp) { preprocessor_args = std::move(pp); }
    void set(virtual_file_monitor* vfm) { vf_monitor = vfm; }
    void set(std::shared_ptr<std::vector<fade_message_s>> fmc) { fade_messages = fmc; };

    context::hlasm_context& get_hlasm_context();
    analyzing_context& get_context();
    workspaces::parse_lib_provider& get_lib_provider() const;
    std::unique_ptr<processing::preprocessor> get_preprocessor(
        processing::library_fetcher, diagnostic_op_consumer&, semantics::source_info_processor&) const;

    friend class analyzer;

public:
    analyzer_options() = default;
    analyzer_options(analyzer_options&&) = default;

    template<typename... Args>
    explicit analyzer_options(Args&&... args)
    {
        constexpr auto rl_cnt =
            (0 + ... + std::is_convertible_v<std::decay_t<Args>, utils::resource::resource_location>);
        constexpr auto lib_cnt = (0 + ... + std::is_convertible_v<std::decay_t<Args>, workspaces::parse_lib_provider*>);
        constexpr auto ao_cnt = (0 + ... + std::is_same_v<std::decay_t<Args>, asm_option>);
        constexpr auto ac_cnt = (0 + ... + std::is_same_v<std::decay_t<Args>, analyzing_context>);
        constexpr auto lib_data_cnt = (0 + ... + std::is_same_v<std::decay_t<Args>, workspaces::library_data>);
        constexpr auto hi_cnt = (0 + ... + std::is_same_v<std::decay_t<Args>, collect_highlighting_info>);
        constexpr auto f_oc_cnt = (0 + ... + std::is_same_v<std::decay_t<Args>, file_is_opencode>);
        constexpr auto ids_cnt = (0 + ... + std::is_same_v<std::decay_t<Args>, std::shared_ptr<context::id_storage>>);
        constexpr auto pp_cnt = (0 + ... + std::is_convertible_v<std::decay_t<Args>, preprocessor_options>)+(
            0 + ... + std::is_same_v<std::decay_t<Args>, std::vector<preprocessor_options>>);
        constexpr auto vfm_cnt = (0 + ... + std::is_convertible_v<std::decay_t<Args>, virtual_file_monitor*>);
        constexpr auto fmc_cnt =
            (0 + ... + std::is_same_v<std::decay_t<Args>, std::shared_ptr<std::vector<fade_message_s>>>);
        constexpr auto cnt = rl_cnt + lib_cnt + ao_cnt + ac_cnt + lib_data_cnt + hi_cnt + f_oc_cnt + ids_cnt + pp_cnt
            + vfm_cnt + fmc_cnt;

        static_assert(rl_cnt <= 1, "Duplicate resource_location");
        static_assert(lib_cnt <= 1, "Duplicate parse_lib_provider");
        static_assert(ao_cnt <= 1, "Duplicate asm_option");
        static_assert(ac_cnt <= 1, "Duplicate analyzing_context");
        static_assert(lib_data_cnt <= 1, "Duplicate library_data");
        static_assert(hi_cnt <= 1, "Duplicate collect_highlighting_info");
        static_assert(f_oc_cnt <= 1, "Duplicate file_is_opencode");
        static_assert(ids_cnt <= 1, "Duplicate id_storage");
        static_assert(pp_cnt <= 1, "Duplicate preprocessor_args");
        static_assert(vfm_cnt <= 1, "Duplicate virtual_file_monitor");
        static_assert(fmc_cnt <= 1, "Duplicate fade message container");
        static_assert(!(ac_cnt && (ao_cnt || ids_cnt || pp_cnt)),
            "Do not specify both analyzing_context and asm_option, id_storage or preprocessor_args");
        static_assert(cnt == sizeof...(Args), "Unrecognized argument provided");

        (set(std::forward<Args>(args)), ...);
    }
};

// this class analyzes provided text and produces diagnostics and highlighting info with respect to provided context
class analyzer : public diagnosable_ctx
{
    struct impl;

    std::unique_ptr<impl> m_impl;

public:
    analyzer(std::string_view text, analyzer_options opts = {});
    ~analyzer();

    std::vector<std::pair<virtual_file_handle, utils::resource::resource_location>> take_vf_handles();
    analyzing_context context() const;

    context::hlasm_context& hlasm_ctx();
    std::vector<token_info> take_semantic_tokens();

    void analyze();
    [[nodiscard]] utils::task co_analyze() &;

    void collect_diags() const override;
    const performance_metrics& get_metrics() const;

    void register_stmt_analyzer(processing::statement_analyzer* stmt_analyzer);

    parsing::hlasmparser_multiline& parser(); // for testing only
};

} // namespace hlasm_plugin::parser_library
#endif
