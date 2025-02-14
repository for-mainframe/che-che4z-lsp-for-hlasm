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

#include "variable_symbol.h"

#include "context/hlasm_context.h"
#include "expressions/conditional_assembly/terms/ca_constant.h"
#include "expressions/evaluation_context.h"

namespace hlasm_plugin::parser_library::semantics {

basic_variable_symbol::basic_variable_symbol(
    context::id_index name, std::vector<expressions::ca_expr_ptr> subscript, range symbol_range)
    : variable_symbol(false, std::move(subscript), std::move(symbol_range))
    , name(name)
{}

basic_variable_symbol::~basic_variable_symbol() = default;

context::id_index basic_variable_symbol::evaluate_name(const expressions::evaluation_context&) const { return name; }

created_variable_symbol::created_variable_symbol(
    concat_chain created_name, std::vector<expressions::ca_expr_ptr> subscript, range symbol_range)
    : variable_symbol(true, std::move(subscript), std::move(symbol_range))
    , created_name(std::move(created_name))
{}

created_variable_symbol::~created_variable_symbol() = default;

context::id_index created_variable_symbol::evaluate_name(const expressions::evaluation_context& eval_ctx) const
{
    auto str_name = concatenation_point::evaluate(created_name, eval_ctx);

    auto [valid, id] = eval_ctx.hlasm_ctx.try_get_symbol_name(str_name);
    if (!valid)
        eval_ctx.diags.add_diagnostic(diagnostic_op::error_E065(symbol_range));

    return id;
}

void created_variable_symbol::resolve(context::SET_t_enum parent_expr_kind, diagnostic_op_consumer& diag)
{
    for (const auto& c : created_name)
        c.resolve(diag);
    variable_symbol::resolve(parent_expr_kind, diag);
}

basic_variable_symbol* variable_symbol::access_basic()
{
    return created ? nullptr : static_cast<basic_variable_symbol*>(this);
}

const basic_variable_symbol* variable_symbol::access_basic() const
{
    return created ? nullptr : static_cast<const basic_variable_symbol*>(this);
}

created_variable_symbol* variable_symbol::access_created()
{
    return created ? static_cast<created_variable_symbol*>(this) : nullptr;
}

const created_variable_symbol* variable_symbol::access_created() const
{
    return created ? static_cast<const created_variable_symbol*>(this) : nullptr;
}

vs_eval variable_symbol::evaluate_symbol(const expressions::evaluation_context& eval_ctx) const
{
    return vs_eval(evaluate_name(eval_ctx), evaluate_subscript(eval_ctx));
}

std::vector<context::A_t> variable_symbol::evaluate_subscript(const expressions::evaluation_context& eval_ctx) const
{
    std::vector<context::A_t> eval_subscript;
    for (const auto& expr : subscript)
    {
        auto val = expr->evaluate<context::A_t>(eval_ctx);
        eval_subscript.push_back(val);
    }

    return eval_subscript;
}

context::SET_t variable_symbol::evaluate(const expressions::evaluation_context& eval_ctx) const
{
    auto [name, evaluated_subscript] = evaluate_symbol(eval_ctx);

    auto val = get_var_sym_value(eval_ctx.hlasm_ctx, name, evaluated_subscript, symbol_range, eval_ctx.diags);

    return val;
}

void variable_symbol::resolve(context::SET_t_enum parent_expr_kind, diagnostic_op_consumer& diag)
{
    expressions::ca_expression_ctx expr_ctx = { context::SET_t_enum::A_TYPE,
        parent_expr_kind == context::SET_t_enum::B_TYPE ? parent_expr_kind : context::SET_t_enum::A_TYPE,
        true };

    for (const auto& v : subscript)
        v->resolve_expression_tree(expr_ctx, diag);
}

variable_symbol::variable_symbol(
    const bool created, std::vector<expressions::ca_expr_ptr> subscript, range symbol_range)
    : created(created)
    , subscript(std::move(subscript))
    , symbol_range(std::move(symbol_range))
{}

} // namespace hlasm_plugin::parser_library::semantics
