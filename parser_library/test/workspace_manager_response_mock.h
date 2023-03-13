/*
 * Copyright (c) 2023 Broadcom.
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


#ifndef HLASMPLUGIN_PARSERLIBRARY_WORKSPACE_MANAGER_RESPONSE_MOCK_H
#define HLASMPLUGIN_PARSERLIBRARY_WORKSPACE_MANAGER_RESPONSE_MOCK_H

#include "gmock/gmock.h"

namespace hlasm_plugin::parser_library {
template<typename T>
class workspace_manager_response_mock
{
public:
    MOCK_METHOD(bool, valid, (), (const));
    MOCK_METHOD(void, error, (int, const char*), ());
    MOCK_METHOD(void, provide, (T), ());
};
} // namespace hlasm_plugin::parser_library

#endif