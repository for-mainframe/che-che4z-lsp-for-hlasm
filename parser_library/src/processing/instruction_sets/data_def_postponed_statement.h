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

#ifndef HLASMPLUGIN_PARSERLIBRARY_PROCESSING_DATA_DEF_POSTPONED_STATEMENT_H
#define HLASMPLUGIN_PARSERLIBRARY_PROCESSING_DATA_DEF_POSTPONED_STATEMENT_H

#include "checking/data_definition/data_def_type_base.h"
#include "context/ordinary_assembly/dependable.h"
#include "context/ordinary_assembly/dependency_solver_redirect.h"
#include "postponed_statement_impl.h"

namespace hlasm_plugin::parser_library::processing {

template<checking::data_instr_type instr_type>
class data_def_dependency final : public context::resolvable
{
    const semantics::operand_ptr* m_begin;
    const semantics::operand_ptr* m_end;
    context::address m_loctr;

public:
    data_def_dependency(const semantics::operand_ptr* b, const semantics::operand_ptr* e, context::address loctr)
        : m_begin(b)
        , m_end(e)
        , m_loctr(std::move(loctr))
    {}

    static int32_t get_operands_length(const semantics::operand_ptr* b,
        const semantics::operand_ptr* e,
        context::dependency_solver& _solver,
        diagnostic_op_consumer& diags,
        const context::address* loctr = nullptr);

    // Inherited via resolvable
    context::dependency_collector get_dependencies(context::dependency_solver& solver) const override;

    context::symbol_value resolve(context::dependency_solver& solver) const override;
};

template<checking::data_instr_type instr_type>
class data_def_postponed_statement final : public postponed_statement_impl
{
    std::vector<data_def_dependency<instr_type>> m_dependencies;

public:
    data_def_postponed_statement(rebuilt_statement stmt,
        context::processing_stack_t stmt_location_stack,
        std::vector<data_def_dependency<instr_type>> dependencies);

    const std::vector<data_def_dependency<instr_type>>& get_dependencies() const { return m_dependencies; }
};

struct data_def_dependency_solver final : public context::dependency_solver_redirect
{
    data_def_dependency_solver(context::dependency_solver& base, const context::address* loctr)
        : dependency_solver_redirect(base)
        , loctr(loctr)
    {}

    const context::address* loctr;
    uint64_t operands_bit_length = 0;
    std::optional<context::address> get_loctr() const override;
};

} // namespace hlasm_plugin::parser_library::processing
#endif
