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

#include "asm_processor.h"

#include <charconv>
#include <memory>
#include <optional>

#include "analyzing_context.h"
#include "checking/diagnostic_collector.h"
#include "checking/instr_operand.h"
#include "context/common_types.h"
#include "context/hlasm_context.h"
#include "context/literal_pool.h"
#include "context/ordinary_assembly/location_counter.h"
#include "context/ordinary_assembly/ordinary_assembly_dependency_solver.h"
#include "context/ordinary_assembly/symbol_dependency_tables.h"
#include "data_def_postponed_statement.h"
#include "diagnosable_ctx.h"
#include "ebcdic_encoding.h"
#include "expressions/mach_expr_term.h"
#include "expressions/mach_expr_visitor.h"
#include "postponed_statement_impl.h"
#include "processing/branching_provider.h"
#include "processing/opencode_provider.h"
#include "processing/processing_manager.h"
#include "processing/statement.h"
#include "processing/statement_fields_parser.h"
#include "range.h"
#include "semantics/operand_impls.h"
#include "utils/string_operations.h"
#include "utils/unicode_text.h"
#include "workspaces/parse_lib_provider.h"

namespace hlasm_plugin::parser_library::processing {

namespace {
std::optional<context::A_t> try_get_abs_value(
    const semantics::simple_expr_operand* op, context::dependency_solver& dep_solver)
{
    if (op->has_dependencies(dep_solver, nullptr))
        return std::nullopt;

    auto val = op->expression->evaluate(dep_solver, drop_diagnostic_op);

    if (val.value_kind() != context::symbol_value_kind::ABS)
        return std::nullopt;
    return val.get_abs();
}

std::optional<context::A_t> try_get_abs_value(const semantics::operand* op, context::dependency_solver& dep_solver)
{
    auto expr_op = dynamic_cast<const semantics::simple_expr_operand*>(op);
    if (!expr_op)
        return std::nullopt;
    return try_get_abs_value(expr_op, dep_solver);
}

std::optional<int> try_get_number(std::string_view s)
{
    int v = 0;
    const char* b = s.data();
    const char* e = b + s.size();
    if (auto ec = std::from_chars(b, e, v); ec.ec == std::errc {} && ec.ptr == e)
        return v;
    return std::nullopt;
}

} // namespace

void asm_processor::process_sect(const context::section_kind kind, rebuilt_statement stmt)
{
    auto sect_name = find_label_symbol(stmt);

    using context::section_kind;
    const auto do_other_private_sections_exist = [this](context::id_index sect_name, section_kind kind) {
        for (auto k : { section_kind::COMMON, section_kind::EXECUTABLE, section_kind::READONLY })
        {
            if (k == kind)
                continue;
            if (hlasm_ctx.ord_ctx.section_defined(sect_name, k))
                return true;
        }
        return false;
    };

    if (!sect_name.empty() && hlasm_ctx.ord_ctx.symbol_defined(sect_name)
            && !hlasm_ctx.ord_ctx.section_defined(sect_name, kind)
        || sect_name.empty() && kind != section_kind::DUMMY && do_other_private_sections_exist(sect_name, kind))
    {
        add_diagnostic(diagnostic_op::error_E031("symbol", stmt.label_ref().field_range));
    }
    else
    {
        auto sym_loc = hlasm_ctx.processing_stack_top().get_location();
        sym_loc.pos.column = 0;
        hlasm_ctx.ord_ctx.set_section(sect_name, kind, std::move(sym_loc), lib_info);
    }
    context::ordinary_assembly_dependency_solver dep_solver(hlasm_ctx.ord_ctx, lib_info);
    hlasm_ctx.ord_ctx.symbol_dependencies().add_dependency(
        std::make_unique<postponed_statement_impl>(std::move(stmt), hlasm_ctx.processing_stack()),
        std::move(dep_solver).derive_current_dependency_evaluation_context(),
        lib_info);
}

void asm_processor::process_LOCTR(rebuilt_statement stmt)
{
    auto loctr_name = find_label_symbol(stmt);

    if (loctr_name.empty())
        add_diagnostic(diagnostic_op::error_E053(stmt.label_ref().field_range));

    if (hlasm_ctx.ord_ctx.symbol_defined(loctr_name) && !hlasm_ctx.ord_ctx.counter_defined(loctr_name))
    {
        add_diagnostic(diagnostic_op::error_E031("symbol", stmt.label_ref().field_range));
    }
    else
    {
        auto sym_loc = hlasm_ctx.processing_stack_top().get_location();
        sym_loc.pos.column = 0;
        hlasm_ctx.ord_ctx.set_location_counter(loctr_name, std::move(sym_loc), lib_info);
    }
    context::ordinary_assembly_dependency_solver dep_solver(hlasm_ctx.ord_ctx, lib_info);
    hlasm_ctx.ord_ctx.symbol_dependencies().add_dependency(
        std::make_unique<postponed_statement_impl>(std::move(stmt), hlasm_ctx.processing_stack()),
        std::move(dep_solver).derive_current_dependency_evaluation_context(),
        lib_info);
}

struct override_symbol_candidates final : public context::dependency_solver_redirect
{
    std::variant<const context::symbol*, context::symbol_candidate> get_symbol_candidate(
        context::id_index name) const override
    {
        if (auto r = dependency_solver_redirect::get_symbol_candidate(name);
            std::holds_alternative<const context::symbol*>(r) && std::get<const context::symbol*>(r) == nullptr)
            return context::symbol_candidate { false };
        else
            return r;
    }

    explicit override_symbol_candidates(context::dependency_solver& solver)
        : dependency_solver_redirect(solver)
    {}
};

void asm_processor::process_EQU(rebuilt_statement stmt)
{
    auto loctr = hlasm_ctx.ord_ctx.align(context::no_align, lib_info);
    context::ordinary_assembly_dependency_solver dep_solver(hlasm_ctx.ord_ctx, loctr, lib_info);

    auto symbol_name = find_label_symbol(stmt);

    if (symbol_name.empty())
    {
        if (stmt.label_ref().type == semantics::label_si_type::EMPTY)
            add_diagnostic(diagnostic_op::error_E053(stmt.label_ref().field_range));
        return;
    }

    if (hlasm_ctx.ord_ctx.symbol_defined(symbol_name))
    {
        add_diagnostic(diagnostic_op::error_E031("symbol", stmt.label_ref().field_range));
        return;
    }

    const auto& ops = stmt.operands_ref().value;

    if (ops.empty() || ops.size() > 5)
    {
        add_diagnostic(diagnostic_op::error_A012_from_to("EQU", 1, 5, stmt.stmt_range_ref()));
        return;
    }

    // type attribute operand
    context::symbol_attributes::type_attr t_attr = context::symbol_attributes::undef_type;
    if (ops.size() >= 3 && ops[2]->type == semantics::operand_type::ASM)
    {
        auto asm_op = ops[2]->access_asm();
        auto expr_op = asm_op->access_expr();

        override_symbol_candidates dep_solver_override(dep_solver);
        if (expr_op && !expr_op->has_dependencies(dep_solver_override, nullptr))
        {
            auto t_value = expr_op->expression->evaluate(dep_solver_override, *this);
            if (t_value.value_kind() == context::symbol_value_kind::ABS && t_value.get_abs() >= 0
                && t_value.get_abs() <= 255)
                t_attr = (context::symbol_attributes::type_attr)t_value.get_abs();
            else
                add_diagnostic(diagnostic_op::error_A134_EQU_type_att_format(asm_op->operand_range));
        }
        else
            add_diagnostic(diagnostic_op::error_A134_EQU_type_att_format(asm_op->operand_range));
    }

    // length attribute operand
    context::symbol_attributes::len_attr length_attr = context::symbol_attributes::undef_length;
    if (ops.size() >= 2 && ops[1]->type == semantics::operand_type::ASM)
    {
        auto asm_op = ops[1]->access_asm();
        auto expr_op = asm_op->access_expr();

        override_symbol_candidates dep_solver_override(dep_solver);
        if (expr_op && !expr_op->has_dependencies(dep_solver_override, nullptr))
        {
            auto length_value = expr_op->expression->evaluate(dep_solver_override, *this);
            if (length_value.value_kind() == context::symbol_value_kind::ABS && length_value.get_abs() >= 0
                && length_value.get_abs() <= 65535)
                length_attr = (context::symbol_attributes::len_attr)length_value.get_abs();
            else
                add_diagnostic(diagnostic_op::error_A133_EQU_len_att_format(asm_op->operand_range));
        }
        else
            add_diagnostic(diagnostic_op::error_A133_EQU_len_att_format(asm_op->operand_range));
    }

    // value operand
    if (ops[0]->type != semantics::operand_type::ASM)
        add_diagnostic(diagnostic_op::error_A132_EQU_value_format(ops[0]->operand_range));
    else if (auto expr_op = ops[0]->access_asm()->access_expr(); !expr_op)
        add_diagnostic(diagnostic_op::error_A132_EQU_value_format(ops[0]->operand_range));
    else
    {
        auto holder(expr_op->expression->get_dependencies(dep_solver));

        if (length_attr == context::symbol_attributes::undef_length)
        {
            auto l_term = expr_op->expression->leftmost_term();
            if (auto symbol_term = dynamic_cast<const expressions::mach_expr_symbol*>(l_term))
            {
                auto len_symbol = hlasm_ctx.ord_ctx.get_symbol(symbol_term->value);

                if (len_symbol != nullptr && len_symbol->kind() != context::symbol_value_kind::UNDEF)
                    length_attr = len_symbol->attributes().length();
                else
                    length_attr = 1;
            }
            else
                length_attr = 1;
        }

        context::symbol_attributes attrs(context::symbol_origin::EQU, t_attr, length_attr);

        if (!holder.contains_dependencies())
            create_symbol(stmt.stmt_range_ref(), symbol_name, expr_op->expression->evaluate(dep_solver, *this), attrs);

        else if (holder.is_address() && holder.unresolved_spaces.empty())
            create_symbol(stmt.stmt_range_ref(), symbol_name, *holder.unresolved_address, attrs);
        else if (const auto& stmt_range = stmt.stmt_range_ref();
                 create_symbol(stmt_range, symbol_name, context::symbol_value(), attrs))
        {
            if (!hlasm_ctx.ord_ctx.symbol_dependencies().add_dependency(symbol_name,
                    expr_op->expression.get(),
                    std::make_unique<postponed_statement_impl>(std::move(stmt), hlasm_ctx.processing_stack()),
                    std::move(dep_solver).derive_current_dependency_evaluation_context(),
                    lib_info))
                add_diagnostic(diagnostic_op::error_E033(stmt_range));
        }
    }
}

template<checking::data_instr_type instr_type>
void asm_processor::process_data_instruction(rebuilt_statement stmt)
{
    if (const auto& ops = stmt.operands_ref().value; ops.empty()
        || std::any_of(
            ops.begin(), ops.end(), [](const auto& op) { return op->type == semantics::operand_type::EMPTY; }))
    {
        context::ordinary_assembly_dependency_solver dep_solver(hlasm_ctx.ord_ctx, lib_info);
        hlasm_ctx.ord_ctx.symbol_dependencies().add_dependency(
            std::make_unique<postponed_statement_impl>(std::move(stmt), hlasm_ctx.processing_stack()),
            dep_solver.derive_current_dependency_evaluation_context(),
            lib_info);
        return;
    }

    // enforce alignment of the first operand
    context::alignment al = stmt.operands_ref().value.front()->access_data_def()->value->get_alignment();
    context::address loctr = hlasm_ctx.ord_ctx.align(al, lib_info);
    context::ordinary_assembly_dependency_solver dep_solver(hlasm_ctx.ord_ctx, loctr, lib_info);

    // process label
    auto label = find_label_symbol(stmt);

    if (!label.empty())
    {
        bool length_has_self_reference = false;
        bool scale_has_self_reference = false;

        const auto has_deps = [label](auto deps, bool& self_ref) {
            if (!deps.contains_dependencies())
                return false;
            struct
            {
                bool operator()(const context::symbolic_reference& l, const context::id_index& r) const
                {
                    return l.name < r;
                }
                bool operator()(const context::id_index& l, const context::symbolic_reference& r) const
                {
                    return l < r.name;
                }
            } cmp;
            self_ref = std::binary_search(deps.undefined_symbolics.begin(), deps.undefined_symbolics.end(), label, cmp);
            return true;
        };
        if (!hlasm_ctx.ord_ctx.symbol_defined(label))
        {
            auto data_op = stmt.operands_ref().value.front()->access_data_def();

            context::symbol_attributes::type_attr type =
                ebcdic_encoding::to_ebcdic((unsigned char)data_op->value->get_type_attribute());

            context::symbol_attributes::len_attr len = context::symbol_attributes::undef_length;
            context::symbol_attributes::scale_attr scale = context::symbol_attributes::undef_scale;

            if (!data_op->value->length
                || !has_deps(data_op->value->length->get_dependencies(dep_solver), length_has_self_reference))
            {
                len = data_op->value->get_length_attribute(dep_solver, drop_diagnostic_op);
            }
            if (data_op->value->scale
                && !has_deps(data_op->value->scale->get_dependencies(dep_solver), scale_has_self_reference))
            {
                scale = data_op->value->get_scale_attribute(dep_solver, drop_diagnostic_op);
            }
            create_symbol(stmt.stmt_range_ref(),
                label,
                loctr,
                context::symbol_attributes(context::symbol_origin::DAT,
                    type,
                    len,
                    scale,
                    data_op->value->get_integer_attribute(dep_solver, drop_diagnostic_op)));

            if (length_has_self_reference
                && !data_op->value->length->get_dependencies(dep_solver).contains_dependencies())
                hlasm_ctx.ord_ctx.get_symbol(label)->set_length(
                    data_op->value->get_length_attribute(dep_solver, drop_diagnostic_op));
            if (scale_has_self_reference
                && !data_op->value->scale->get_dependencies(dep_solver).contains_dependencies())
                hlasm_ctx.ord_ctx.get_symbol(label)->set_scale(
                    data_op->value->get_scale_attribute(dep_solver, drop_diagnostic_op));
        }
        else
            add_diagnostic(diagnostic_op::error_E031("symbol", stmt.label_ref().field_range));
    }

    const auto& operands = stmt.operands_ref().value;

    const context::resolvable* l_dep = nullptr;
    const context::resolvable* s_dep = nullptr;
    if (!label.empty())
    {
        auto data_op = operands.front()->access_data_def();

        if (data_op->value->length && data_op->value->length->get_dependencies(dep_solver).contains_dependencies())
            l_dep = data_op->value->length.get();

        if (data_op->value->scale && data_op->value->scale->get_dependencies(dep_solver).contains_dependencies())
            s_dep = data_op->value->scale.get();
    }

    // TODO issue warning when alignment is bigger than section's alignment
    // hlasm_ctx.ord_ctx.current_section()->current_location_counter().

    std::vector<data_def_dependency<instr_type>> dependencies;
    std::vector<context::space_ptr> dependencies_spaces;

    // Why is this so complicated?
    // 1. We cannot represent the individual operands because of bitfields.
    // 2. We cannot represent the whole area as a single dependency when the alignment requirements are growing.
    // Therefore, we split the operands into chunks depending on the alignment.
    // Whenever the alignment requirement increases between consecutive operands, we start a new chunk.
    for (auto it = operands.begin(); it != operands.end();)
    {
        const auto start = it;

        const auto initial_alignment = (*it)->access_data_def()->value->get_alignment();
        context::address op_loctr = hlasm_ctx.ord_ctx.align(initial_alignment, lib_info);
        data_def_dependency_solver op_solver(dep_solver, &op_loctr);

        auto current_alignment = initial_alignment;

        // has_length_dependencies specifies whether the length of the data instruction can be resolved right now or
        // must be postponed
        bool has_length_dependencies = false;

        for (; it != operands.end(); ++it)
        {
            const auto& op = *it;

            auto data_op = op->access_data_def();
            auto op_align = data_op->value->get_alignment();

            // leave for the next round to make sure that the actual alignment is computed correctly
            if (op_align.boundary > current_alignment.boundary)
                break;
            current_alignment = op_align;

            has_length_dependencies |= data_op->get_length_dependencies(op_solver).contains_dependencies();

            // some types require operands that consist only of one symbol
            (void)data_op->value->check_single_symbol_ok(diagnostic_collector(this));
        }

        const auto* const b = std::to_address(start);
        const auto* const e = std::to_address(it);

        if (has_length_dependencies)
        {
            dependencies.emplace_back(b, e, std::move(op_loctr));
            dependencies_spaces.emplace_back(hlasm_ctx.ord_ctx.register_ordinary_space(current_alignment));
        }
        else
        {
            auto length = data_def_dependency<instr_type>::get_operands_length(b, e, op_solver, drop_diagnostic_op);
            hlasm_ctx.ord_ctx.reserve_storage_area(length, context::no_align, lib_info);
        }
    }

    auto dep_stmt = std::make_unique<data_def_postponed_statement<instr_type>>(
        std::move(stmt), hlasm_ctx.processing_stack(), std::move(dependencies));
    const auto& deps = dep_stmt->get_dependencies();

    auto adder = hlasm_ctx.ord_ctx.symbol_dependencies().add_dependencies(
        std::move(dep_stmt), std::move(dep_solver).derive_current_dependency_evaluation_context(), lib_info);
    adder.add_dependency();

    bool cycle_ok = true;

    if (l_dep)
        cycle_ok &= adder.add_dependency(label, context::data_attr_kind::L, l_dep);
    if (s_dep)
        cycle_ok &= adder.add_dependency(label, context::data_attr_kind::S, s_dep);

    if (!cycle_ok)
        add_diagnostic(diagnostic_op::error_E033(operands.front()->operand_range));

    auto sp = dependencies_spaces.begin();
    for (const auto& d : deps)
        adder.add_dependency(std::move(*sp++), &d);

    adder.finish();
}

void asm_processor::process_DC(rebuilt_statement stmt)
{
    process_data_instruction<checking::data_instr_type::DC>(std::move(stmt));
}

void asm_processor::process_DS(rebuilt_statement stmt)
{
    process_data_instruction<checking::data_instr_type::DS>(std::move(stmt));
}

void asm_processor::process_COPY(rebuilt_statement stmt)
{
    find_sequence_symbol(stmt);

    if (stmt.operands_ref().value.size() == 1 && stmt.operands_ref().value.front()->type == semantics::operand_type::ASM
        && stmt.operands_ref().value.front()->access_asm()->access_expr())
    {
        if (auto extract = extract_copy_id(stmt, this); extract.has_value())
        {
            if (ctx.hlasm_ctx->copy_members().contains(extract->name))
                common_copy_postprocess(true, *extract, *ctx.hlasm_ctx, this);
            else
            {
                branch_provider.request_external_processing(extract->name,
                    processing_kind::COPY,
                    [extract, this](bool result) { common_copy_postprocess(result, *extract, *ctx.hlasm_ctx, this); });
            }
        }
    }
    else
    {
        context::ordinary_assembly_dependency_solver dep_solver(hlasm_ctx.ord_ctx, lib_info);
        hlasm_ctx.ord_ctx.symbol_dependencies().add_dependency(
            std::make_unique<postponed_statement_impl>(std::move(stmt), hlasm_ctx.processing_stack()),
            std::move(dep_solver).derive_current_dependency_evaluation_context(),
            lib_info);
    }
}

void asm_processor::process_EXTRN(rebuilt_statement stmt) { process_external(std::move(stmt), external_type::strong); }

void asm_processor::process_WXTRN(rebuilt_statement stmt) { process_external(std::move(stmt), external_type::weak); }

void asm_processor::process_external(rebuilt_statement stmt, external_type t)
{
    if (auto label_type = stmt.label_ref().type; label_type != semantics::label_si_type::EMPTY)
    {
        if (label_type != semantics::label_si_type::SEQ)
            add_diagnostic(diagnostic_op::warning_A249_sequence_symbol_expected(stmt.label_ref().field_range));
        else
            find_sequence_symbol(stmt);
    }

    const auto add_external = [s_kind = t == external_type::strong ? context::section_kind::EXTERNAL
                                                                   : context::section_kind::WEAK_EXTERNAL,
                                  this](context::id_index name, range op_range) {
        if (hlasm_ctx.ord_ctx.symbol_defined(name))
            add_diagnostic(diagnostic_op::error_E031("external symbol", op_range));
        else
            hlasm_ctx.ord_ctx.create_external_section(
                name, s_kind, hlasm_ctx.current_statement_location(), hlasm_ctx.processing_stack());
    };
    for (const auto& op : stmt.operands_ref().value)
    {
        auto op_asm = op->access_asm();
        if (!op_asm)
            continue;

        if (auto expr = op_asm->access_expr())
        {
            if (auto sym = dynamic_cast<const expressions::mach_expr_symbol*>(expr->expression.get()))
                add_external(sym->value, expr->operand_range);
        }
        else if (auto complex = op_asm->access_complex())
        {
            if (utils::to_upper_copy(complex->value.identifier) != "PART")
                continue;
            for (const auto& nested : complex->value.values)
            {
                if (const auto* string_val =
                        dynamic_cast<const semantics::complex_assembler_operand::string_value_t*>(nested.get());
                    string_val && !string_val->value.empty())
                    add_external(hlasm_ctx.ids().add(string_val->value), string_val->op_range);
            }
        }
    }
    context::ordinary_assembly_dependency_solver dep_solver(hlasm_ctx.ord_ctx, lib_info);
    hlasm_ctx.ord_ctx.symbol_dependencies().add_dependency(
        std::make_unique<postponed_statement_impl>(std::move(stmt), hlasm_ctx.processing_stack()),
        std::move(dep_solver).derive_current_dependency_evaluation_context(),
        lib_info);
}

void asm_processor::process_ORG(rebuilt_statement stmt)
{
    find_sequence_symbol(stmt);

    auto label = find_label_symbol(stmt);
    auto loctr = hlasm_ctx.ord_ctx.align(context::no_align, lib_info);

    if (!label.empty())
    {
        if (hlasm_ctx.ord_ctx.symbol_defined(label))
            add_diagnostic(diagnostic_op::error_E031("symbol", stmt.label_ref().field_range));
        else
            create_symbol(stmt.stmt_range_ref(), label, loctr, context::symbol_attributes::make_org_attrs());
    }

    const auto& ops = stmt.operands_ref().value;

    if (ops.empty()
        || (ops.size() == 2 && ops[0]->type == semantics::operand_type::EMPTY
            && ops[1]->type == semantics::operand_type::EMPTY))
    {
        hlasm_ctx.ord_ctx.set_available_location_counter_value(lib_info);
        return;
    }

    context::ordinary_assembly_dependency_solver dep_solver(hlasm_ctx.ord_ctx, loctr, lib_info);

    const semantics::expr_assembler_operand* reloc_expr = nullptr;
    size_t boundary = 0;
    int offset = 0;


    for (size_t i = 0; i < ops.size(); ++i)
    {
        if (ops[i]->type != semantics::operand_type::ASM)
            continue;

        auto asm_op = ops[i]->access_asm();
        assert(asm_op);
        auto expr = asm_op->access_expr();
        if (!expr)
        {
            if (i != 0)
                add_diagnostic(diagnostic_op::error_A115_ORG_op_format(stmt.stmt_range_ref()));
            break;
        }

        if (i == 0)
        {
            reloc_expr = expr;
        }

        if (i == 1)
        {
            auto val = try_get_abs_value(expr, dep_solver);
            if (!val || *val < 2 || *val > 4096 || ((*val & (*val - 1)) != 0)) // check range and test for power of 2
            {
                add_diagnostic(diagnostic_op::error_A116_ORG_boundary_operand(stmt.stmt_range_ref()));
                return;
            }
            boundary = (size_t)*val;
        }
        if (i == 2)
        {
            auto val = try_get_abs_value(expr, dep_solver);
            if (!val)
            {
                add_diagnostic(diagnostic_op::error_A115_ORG_op_format(stmt.stmt_range_ref()));
                return;
            }
            offset = *val;
        }
    }

    if (!reloc_expr)
    {
        add_diagnostic(diagnostic_op::error_A245_ORG_expression(stmt.stmt_range_ref()));
        return;
    }

    context::address reloc_val;
    auto deps = reloc_expr->expression->get_dependencies(dep_solver);
    bool undefined_absolute_part = !deps.undefined_symbolics.empty() || !deps.unresolved_spaces.empty();

    if (!undefined_absolute_part)
    {
        if (auto res = reloc_expr->expression->evaluate(dep_solver, drop_diagnostic_op);
            res.value_kind() == context::symbol_value_kind::RELOC)
            reloc_val = std::move(res).get_reloc();
        else
        {
            add_diagnostic(diagnostic_op::error_A245_ORG_expression(stmt.stmt_range_ref()));
            return;
        }
    }
    else
    {
        if (deps.unresolved_address)
            reloc_val = std::move(*deps.unresolved_address);
        else
            reloc_val = loctr;
    }

    switch (check_address_for_ORG(reloc_val, loctr, boundary, offset))
    {
        case check_org_result::valid:
            break;

        case check_org_result::underflow:
            add_diagnostic(diagnostic_op::error_E068(stmt.stmt_range_ref()));
            return;

        case check_org_result::invalid_address:
            add_diagnostic(diagnostic_op::error_A115_ORG_op_format(stmt.stmt_range_ref()));
            return;
    }

    if (undefined_absolute_part)
        hlasm_ctx.ord_ctx.set_location_counter_value(reloc_val,
            boundary,
            offset,
            reloc_expr->expression.get(),
            std::make_unique<postponed_statement_impl>(std::move(stmt), hlasm_ctx.processing_stack()),
            std::move(dep_solver).derive_current_dependency_evaluation_context(),
            lib_info);
    else
        hlasm_ctx.ord_ctx.set_location_counter_value(reloc_val, boundary, offset, lib_info);

    if (boundary > 1 && offset == 0)
    {
        hlasm_ctx.ord_ctx.align(context::alignment { 0, boundary }, lib_info);
    }
}

void asm_processor::process_OPSYN(rebuilt_statement stmt)
{
    const auto& operands = stmt.operands_ref().value;

    auto label = find_label_symbol(stmt);
    if (label.empty())
    {
        if (stmt.label_ref().type == semantics::label_si_type::EMPTY)
            add_diagnostic(diagnostic_op::error_E053(stmt.label_ref().field_range));
        return;
    }

    context::id_index operand;
    if (operands.size() == 1) // covers also the " , " case
    {
        auto asm_op = operands.front()->access_asm();
        if (asm_op)
        {
            auto expr_op = asm_op->access_expr();
            if (expr_op)
            {
                if (auto expr = dynamic_cast<const expressions::mach_expr_symbol*>(expr_op->expression.get()))
                    operand = expr->value;
            }
        }
    }

    if (operand.empty())
    {
        if (hlasm_ctx.get_operation_code(label))
            hlasm_ctx.remove_mnemonic(label);
        else
            add_diagnostic(diagnostic_op::error_E049(label.to_string_view(), stmt.label_ref().field_range));
    }
    else
    {
        if (hlasm_ctx.get_operation_code(operand))
            hlasm_ctx.add_mnemonic(label, operand);
        else
            add_diagnostic(diagnostic_op::error_A246_OPSYN(operands.front()->operand_range));
    }

    context::ordinary_assembly_dependency_solver dep_solver(hlasm_ctx.ord_ctx, lib_info);
    hlasm_ctx.ord_ctx.symbol_dependencies().add_dependency(
        std::make_unique<postponed_statement_impl>(std::move(stmt), hlasm_ctx.processing_stack()),
        std::move(dep_solver).derive_current_dependency_evaluation_context(),
        lib_info);
}

asm_processor::asm_processor(analyzing_context ctx,
    branching_provider& branch_provider,
    workspaces::parse_lib_provider& lib_provider,
    statement_fields_parser& parser,
    opencode_provider& open_code,
    const processing_manager& proc_mgr)
    : low_language_processor(ctx, branch_provider, lib_provider, parser, proc_mgr)
    , table_(create_table())
    , open_code_(&open_code)
{}

void asm_processor::process(std::shared_ptr<const processing::resolved_statement> stmt)
{
    auto rebuilt_stmt = preprocess(stmt);

    register_literals(rebuilt_stmt, context::no_align, hlasm_ctx.ord_ctx.next_unique_id());

    auto it = table_.find(rebuilt_stmt.opcode_ref().value);
    if (it != table_.end())
    {
        auto& [key, func] = *it;
        func(std::move(rebuilt_stmt));
    }
    else
    {
        context::ordinary_assembly_dependency_solver dep_solver(hlasm_ctx.ord_ctx, lib_info);
        hlasm_ctx.ord_ctx.symbol_dependencies().add_dependency(
            std::make_unique<postponed_statement_impl>(std::move(rebuilt_stmt), hlasm_ctx.processing_stack()),
            std::move(dep_solver).derive_current_dependency_evaluation_context(),
            lib_info);
    }
}
std::optional<asm_processor::extract_copy_id_result> asm_processor::extract_copy_id(
    const semantics::complete_statement& stmt, diagnosable_ctx* diagnoser)
{
    if (stmt.operands_ref().value.size() != 1 || !stmt.operands_ref().value.front()->access_asm()
        || !stmt.operands_ref().value.front()->access_asm()->access_expr())
    {
        if (diagnoser)
            diagnoser->add_diagnostic(diagnostic_op::error_E058(stmt.operands_ref().field_range));
        return {};
    }

    auto& expr = stmt.operands_ref().value.front()->access_asm()->access_expr()->expression;
    auto sym_expr = dynamic_cast<const expressions::mach_expr_symbol*>(expr.get());

    if (!sym_expr)
    {
        if (diagnoser)
            diagnoser->add_diagnostic(diagnostic_op::error_E058(stmt.operands_ref().value.front()->operand_range));
        return {};
    }

    return asm_processor::extract_copy_id_result {
        sym_expr->value,
        stmt.operands_ref().value.front()->operand_range,
        stmt.stmt_range_ref(),
    };
}

bool asm_processor::common_copy_postprocess(
    bool processed, const extract_copy_id_result& data, context::hlasm_context& hlasm_ctx, diagnosable_ctx* diagnoser)
{
    if (!processed)
    {
        if (diagnoser)
            diagnoser->add_diagnostic(diagnostic_op::error_E058(data.operand));
        return false;
    }

    auto whole_copy_stack = hlasm_ctx.whole_copy_stack();

    auto cycle_tmp = std::find(whole_copy_stack.begin(), whole_copy_stack.end(), data.name);

    if (cycle_tmp != whole_copy_stack.end())
    {
        if (diagnoser)
            diagnoser->add_diagnostic(diagnostic_op::error_E062(data.statement));
        return false;
    }

    hlasm_ctx.enter_copy_member(data.name);

    return true;
}

asm_processor::process_table_t asm_processor::create_table()
{
    process_table_t table;
    table.emplace(context::id_index("CSECT"),
        [this](rebuilt_statement stmt) { process_sect(context::section_kind::EXECUTABLE, std::move(stmt)); });
    table.emplace(context::id_index("DSECT"),
        [this](rebuilt_statement stmt) { process_sect(context::section_kind::DUMMY, std::move(stmt)); });
    table.emplace(context::id_index("RSECT"),
        [this](rebuilt_statement stmt) { process_sect(context::section_kind::READONLY, std::move(stmt)); });
    table.emplace(context::id_index("COM"),
        [this](rebuilt_statement stmt) { process_sect(context::section_kind::COMMON, std::move(stmt)); });
    table.emplace(context::id_index("LOCTR"), [this](rebuilt_statement stmt) { process_LOCTR(std::move(stmt)); });
    table.emplace(context::id_index("EQU"), [this](rebuilt_statement stmt) { process_EQU(std::move(stmt)); });
    table.emplace(context::id_index("DC"), [this](rebuilt_statement stmt) { process_DC(std::move(stmt)); });
    table.emplace(context::id_index("DS"), [this](rebuilt_statement stmt) { process_DS(std::move(stmt)); });
    table.emplace(
        context::id_storage::well_known::COPY, [this](rebuilt_statement stmt) { process_COPY(std::move(stmt)); });
    table.emplace(context::id_index("EXTRN"), [this](rebuilt_statement stmt) { process_EXTRN(std::move(stmt)); });
    table.emplace(context::id_index("WXTRN"), [this](rebuilt_statement stmt) { process_WXTRN(std::move(stmt)); });
    table.emplace(context::id_index("ORG"), [this](rebuilt_statement stmt) { process_ORG(std::move(stmt)); });
    table.emplace(context::id_index("OPSYN"), [this](rebuilt_statement stmt) { process_OPSYN(std::move(stmt)); });
    table.emplace(context::id_index("AINSERT"), [this](rebuilt_statement stmt) { process_AINSERT(std::move(stmt)); });
    table.emplace(context::id_index("CCW"), [this](rebuilt_statement stmt) { process_CCW(std::move(stmt)); });
    table.emplace(context::id_index("CCW0"), [this](rebuilt_statement stmt) { process_CCW(std::move(stmt)); });
    table.emplace(context::id_index("CCW1"), [this](rebuilt_statement stmt) { process_CCW(std::move(stmt)); });
    table.emplace(context::id_index("CNOP"), [this](rebuilt_statement stmt) { process_CNOP(std::move(stmt)); });
    table.emplace(context::id_index("START"), [this](rebuilt_statement stmt) { process_START(std::move(stmt)); });
    table.emplace(context::id_index("ALIAS"), [this](rebuilt_statement stmt) { process_ALIAS(std::move(stmt)); });
    table.emplace(context::id_index("END"), [this](rebuilt_statement stmt) { process_END(std::move(stmt)); });
    table.emplace(context::id_index("LTORG"), [this](rebuilt_statement stmt) { process_LTORG(std::move(stmt)); });
    table.emplace(context::id_index("USING"), [this](rebuilt_statement stmt) { process_USING(std::move(stmt)); });
    table.emplace(context::id_index("DROP"), [this](rebuilt_statement stmt) { process_DROP(std::move(stmt)); });
    table.emplace(context::id_index("PUSH"), [this](rebuilt_statement stmt) { process_PUSH(std::move(stmt)); });
    table.emplace(context::id_index("POP"), [this](rebuilt_statement stmt) { process_POP(std::move(stmt)); });
    table.emplace(context::id_index("MNOTE"), [this](rebuilt_statement stmt) { process_MNOTE(std::move(stmt)); });
    table.emplace(context::id_index("CXD"), [this](rebuilt_statement stmt) { process_CXD(std::move(stmt)); });
    table.emplace(context::id_index("TITLE"), [this](rebuilt_statement stmt) { process_TITLE(std::move(stmt)); });

    return table;
}

context::id_index asm_processor::find_sequence_symbol(const rebuilt_statement& stmt)
{
    semantics::seq_sym symbol;
    switch (stmt.label_ref().type)
    {
        case semantics::label_si_type::SEQ:
            symbol = std::get<semantics::seq_sym>(stmt.label_ref().value);
            branch_provider.register_sequence_symbol(symbol.name, symbol.symbol_range);
            return symbol.name;
        default:
            return context::id_index();
    }
}

namespace {
class AINSERT_operand_visitor final : public expressions::mach_expr_visitor
{
public:
    // Inherited via mach_expr_visitor
    void visit(const expressions::mach_expr_constant&) override {}
    void visit(const expressions::mach_expr_data_attr&) override {}
    void visit(const expressions::mach_expr_data_attr_literal&) override {}
    void visit(const expressions::mach_expr_symbol& expr) override { value = expr.value; }
    void visit(const expressions::mach_expr_location_counter&) override {}
    void visit(const expressions::mach_expr_default&) override {}
    void visit(const expressions::mach_expr_literal&) override {}

    context::id_index value;
};
} // namespace

void asm_processor::process_AINSERT(rebuilt_statement stmt)
{
    static constexpr std::string_view AINSERT = "AINSERT";
    const auto& ops = stmt.operands_ref();

    if (ops.value.size() != 2)
    {
        add_diagnostic(diagnostic_op::error_A011_exact(AINSERT, 2, ops.field_range));
        return;
    }

    auto second_op = dynamic_cast<const semantics::expr_assembler_operand*>(ops.value[1].get());
    if (!second_op)
    {
        add_diagnostic(diagnostic_op::error_A156_AINSERT_second_op_format(ops.value[1]->operand_range));
        return;
    }

    AINSERT_operand_visitor visitor;
    second_op->expression->apply(visitor);
    const auto& [value] = visitor;

    if (value.empty())
        return;
    processing::ainsert_destination dest;
    if (value.to_string_view() == "FRONT")
        dest = processing::ainsert_destination::front;
    else if (value.to_string_view() == "BACK")
        dest = processing::ainsert_destination::back;
    else
    {
        add_diagnostic(diagnostic_op::error_A156_AINSERT_second_op_format(ops.value[1]->operand_range));
        return;
    }

    if (auto arg = dynamic_cast<const semantics::string_assembler_operand*>(ops.value[0].get()))
    {
        const auto& record = arg->value;
        if (record.size() > checking::string_max_length)
        {
            add_diagnostic(diagnostic_op::error_A157_AINSERT_first_op_size(ops.value[0]->operand_range));
            return;
        }
        if (record.empty())
        {
            add_diagnostic(diagnostic_op::error_A021_cannot_be_empty(AINSERT, arg->operand_range));
            return;
        }

        open_code_->ainsert(record, dest);
    }
    else
    {
        add_diagnostic(diagnostic_op::error_A301_op_apostrophes_missing(AINSERT, ops.value[0]->operand_range));
    }
}

void asm_processor::process_CCW(rebuilt_statement stmt)
{
    constexpr context::alignment ccw_align = context::doubleword;
    constexpr size_t ccw_length = 8U;

    auto loctr = hlasm_ctx.ord_ctx.align(ccw_align, lib_info);
    context::ordinary_assembly_dependency_solver dep_solver(hlasm_ctx.ord_ctx, loctr, lib_info);
    find_sequence_symbol(stmt);

    if (auto label = find_label_symbol(stmt); !label.empty())
    {
        if (hlasm_ctx.ord_ctx.symbol_defined(label))
            add_diagnostic(diagnostic_op::error_E031("symbol", stmt.label_ref().field_range));
        else
            create_symbol(stmt.stmt_range_ref(), label, loctr, context::symbol_attributes::make_ccw_attrs());
    }

    hlasm_ctx.ord_ctx.reserve_storage_area(ccw_length, ccw_align, lib_info);

    hlasm_ctx.ord_ctx.symbol_dependencies().add_dependency(
        std::make_unique<postponed_statement_impl>(std::move(stmt), hlasm_ctx.processing_stack()),
        std::move(dep_solver).derive_current_dependency_evaluation_context(),
        lib_info);
}

void asm_processor::process_CNOP(rebuilt_statement stmt)
{
    auto loctr = hlasm_ctx.ord_ctx.align(context::halfword, lib_info);
    context::ordinary_assembly_dependency_solver dep_solver(hlasm_ctx.ord_ctx, loctr, lib_info);
    find_sequence_symbol(stmt);

    if (auto label = find_label_symbol(stmt); !label.empty())
    {
        if (hlasm_ctx.ord_ctx.symbol_defined(label))
            add_diagnostic(diagnostic_op::error_E031("symbol", stmt.label_ref().field_range));
        else
            create_symbol(stmt.stmt_range_ref(), label, loctr, context::symbol_attributes::make_cnop_attrs());
    }

    if (stmt.operands_ref().value.size() == 2)
    {
        std::optional<int> byte_value = try_get_abs_value(stmt.operands_ref().value[0].get(), dep_solver);
        std::optional<int> boundary_value = try_get_abs_value(stmt.operands_ref().value[1].get(), dep_solver);
        // For now, the implementation ignores the instruction, if the operands have dependencies. Most uses of this
        // instruction should by covered anyway. It will still generate the label correctly.
        if (byte_value.has_value() && boundary_value.has_value() && *byte_value >= 0 && *boundary_value > 0
            && ((*boundary_value) & (*boundary_value - 1)) == 0 && *byte_value < *boundary_value
            && *byte_value % 2 == 0)
            hlasm_ctx.ord_ctx.reserve_storage_area(
                0, context::alignment { (size_t)*byte_value, (size_t)*boundary_value }, lib_info);
    }

    hlasm_ctx.ord_ctx.symbol_dependencies().add_dependency(
        std::make_unique<postponed_statement_impl>(std::move(stmt), hlasm_ctx.processing_stack()),
        std::move(dep_solver).derive_current_dependency_evaluation_context(),
        lib_info);
}


void asm_processor::process_START(rebuilt_statement stmt)
{
    auto sect_name = find_label_symbol(stmt);

    if (std::any_of(hlasm_ctx.ord_ctx.sections().begin(), hlasm_ctx.ord_ctx.sections().end(), [](const auto& s) {
            return s->kind == context::section_kind::EXECUTABLE || s->kind == context::section_kind::READONLY;
        }))
    {
        add_diagnostic(diagnostic_op::error_E073(stmt.stmt_range_ref()));
        return;
    }

    if (hlasm_ctx.ord_ctx.symbol_defined(sect_name))
    {
        add_diagnostic(diagnostic_op::error_E031("symbol", stmt.label_ref().field_range));
        return;
    }

    auto sym_loc = hlasm_ctx.processing_stack_top().get_location();
    sym_loc.pos.column = 0;
    auto* section =
        hlasm_ctx.ord_ctx.set_section(sect_name, context::section_kind::EXECUTABLE, std::move(sym_loc), lib_info);

    const auto& ops = stmt.operands_ref().value;
    if (ops.size() != 1)
    {
        context::ordinary_assembly_dependency_solver dep_solver(hlasm_ctx.ord_ctx, lib_info);
        hlasm_ctx.ord_ctx.symbol_dependencies().add_dependency(
            std::make_unique<postponed_statement_impl>(std::move(stmt), hlasm_ctx.processing_stack()),
            std::move(dep_solver).derive_current_dependency_evaluation_context(),
            lib_info);
        return;
    }

    context::ordinary_assembly_dependency_solver dep_solver(hlasm_ctx.ord_ctx, lib_info);
    auto initial_offset = try_get_abs_value(ops.front().get(), dep_solver);
    if (!initial_offset.has_value())
    {
        add_diagnostic(diagnostic_op::error_A250_absolute_with_known_symbols(ops.front()->operand_range));
        return;
    }

    size_t start_section_alignment = hlasm_ctx.section_alignment().boundary;
    size_t start_section_alignment_mask = start_section_alignment - 1;

    uint32_t offset = initial_offset.value();
    if (offset & start_section_alignment_mask)
    {
        // TODO: generate informational message?
        offset += start_section_alignment_mask;
        offset &= ~start_section_alignment_mask;
    }

    section->current_location_counter().reserve_storage_area(offset, context::no_align);
}
void asm_processor::process_END(rebuilt_statement stmt)
{
    const auto& label = stmt.label_ref();
    context::ordinary_assembly_dependency_solver dep_solver(hlasm_ctx.ord_ctx, lib_info);

    if (!(label.type == semantics::label_si_type::EMPTY || label.type == semantics::label_si_type::SEQ))
    {
        add_diagnostic(diagnostic_op::warning_A249_sequence_symbol_expected(stmt.label_ref().field_range));
    }
    if (!stmt.operands_ref().value.empty() && !(stmt.operands_ref().value[0]->type == semantics::operand_type::EMPTY))
    {
        if (stmt.operands_ref().value[0]->access_asm() != nullptr
            && stmt.operands_ref().value[0]->access_asm()->kind == semantics::asm_kind::EXPR)
        {
            auto symbol = stmt.operands_ref().value[0]->access_asm()->access_expr()->expression.get()->evaluate(
                dep_solver, drop_diagnostic_op);

            if (symbol.value_kind() == context::symbol_value_kind::ABS)
            {
                add_diagnostic(
                    diagnostic_op::error_E032(std::to_string(symbol.get_abs()), stmt.operands_ref().field_range));
            }
        }
    }

    hlasm_ctx.ord_ctx.symbol_dependencies().add_dependency(
        std::make_unique<postponed_statement_impl>(std::move(stmt), hlasm_ctx.processing_stack()),
        std::move(dep_solver).derive_current_dependency_evaluation_context(),
        lib_info);

    hlasm_ctx.end_reached();
}
void asm_processor::process_ALIAS(rebuilt_statement stmt)
{
    auto symbol_name = find_label_symbol(stmt);
    if (symbol_name.empty())
    {
        add_diagnostic(diagnostic_op::error_A163_ALIAS_mandatory_label(stmt.stmt_range_ref()));
        return;
    }

    context::ordinary_assembly_dependency_solver dep_solver(hlasm_ctx.ord_ctx, lib_info);
    hlasm_ctx.ord_ctx.symbol_dependencies().add_dependency(
        std::make_unique<postponed_statement_impl>(std::move(stmt), hlasm_ctx.processing_stack()),
        std::move(dep_solver).derive_current_dependency_evaluation_context(),
        lib_info);
}
void asm_processor::process_LTORG(rebuilt_statement stmt)
{
    constexpr size_t sectalgn = 8;
    auto loctr = hlasm_ctx.ord_ctx.align(context::alignment { 0, sectalgn }, lib_info);

    find_sequence_symbol(stmt);


    if (auto label = find_label_symbol(stmt); !label.empty())
    {
        if (hlasm_ctx.ord_ctx.symbol_defined(label))
            add_diagnostic(diagnostic_op::error_E031("symbol", stmt.label_ref().field_range));
        else
            create_symbol(stmt.stmt_range_ref(),
                label,
                loctr,
                context::symbol_attributes(context::symbol_origin::EQU, 'U'_ebcdic, 1));
    }

    hlasm_ctx.ord_ctx.generate_pool(*this, hlasm_ctx.using_current(), lib_info);

    context::ordinary_assembly_dependency_solver dep_solver(hlasm_ctx.ord_ctx, lib_info);
    hlasm_ctx.ord_ctx.symbol_dependencies().add_dependency(
        std::make_unique<postponed_statement_impl>(std::move(stmt), hlasm_ctx.processing_stack()),
        std::move(dep_solver).derive_current_dependency_evaluation_context(),
        lib_info);
}

void asm_processor::process_USING(rebuilt_statement stmt)
{
    using namespace expressions;

    auto loctr = hlasm_ctx.ord_ctx.align(context::no_align, lib_info);
    context::ordinary_assembly_dependency_solver dep_solver(hlasm_ctx.ord_ctx, loctr, lib_info);

    auto label = find_using_label(stmt);

    if (!label.empty())
    {
        if (!hlasm_ctx.ord_ctx.symbol_defined(label))
        {
            hlasm_ctx.ord_ctx.register_using_label(label);
        }
        else if (!hlasm_ctx.ord_ctx.is_using_label(label))
        {
            add_diagnostic(diagnostic_op::error_E031("symbol", stmt.label_ref().field_range));
            return;
        }
    }
    mach_expr_ptr b;
    mach_expr_ptr e;

    const auto& ops = stmt.operands_ref().value;

    if (ops.size() < 2 || ops.size() > 17)
    {
        add_diagnostic(diagnostic_op::error_A012_from_to("USING", 2, 17, stmt.operands_ref().field_range));
        return;
    }

    if (ops.front()->type != semantics::operand_type::ASM)
    {
        add_diagnostic(diagnostic_op::error_A104_USING_first_format(ops.front()->operand_range));
        return;
    }

    switch (auto asm_op = ops.front()->access_asm(); asm_op->kind)
    {
        case hlasm_plugin::parser_library::semantics::asm_kind::EXPR:
            b = asm_op->access_expr()->expression->clone();
            break;

        case hlasm_plugin::parser_library::semantics::asm_kind::BASE_END: {
            auto using_op = asm_op->access_base_end();
            b = using_op->base->clone();
            e = using_op->end->clone();
            break;
        }
        default:
            add_diagnostic(diagnostic_op::error_A104_USING_first_format(asm_op->operand_range));
            return;
    }

    std::vector<mach_expr_ptr> bases;
    bases.reserve(ops.size() - 1);
    for (const auto& expr : std::span(ops).subspan(1))
    {
        if (expr->type != semantics::operand_type::ASM)
        {
            add_diagnostic(diagnostic_op::error_A164_USING_mapping_format(expr->operand_range));
            return;
        }
        else if (auto asm_expr = expr->access_asm()->access_expr(); !asm_expr)
        {
            add_diagnostic(diagnostic_op::error_A164_USING_mapping_format(expr->operand_range));
            return;
        }
        else
            bases.push_back(asm_expr->expression->clone());
    }

    hlasm_ctx.using_add(label,
        std::move(b),
        std::move(e),
        std::move(bases),
        std::move(dep_solver).derive_current_dependency_evaluation_context(),
        hlasm_ctx.processing_stack());
}

void asm_processor::process_DROP(rebuilt_statement stmt)
{
    using namespace expressions;

    auto loctr = hlasm_ctx.ord_ctx.align(context::no_align, lib_info);
    context::ordinary_assembly_dependency_solver dep_solver(hlasm_ctx.ord_ctx, loctr, lib_info);

    if (auto label = find_label_symbol(stmt); !label.empty())
    {
        if (hlasm_ctx.ord_ctx.symbol_defined(label))
        {
            add_diagnostic(diagnostic_op::error_E031("symbol", stmt.label_ref().field_range));
        }
        else
        {
            add_diagnostic(diagnostic_op::warn_A251_unexpected_label(stmt.label_ref().field_range));
            create_symbol(stmt.stmt_range_ref(), label, loctr, context::symbol_attributes(context::symbol_origin::EQU));
        }
    }

    const auto& ops = stmt.operands_ref().value;

    std::vector<mach_expr_ptr> bases;
    if (!ops.empty()
        && !(ops.size() == 2 && ops[0]->type == semantics::operand_type::EMPTY
            && ops[1]->type == semantics::operand_type::EMPTY))
    {
        bases.reserve(ops.size());
        for (const auto& op : ops)
        {
            if (auto asm_op = op->access_asm(); !asm_op)
                add_diagnostic(diagnostic_op::error_A141_DROP_op_format(op->operand_range));
            else if (auto expr = asm_op->access_expr(); !expr)
                add_diagnostic(diagnostic_op::error_A141_DROP_op_format(op->operand_range));
            else
                bases.push_back(expr->expression->clone());
        }
    }

    hlasm_ctx.using_remove(std::move(bases),
        std::move(dep_solver).derive_current_dependency_evaluation_context(),
        hlasm_ctx.processing_stack());
}

namespace {
bool asm_expr_quals(const semantics::operand_ptr& op, std::string_view value)
{
    auto asm_op = op->access_asm();
    if (!asm_op)
        return false;
    auto expr = asm_op->access_expr();
    return expr && expr->get_value() == value;
}
} // namespace

void asm_processor::process_PUSH(rebuilt_statement stmt)
{
    const auto& ops = stmt.operands_ref().value;

    if (std::any_of(ops.begin(), ops.end(), [](const auto& op) { return asm_expr_quals(op, "USING"); }))
        hlasm_ctx.using_push();

    context::ordinary_assembly_dependency_solver dep_solver(hlasm_ctx.ord_ctx, lib_info);
    hlasm_ctx.ord_ctx.symbol_dependencies().add_dependency(
        std::make_unique<postponed_statement_impl>(std::move(stmt), hlasm_ctx.processing_stack()),
        std::move(dep_solver).derive_current_dependency_evaluation_context(),
        lib_info);
}

void asm_processor::process_POP(rebuilt_statement stmt)
{
    const auto& ops = stmt.operands_ref().value;

    if (std::any_of(ops.begin(), ops.end(), [](const auto& op) { return asm_expr_quals(op, "USING"); })
        && !hlasm_ctx.using_pop())
        add_diagnostic(diagnostic_op::error_A165_POP_USING(stmt.stmt_range_ref()));

    context::ordinary_assembly_dependency_solver dep_solver(hlasm_ctx.ord_ctx, lib_info);
    hlasm_ctx.ord_ctx.symbol_dependencies().add_dependency(
        std::make_unique<postponed_statement_impl>(std::move(stmt), hlasm_ctx.processing_stack()),
        std::move(dep_solver).derive_current_dependency_evaluation_context(),
        lib_info);
}

void asm_processor::process_MNOTE(rebuilt_statement stmt)
{
    static constexpr std::string_view MNOTE = "MNOTE";
    const auto& ops = stmt.operands_ref().value;

    std::optional<int> level;
    size_t first_op_len = 0;

    find_sequence_symbol(stmt);

    switch (ops.size())
    {
        case 1:
            level = 0;
            break;
        case 2:
            switch (ops[0]->type)
            {
                case semantics::operand_type::EMPTY:
                    level = 1;
                    break;
                case semantics::operand_type::ASM:
                    if (auto expr = ops[0]->access_asm()->access_expr(); !expr)
                    {
                        // fail
                    }
                    else if (dynamic_cast<const expressions::mach_expr_location_counter*>(expr->expression.get()))
                    {
                        level = 0;
                        first_op_len = 1;
                    }
                    else
                    {
                        const auto& val = expr->get_value();
                        first_op_len = val.size();
                        level = try_get_number(val);
                    }
                    break;

                default:
                    break;
            }
            break;
        default:
            add_diagnostic(diagnostic_op::error_A012_from_to(MNOTE, 1, 2, stmt.operands_ref().field_range));
            return;
    }
    if (!level.has_value() || level.value() < 0 || level.value() > 255)
    {
        add_diagnostic(diagnostic_op::error_A119_MNOTE_first_op_format(ops[0]->operand_range));
        return;
    }

    std::string_view text;

    const auto& r = ops.back()->operand_range;
    if (ops.back()->type != semantics::operand_type::ASM)
    {
        add_diagnostic(diagnostic_op::warning_A300_op_apostrophes_missing(MNOTE, r));
    }
    else
    {
        auto* string_op = ops.back()->access_asm();
        if (string_op->kind == semantics::asm_kind::STRING)
        {
            text = string_op->access_string()->value;
        }
        else
        {
            if (string_op->kind == semantics::asm_kind::EXPR)
            {
                text = string_op->access_expr()->get_value();
            }
            add_diagnostic(diagnostic_op::warning_A300_op_apostrophes_missing(MNOTE, r));
        }
    }

    if (text.size() > checking::MNOTE_max_message_length)
    {
        add_diagnostic(diagnostic_op::error_A117_MNOTE_message_size(r));
        text = text.substr(0, checking::MNOTE_max_message_length);
    }
    else if (text.size() + first_op_len > checking::MNOTE_max_operands_length)
    {
        add_diagnostic(diagnostic_op::error_A118_MNOTE_operands_size(r));
    }

    std::string sanitized;
    sanitized.reserve(text.size());
    utils::append_utf8_sanitized(sanitized, text);

    add_diagnostic(diagnostic_op::mnote_diagnostic(level.value(), sanitized, r));

    hlasm_ctx.update_mnote_max((unsigned)level.value());
}

void asm_processor::process_CXD(rebuilt_statement stmt)
{
    context::address loctr = hlasm_ctx.ord_ctx.align(context::fullword, lib_info);
    constexpr uint32_t cxd_length = 4;

    // process label
    if (auto label = find_label_symbol(stmt); !label.empty())
    {
        if (!hlasm_ctx.ord_ctx.symbol_defined(label))
        {
            create_symbol(stmt.stmt_range_ref(),
                label,
                loctr,
                context::symbol_attributes(context::symbol_origin::ASM, 'A'_ebcdic, cxd_length));
        }
        else
            add_diagnostic(diagnostic_op::error_E031("symbol", stmt.label_ref().field_range));
    }

    hlasm_ctx.ord_ctx.reserve_storage_area(cxd_length, context::no_align, lib_info);
}

struct title_label_visitor
{
    std::string operator()(const std::string& v) const { return v; }
    std::string operator()(const semantics::ord_symbol_string& v) const { return v.mixed_case; }
    std::string operator()(const semantics::concat_chain&) const { return {}; }
    std::string operator()(const semantics::seq_sym&) const { return {}; }
    std::string operator()(const semantics::vs_ptr&) const { return {}; }
};

void asm_processor::process_TITLE(rebuilt_statement stmt)
{
    const auto& label = stmt.label_ref();

    if (auto label_text = std::visit(title_label_visitor(), label.value); !label_text.empty())
    {
        if (hlasm_ctx.get_title_name().empty())
            hlasm_ctx.set_title_name(std::move(label_text));
        else
            add_diagnostic(diagnostic_op::warning_W016(label.field_range));
    }

    hlasm_ctx.ord_ctx.symbol_dependencies().add_dependency(
        std::make_unique<postponed_statement_impl>(std::move(stmt), hlasm_ctx.processing_stack()),
        context::ordinary_assembly_dependency_solver(hlasm_ctx.ord_ctx, lib_info)
            .derive_current_dependency_evaluation_context(),
        lib_info);
}

} // namespace hlasm_plugin::parser_library::processing
