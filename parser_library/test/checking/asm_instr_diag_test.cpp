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

#include "gtest/gtest.h"

#include "../common_testing.h"

TEST(diagnostics, org_incorrect_second_op)
{
    std::string input(
        R"( 
 ORG *,complex(operand)
)");
    analyzer a(input);
    a.analyze();
    a.collect_diags();
    EXPECT_EQ(get_syntax_errors(a), (size_t)0);
    EXPECT_TRUE(matches_message_codes(a.diags(), { "A115" }));
}

TEST(diagnostics, exitctl_op_incorrect_format)
{
    std::string input(
        R"( 
 EXITCTL SOURCE,complex(operand)
)");
    analyzer a(input);
    a.analyze();
    a.collect_diags();
    EXPECT_EQ(get_syntax_errors(a), (size_t)0);
    EXPECT_TRUE(matches_message_codes(a.diags(), { "A020" }));
}

TEST(diagnostics, exitctl_op_incorrect_value)
{
    std::string input(
        R"( 
 EXITCTL LISTING,not_number
)");
    analyzer a(input);
    a.analyze();
    a.collect_diags();
    EXPECT_EQ(get_syntax_errors(a), (size_t)0);
    EXPECT_TRUE(matches_message_codes(a.diags(), { "A131" }));
}

TEST(diagnostics, extrn_incorrect_part_operand)
{
    std::string input(
        R"( 
 EXTRN PART(,)
)");
    analyzer a(input);
    a.analyze();
    a.collect_diags();
    EXPECT_EQ(get_syntax_errors(a), (size_t)0);
    EXPECT_TRUE(matches_message_codes(a.diags(), { "A129" }));
}

TEST(diagnostics, extrn_incorrect_complex_operand)
{
    std::string input(
        R"( 
 EXTRN complex(operand)
)");
    analyzer a(input);
    a.analyze();
    a.collect_diags();
    EXPECT_EQ(get_syntax_errors(a), (size_t)0);
    EXPECT_TRUE(matches_message_codes(a.diags(), { "A129" }));
}

TEST(diagnostics, extrn_incorrect_part_type)
{
    std::string input(
        R"( 
 EXTRN PART(1)
)");
    analyzer a(input);
    a.analyze();
    a.collect_diags();

    EXPECT_EQ(get_syntax_errors(a), (size_t)0);
    EXPECT_TRUE(matches_message_codes(a.diags(), { "A129" }));
}

TEST(diagnostics, ictl_empty_op)
{
    std::string input(
        R"( 
 ICTL , 
)");
    analyzer a(input);
    a.analyze();
    a.collect_diags();
    EXPECT_EQ(get_syntax_errors(a), (size_t)0);
    EXPECT_TRUE(matches_message_codes(a.diags(), { "A021" }));
}

TEST(diagnostics, ictl_undefined_op)
{
    std::string input(
        R"( 
 ICTL 1, 
)");
    analyzer a(input);
    a.analyze();
    a.collect_diags();
    EXPECT_EQ(get_syntax_errors(a), (size_t)0);
    EXPECT_TRUE(matches_message_codes(a.diags(), { "A242" }));
}

TEST(diagnostics, ictl_incorrect_begin_val)
{
    std::string input(
        R"( 
 ICTL 120
)");
    analyzer a(input);
    a.analyze();
    a.collect_diags();
    EXPECT_EQ(get_syntax_errors(a), (size_t)0);
    EXPECT_TRUE(matches_message_codes(a.diags(), { "A123" }));
}

TEST(diagnostics, ictl_incorrect_continuation_val)
{
    std::string input(
        R"( 
 ICTL 1,41,130
)");
    analyzer a(input);
    a.analyze();
    a.collect_diags();
    EXPECT_EQ(get_syntax_errors(a), (size_t)0);
    EXPECT_TRUE(matches_message_codes(a.diags(), { "A126" }));
}

TEST(diagnostics, ictl_incorrect_end_begin_diff)
{
    std::string input(
        R"( 
 ICTL 40,41
)");
    analyzer a(input);
    a.analyze();
    a.collect_diags();
    EXPECT_EQ(get_syntax_errors(a), (size_t)0);
    EXPECT_TRUE(matches_message_codes(a.diags(), { "A125" }));
}

TEST(diagnostics, ictl_incorrect_continuation_begin_diff)
{
    std::string input(
        R"( 
 ICTL 10,70,2
)");
    analyzer a(input);
    a.analyze();
    a.collect_diags();
    EXPECT_EQ(get_syntax_errors(a), (size_t)0);
    EXPECT_TRUE(matches_message_codes(a.diags(), { "A127" }));
}

TEST(diagnostics, end_incorrect_first_op_format)
{
    std::string input(
        R"( 
 END complex(operand) 
)");
    analyzer a(input);
    a.analyze();
    a.collect_diags();
    EXPECT_EQ(get_syntax_errors(a), (size_t)0);
    EXPECT_TRUE(matches_message_codes(a.diags(), { "A243" }));
}

TEST(diagnostics, end_incorrect_second_op_format)
{
    std::string input(
        R"( 
simple equ 2
 END ,simple
)");
    analyzer a(input);
    a.analyze();
    a.collect_diags();
    EXPECT_EQ(get_syntax_errors(a), (size_t)0);
    EXPECT_TRUE(matches_message_codes(a.diags(), { "A001" }));
}


TEST(diagnostics, end_incorrect_language_third_char)
{
    std::string input(
        R"( 
 END ,(one,four,toolong)
)");
    analyzer a(input);
    a.analyze();
    a.collect_diags();
    EXPECT_EQ(get_syntax_errors(a), (size_t)0);
    EXPECT_TRUE(matches_message_codes(a.diags(), { "A140" }));
}

TEST(diagnostics, end_incorrect_language_second_char)
{
    std::string input(
        R"( 
 END ,(one,two,three)
)");
    analyzer a(input);
    a.analyze();
    a.collect_diags();
    EXPECT_EQ(get_syntax_errors(a), (size_t)0);
    EXPECT_TRUE(matches_message_codes(a.diags(), { "A139" }));
}

TEST(diagnostics, end_incorrect_language_format)
{
    std::string input(
        R"( 
 END ,wrong(one,two,three)
)");
    analyzer a(input);
    a.analyze();
    a.collect_diags();
    EXPECT_EQ(get_syntax_errors(a), (size_t)0);
    EXPECT_TRUE(matches_message_codes(a.diags(), { "A137" }));
}

TEST(diagnostics, drop_incorrect_op_format)
{
    std::string input(
        R"( 
 DROP complex(operand)
)");
    analyzer a(input);
    a.analyze();
    a.collect_diags();
    EXPECT_EQ(get_syntax_errors(a), (size_t)0);
    EXPECT_TRUE(matches_message_codes(a.diags(), { "A141" }));
}

TEST(diagnostics, cnop_incorrect_first_op_format)
{
    std::string input(
        R"( 
 CNOP complex(operand),3
)");
    analyzer a(input);
    a.analyze();
    a.collect_diags();
    EXPECT_EQ(get_syntax_errors(a), (size_t)0);
    EXPECT_TRUE(matches_message_codes(a.diags(), { "A143" }));
}

TEST(diagnostics, cnop_incorrect_second_op_format)
{
    std::string input(
        R"( 
 CNOP 10,complex(operand)
)");
    analyzer a(input);
    a.analyze();
    a.collect_diags();
    EXPECT_EQ(get_syntax_errors(a), (size_t)0);
    EXPECT_TRUE(matches_message_codes(a.diags(), { "A143" }));
}

TEST(diagnostics, cnop_incorrect_boundary)
{
    std::string input(
        R"( 
 CNOP 14,17
)");
    analyzer a(input);
    a.analyze();
    a.collect_diags();
    EXPECT_EQ(get_syntax_errors(a), (size_t)0);
    EXPECT_TRUE(matches_message_codes(a.diags(), { "A145" }));
}

TEST(diagnostics, ccw_unspecified_operand)
{
    std::string input(
        R"( 
  CCW ,,,
)");
    analyzer a(input);
    a.analyze();
    a.collect_diags();
    EXPECT_EQ(get_syntax_errors(a), (size_t)0);
    EXPECT_TRUE(matches_message_codes(a.diags(), { "A147" }));
}

TEST(diagnostics, ccw_incorrect_first_op)
{
    std::string input(
        R"( 
  CCW complex(operand),,,
)");
    analyzer a(input);
    a.analyze();
    a.collect_diags();
    EXPECT_EQ(get_syntax_errors(a), (size_t)0);
    EXPECT_TRUE(matches_message_codes(a.diags(), { "A143" }));
}

TEST(diagnostics, ccw_incorrect_second_op)
{
    std::string input(
        R"( 
  CCW 2,complex(operand),,
)");
    analyzer a(input);
    a.analyze();
    a.collect_diags();
    EXPECT_EQ(get_syntax_errors(a), (size_t)0);
    EXPECT_TRUE(matches_message_codes(a.diags(), { "A247" }));
}

TEST(diagnostics, space_incorrect_op_format)
{
    std::string input(
        R"( 
 SPACE complex(operand)
)");
    analyzer a(input);
    a.analyze();
    a.collect_diags();
    EXPECT_EQ(get_syntax_errors(a), (size_t)0);
    EXPECT_TRUE(matches_message_codes(a.diags(), { "A240" }));
}

TEST(diagnostics, space_incorrect_op_value)
{
    std::string input(
        R"( 
 SPACE -1
)");
    analyzer a(input);
    a.analyze();
    a.collect_diags();
    EXPECT_EQ(get_syntax_errors(a), (size_t)0);
    EXPECT_TRUE(matches_message_codes(a.diags(), { "A148" }));
}

TEST(diagnostics, cattr_incorrect_simple_format)
{
    std::string input(
        R"( 
 CATTR wrong
)");
    analyzer a(input);
    a.analyze();
    a.collect_diags();
    EXPECT_EQ(get_syntax_errors(a), (size_t)0);
    EXPECT_TRUE(matches_message_codes(a.diags(), { "A149" }));
}

TEST(diagnostics, cattr_incorrect_complex_format)
{
    std::string input(
        R"( 
 CATTR wrong(wrong)
)");
    analyzer a(input);
    a.analyze();
    a.collect_diags();
    EXPECT_EQ(get_syntax_errors(a), (size_t)0);
    EXPECT_TRUE(matches_message_codes(a.diags(), { "A149" }));
}

TEST(diagnostics, cattr_incorrect_complex_params)
{
    std::string input(
        R"( 
 CATTR RMODE(one,two)
)");
    analyzer a(input);
    a.analyze();
    a.collect_diags();
    EXPECT_EQ(get_syntax_errors(a), (size_t)0);
    EXPECT_TRUE(matches_message_codes(a.diags(), { "A016" }));
}

TEST(diagnostics, cattr_incorrect_rmode_param)
{
    std::string input(
        R"( 
 CATTR RMODE(wrong)
)");
    analyzer a(input);
    a.analyze();
    a.collect_diags();
    EXPECT_EQ(get_syntax_errors(a), (size_t)0);
    EXPECT_TRUE(matches_message_codes(a.diags(), { "A204" }));
}

TEST(diagnostics, cattr_incorrect_align_param)
{
    std::string input(
        R"( 
 CATTR ALIGN(6)
)");
    analyzer a(input);
    a.analyze();
    a.collect_diags();
    EXPECT_EQ(get_syntax_errors(a), (size_t)0);
    EXPECT_TRUE(matches_message_codes(a.diags(), { "A205" }));
}

TEST(diagnostics, cattr_incorrect_fill_param)
{
    std::string input(
        R"( 
 CATTR FILL(256)
)");
    analyzer a(input);
    a.analyze();
    a.collect_diags();
    EXPECT_EQ(get_syntax_errors(a), (size_t)0);
    EXPECT_TRUE(matches_message_codes(a.diags(), { "A206" }));
}

TEST(diagnostics, cattr_incorrect_priority_param)
{
    std::string input(
        R"( 
 CATTR PRIORITY(wrong)
)");
    analyzer a(input);
    a.analyze();
    a.collect_diags();
    EXPECT_EQ(get_syntax_errors(a), (size_t)0);
    EXPECT_TRUE(matches_message_codes(a.diags(), { "A208" }));
}

TEST(diagnostics, cattr_incorrect_part_param)
{
    std::string input(
        R"( 
 CATTR PART()
)");
    analyzer a(input);
    a.analyze();
    a.collect_diags();
    EXPECT_EQ(get_syntax_errors(a), (size_t)0);
    EXPECT_TRUE(matches_message_codes(a.diags(), { "A207" }));
}

TEST(diagnostics, cattr_empty_op)
{
    std::string input(
        R"( 
 CATTR ,NOLOAD
)");
    analyzer a(input);
    a.analyze();
    a.collect_diags();
    EXPECT_EQ(get_syntax_errors(a), (size_t)0);
    EXPECT_TRUE(matches_message_codes(a.diags(), { "A021" }));
}

TEST(diagnostics, ainsert_incorrect_string)
{
    std::string input(
        R"( 
 AINSERT one,back
)");
    analyzer a(input);
    a.analyze();
    a.collect_diags();
    EXPECT_EQ(get_syntax_errors(a), (size_t)0);
    EXPECT_TRUE(matches_message_codes(a.diags(), { "A301" }));
}

TEST(diagnostics, ainsert_incorrect_second_op)
{
    std::string input(
        R"( 
 AINSERT 'string',wrong
)");
    analyzer a(input);
    a.analyze();
    a.collect_diags();
    EXPECT_EQ(get_syntax_errors(a), (size_t)0);
    EXPECT_TRUE(matches_message_codes(a.diags(), { "A156" }));
}

TEST(diagnostics, adata_incorrect_op_format)
{
    std::string input(
        R"( 
 ADATA complex(operand),1,1,1,'string'
)");
    analyzer a(input);
    a.analyze();
    a.collect_diags();
    EXPECT_EQ(get_syntax_errors(a), (size_t)0);
    EXPECT_TRUE(matches_message_codes(a.diags(), { "A158" }));
}

TEST(diagnostics, adata_incorrect_last_op_format)
{
    std::string input(
        R"( 
 ADATA 1,2,3,4,complex(operand)
)");
    analyzer a(input);
    a.analyze();
    a.collect_diags();
    EXPECT_EQ(get_syntax_errors(a), (size_t)0);
    EXPECT_TRUE(matches_message_codes(a.diags(), { "A239" }));
}

TEST(diagnostics, adata_string_not_enclosed)
{
    std::string input(
        R"( 
 ADATA 1,2,3,4,string
)");
    analyzer a(input);
    a.analyze();
    a.collect_diags();
    EXPECT_EQ(get_syntax_errors(a), (size_t)0);
    EXPECT_TRUE(matches_message_codes(a.diags(), { "A300" }));
}

TEST(diagnostics, adata_string_too_long)
{
    std::string input(
        R"( 
 ADATA 1,2,3,4,'loremipsumdolorsitametloremipsumdolorsitametloremipsumsX
                loremipsumdolorsitametloremipsumdolorsitametloremipsumsX
                loremipsumdolorsitametloremipsumdolorsitametloremipsumsX
                loremipsumdolorsitametloremipsumdolorsitametloremipsumsX
                loremipsumdolorsitametloremipsumdolorsitametloremipsumsX
               '
)");
    analyzer a(input);
    a.analyze();
    a.collect_diags();
    EXPECT_EQ(get_syntax_errors(a), (size_t)0);
    EXPECT_TRUE(matches_message_codes(a.diags(), { "A160" }));
}

TEST(diagnostics, acontrol_incorrect_simple_op_format)
{
    std::string input(
        R"( 
 ACONTROL wrong
)");
    analyzer a(input);
    a.analyze();
    a.collect_diags();
    EXPECT_EQ(get_syntax_errors(a), (size_t)0);
    EXPECT_TRUE(matches_message_codes(a.diags(), { "A161" }));
}

TEST(diagnostics, acontrol_incorrect_complex_op_format)
{
    std::string input(
        R"( 
 ACONTROL wrong(wrong)
)");
    analyzer a(input);
    a.analyze();
    a.collect_diags();
    EXPECT_EQ(get_syntax_errors(a), (size_t)0);
    EXPECT_TRUE(matches_message_codes(a.diags(), { "A161" }));
}

TEST(diagnostics, acontrol_compat_format)
{
    std::string input(
        R"( 
 ACONTROL COMPAT(wrong)
)");
    analyzer a(input);
    a.analyze();
    a.collect_diags();
    EXPECT_EQ(get_syntax_errors(a), (size_t)0);
    EXPECT_TRUE(matches_message_codes(a.diags(), { "A209" }));
}

TEST(diagnostics, acontrol_flag_format)
{
    std::string input(
        R"( 
 ACONTROL FLAG(wrong)
)");
    analyzer a(input);
    a.analyze();
    a.collect_diags();
    EXPECT_EQ(get_syntax_errors(a), (size_t)0);
    EXPECT_TRUE(matches_message_codes(a.diags(), { "A211" }));
}

TEST(diagnostics, acontrol_optable_params_size)
{
    std::string input(
        R"( 
 ACONTROL OPTABLE(one,two,three)
)");
    analyzer a(input);
    a.analyze();
    a.collect_diags();
    EXPECT_EQ(get_syntax_errors(a), (size_t)0);
    EXPECT_TRUE(matches_message_codes(a.diags(), { "A018" }));
}

TEST(diagnostics, acontrol_optable_first_params_format)
{
    std::string input(
        R"( 
 ACONTROL OPTABLE(one,two)
)");
    analyzer a(input);
    a.analyze();
    a.collect_diags();
    EXPECT_EQ(get_syntax_errors(a), (size_t)0);
    EXPECT_TRUE(matches_message_codes(a.diags(), { "A212" }));
}

TEST(diagnostics, acontrol_optable_second_params_format)
{
    std::string input(
        R"( 
  ACONTROL OPTABLE(DOS,wrong)
)");
    analyzer a(input);
    a.analyze();
    a.collect_diags();
    EXPECT_EQ(get_syntax_errors(a), (size_t)0);
    EXPECT_TRUE(matches_message_codes(a.diags(), { "A213" }));
}

TEST(diagnostics, acontrol_typecheck_param)
{
    std::string input(
        R"( 
 ACONTROL TC(wrong)
)");
    analyzer a(input);
    a.analyze();
    a.collect_diags();
    EXPECT_EQ(get_syntax_errors(a), (size_t)0);
    EXPECT_TRUE(matches_message_codes(a.diags(), { "A214" }));
}

TEST(diagnostics, acontrol_empty_op)
{
    std::string input(
        R"( 
 ACONTROL ,
)");
    analyzer a(input);
    a.analyze();
    a.collect_diags();
    EXPECT_EQ(get_syntax_errors(a), (size_t)0);
    EXPECT_TRUE(matches_message_codes(a.diags(), { "A021" }));
}


TEST(diagnostics, extrn_empty_op)
{
    std::string input(
        R"( 
 EXTRN ,
)");
    analyzer a(input);
    a.analyze();
    a.collect_diags();
    EXPECT_EQ(get_syntax_errors(a), (size_t)0);
    EXPECT_TRUE(matches_message_codes(a.diags(), { "A021" }));
}

TEST(diagnostics, xattr_scope_value)
{
    std::string input(
        R"( 
 XATTR SCOPE(wrong)
)");
    analyzer a(input);
    a.analyze();
    a.collect_diags();
    EXPECT_EQ(get_syntax_errors(a), (size_t)0);
    EXPECT_TRUE(matches_message_codes(a.diags(), { "A200" }));
}

TEST(diagnostics, xattr_linkage_value)
{
    std::string input(
        R"( 
 XATTR LINKAGE(wrong)
)");
    analyzer a(input);
    a.analyze();
    a.collect_diags();
    EXPECT_EQ(get_syntax_errors(a), (size_t)0);
    EXPECT_TRUE(matches_message_codes(a.diags(), { "A201" }));
}

TEST(diagnostics, xattr_reference_value)
{
    std::string input(
        R"( 
 XATTR REFERENCE(wrong)
)");
    analyzer a(input);
    a.analyze();
    a.collect_diags();
    EXPECT_EQ(get_syntax_errors(a), (size_t)0);
    EXPECT_TRUE(matches_message_codes(a.diags(), { "A288" }));
}

TEST(diagnostics, xattr_reference_direct_indirect_options)
{
    std::string input(
        R"( 
 XATTR REFERENCE(DIRECT,INDIRECT)
)");
    analyzer a(input);
    a.analyze();
    a.collect_diags();
    EXPECT_EQ(get_syntax_errors(a), (size_t)0);
    EXPECT_TRUE(matches_message_codes(a.diags(), { "A202" }));
}

TEST(diagnostics, xattr_reference_number_of_params)
{
    std::string input(
        R"( 
 XATTR REFERENCE(operand,operand,operand)
)");
    analyzer a(input);
    a.analyze();
    a.collect_diags();
    EXPECT_EQ(get_syntax_errors(a), (size_t)0);
    EXPECT_TRUE(matches_message_codes(a.diags(), { "A018" }));
}

TEST(diagnostics, mnote_incorrect_message)
{
    std::string input(
        R"( 
 MNOTE complex(operand),'message'
)");
    analyzer a(input);
    a.analyze();
    a.collect_diags();
    EXPECT_EQ(get_syntax_errors(a), (size_t)0);
    EXPECT_TRUE(matches_message_codes(a.diags(), { "A119" }));
}

TEST(diagnostics, mnote_first_op_value)
{
    std::string input(
        R"( 
 MNOTE not_number,'message'
)");
    analyzer a(input);
    a.analyze();
    a.collect_diags();
    EXPECT_EQ(get_syntax_errors(a), (size_t)0);
    EXPECT_TRUE(matches_message_codes(a.diags(), { "A119" }));
}

TEST(diagnostics, mnote_first_op_format)
{
    std::string input(
        R"( 
 MNOTE complex(operand)
)");
    analyzer a(input);
    a.analyze();
    a.collect_diags();
    EXPECT_EQ(get_syntax_errors(a), (size_t)0);
    EXPECT_TRUE(matches_message_codes(a.diags(), { "A300", "MNOTE" }));
}

TEST(diagnostics, mnote_long_message)
{
    std::string input(
        R"( 
 MNOTE 'extremely_long_character_sequence_that_is_over_the_allowed_charX
               limit_loremipsumdolorsitamet_loremipsumdolorsitametloremX
               ipsumdolorsitamet_loremipsumdolorsitamet_loremipsumdolorX
               ipsumdolorsitamet_loremipsumdolorsitamet_loremipsumdolorX
               ipsumdolorsitamet_loremipsumdolorsitamet_loremipsumdolorX
               ipsumdolorsitamet_loremipsumdolorsitamet_loremipsumdolorX
               ipsumdolorsitamet_loremipsumdolorsitamet_loremipsumdolorX
               ipsumdolorsitamet_loremipsumdolorsitamet_loremipsumdolorX
               ipsumdolorsitamet_loremipsumdolorsitamet_loremipsumdolorX
               ipsumdolorsitamet_loremipsumdolorsitamet_loremipsumdolorX
               ipsumdolorsitamet_loremipsumdolorsitamet_loremipsumdolorX
               ipsumdolorsitamet_loremipsumdolorsitamet_loremipsumdolorX
               ipsumdolorsitamet_loremipsumdolorsitamet_loremipsumdolorX
               ipsumdolorsitamet_loremipsumdolorsitamet_loremipsumdolorX
               ipsumdolorsitamet_loremipsumdolorsitamet_loremipsumdolorX
               ipsumdolorsitamet_loremipsumdolorsitamet_loremipsumdolorX
               ipsumdolorsitamet_loremipsumdolorsitamet_loremipsumdolorX
               ipsumdolorsitamet_loremipsumdolorsitamet_loremipsumdolorX
               ipsumdolorsitamet_loremipsumdolorsitamet_loremipsumdolo'
)");
    analyzer a(input);
    a.analyze();
    a.collect_diags();
    EXPECT_EQ(get_syntax_errors(a), (size_t)0);
    EXPECT_TRUE(matches_message_codes(a.diags(), { "A117", "MNOTE" }));
}

TEST(diagnostics, iseq_number_of_operands)
{
    std::string input(
        R"( 
 ISEQ 4
)");
    analyzer a(input);
    a.analyze();
    a.collect_diags();
    EXPECT_EQ(get_syntax_errors(a), (size_t)0);
    EXPECT_TRUE(matches_message_codes(a.diags(), { "A013" }));
}

TEST(diagnostics, iseq_incorrect_op_value)
{
    std::string input(
        R"( 
 ISEQ 1,200 
)");
    analyzer a(input);
    a.analyze();
    a.collect_diags();
    EXPECT_EQ(get_syntax_errors(a), (size_t)0);
    EXPECT_TRUE(matches_message_codes(a.diags(), { "A120" }));
}

TEST(diagnostics, push_print_specified)
{
    std::string input(
        R"( 
 PUSH PRINT,PRINT
)");
    analyzer a(input);
    a.analyze();
    a.collect_diags();
    EXPECT_EQ(get_syntax_errors(a), (size_t)0);
    EXPECT_TRUE(matches_message_codes(a.diags(), { "A112" }));
}

TEST(diagnostics, push_acontrol_specified)
{
    std::string input(
        R"( 
 PUSH ACONTROL,ACONTROL
)");
    analyzer a(input);
    a.analyze();
    a.collect_diags();
    EXPECT_EQ(get_syntax_errors(a), (size_t)0);
    EXPECT_TRUE(matches_message_codes(a.diags(), { "A112" }));
}

TEST(diagnostics, pop_noprint_first)
{
    std::string input(
        R"( 
 POP NOPRINT,PRINT
)");
    analyzer a(input);
    a.analyze();
    a.collect_diags();
    EXPECT_EQ(get_syntax_errors(a), (size_t)0);
    EXPECT_TRUE(matches_message_codes(a.diags(), { "A113" }));
}

TEST(diagnostics, pop_incorrect_last_operand)
{
    std::string input(
        R"( 
 POP wrong
)");
    analyzer a(input);
    a.analyze();
    a.collect_diags();
    EXPECT_EQ(get_syntax_errors(a), (size_t)0);
    EXPECT_TRUE(matches_message_codes(a.diags(), { "A110" }));
}

TEST(diagnostics, pop_only_noprint_specified)
{
    std::string input(
        R"( 
 POP NOPRINT
)");
    analyzer a(input);
    a.analyze();
    a.collect_diags();
    EXPECT_EQ(get_syntax_errors(a), (size_t)0);
    EXPECT_TRUE(matches_message_codes(a.diags(), { "A114" }));
}

TEST(diagnostics, push_incorrect_op_value)
{
    std::string input(
        R"( 
 PUSH wrong,ACONTROL
)");
    analyzer a(input);
    a.analyze();
    a.collect_diags();
    EXPECT_EQ(get_syntax_errors(a), (size_t)0);
    EXPECT_TRUE(matches_message_codes(a.diags(), { "A111" }));
}

TEST(diagnostics, org_incorrect_first_op)
{
    std::string input(
        R"( 
 ORG complex(operand)
)");
    analyzer a(input);
    a.analyze();
    a.collect_diags();
    EXPECT_EQ(get_syntax_errors(a), (size_t)0);
    EXPECT_TRUE(matches_message_codes(a.diags(), { "A245" }));
}

struct mnote_test
{
    int code;
    std::string text;
    diagnostic_severity expected;
};

class mnote_fixture : public ::testing::TestWithParam<mnote_test>
{};

INSTANTIATE_TEST_SUITE_P(mnote,
    mnote_fixture,
    ::testing::Values(mnote_test { -2, "test", diagnostic_severity::hint },
        mnote_test { -1, "test", diagnostic_severity::hint },
        mnote_test { 0, "test", diagnostic_severity::hint },
        mnote_test { 1, "test", diagnostic_severity::hint },
        mnote_test { 2, "test", diagnostic_severity::info },
        mnote_test { 3, "test", diagnostic_severity::info },
        mnote_test { 4, "test", diagnostic_severity::warning },
        mnote_test { 5, "test", diagnostic_severity::warning },
        mnote_test { 6, "test", diagnostic_severity::warning },
        mnote_test { 7, "test", diagnostic_severity::warning },
        mnote_test { 8, "test", diagnostic_severity::error },
        mnote_test { 20, "test", diagnostic_severity::error },
        mnote_test { 150, "test", diagnostic_severity::error },
        mnote_test { 255, "test", diagnostic_severity::error }));

TEST_P(mnote_fixture, diagnostic_severity)
{
    const auto& [code, text, expected] = GetParam();
    std::string input = " MNOTE "
        + (code == -2        ? ""
                : code == -1 ? "*,"
                             : std::to_string(code) + ",")
        + "'" + text + "'";

    analyzer a(input);
    a.analyze();
    a.collect_diags();

    ASSERT_EQ(a.diags().size(), (size_t)1);

    const auto& d = a.diags()[0];
    EXPECT_EQ(d.code, "MNOTE");
    EXPECT_EQ(d.message, text);
    EXPECT_EQ(d.severity, expected);
}

TEST(mnote, substitution_first)
{
    std::string input = R"(
&L  SETA  4
    MNOTE &L,'test message'
)";

    analyzer a(input);
    a.analyze();
    a.collect_diags();

    ASSERT_EQ(a.diags().size(), (size_t)1);

    const auto& d = a.diags()[0];
    EXPECT_EQ(d.code, "MNOTE");
    EXPECT_EQ(d.message, "test message");
    EXPECT_EQ(d.severity, diagnostic_severity::warning);
}

TEST(mnote, substitution_both)
{
    std::string input = R"(
&L  SETA  8
&M  SETC  'test message'
    MNOTE &L,'&M'
)";

    analyzer a(input);
    a.analyze();
    a.collect_diags();

    ASSERT_EQ(a.diags().size(), (size_t)1);

    const auto& d = a.diags()[0];
    EXPECT_EQ(d.code, "MNOTE");
    EXPECT_EQ(d.message, "test message");
    EXPECT_EQ(d.severity, diagnostic_severity::error);
}

TEST(mnote, empty_first_arg)
{
    std::string input = R"(
    MNOTE ,'test message'
)";

    analyzer a(input);
    a.analyze();
    a.collect_diags();

    ASSERT_EQ(a.diags().size(), (size_t)1);

    const auto& d = a.diags()[0];
    EXPECT_EQ(d.code, "MNOTE");
    EXPECT_EQ(d.message, "test message");
    EXPECT_EQ(d.severity, diagnostic_severity::hint);
}

TEST(mnote, three_args)
{
    std::string input = R"(
    MNOTE ,'test message',
)";

    analyzer a(input);
    a.analyze();
    a.collect_diags();

    EXPECT_TRUE(matches_message_codes(a.diags(), { "A012" }));
}

TEST(mnote, emtpy_second_arg)
{
    std::string input = R"(
    MNOTE 0,
)";

    analyzer a(input);
    a.analyze();
    a.collect_diags();

    EXPECT_TRUE(matches_message_codes(a.diags(), { "MNOTE", "A300" }));
}

TEST(mnote, missing_quotes)
{
    std::string input = R"(
    MNOTE 0,test
)";

    analyzer a(input);
    a.analyze();
    a.collect_diags();

    EXPECT_TRUE(matches_message_codes(a.diags(), { "MNOTE", "A300" }));
}

TEST(mnote, nonprintable_characters)
{
    std::string input = R"(
&C  SETC X2C('0101')
    MNOTE 0,'&C'
)";

    analyzer a(input);
    a.analyze();
    a.collect_diags();

    ASSERT_TRUE(matches_message_codes(a.diags(), { "MNOTE" }));
    EXPECT_EQ(a.diags()[0].message, "<01><01>");
}
