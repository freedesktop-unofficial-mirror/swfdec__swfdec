/* Vivified
 * Copyright (C) 2008 Pekka Lampila <pekka.lampila@iki.fi>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 * 
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, 
 * Boston, MA  02110-1301  USA
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>

#include "vivi_compiler.h"

#include "vivi_code_assignment.h"
#include "vivi_code_binary.h"
#include "vivi_code_block.h"
#include "vivi_code_break.h"
#include "vivi_code_constant.h"
#include "vivi_code_continue.h"
#include "vivi_code_function.h"
#include "vivi_code_function_call.h"
#include "vivi_code_get.h"
#include "vivi_code_get_url.h"
#include "vivi_code_goto.h"
#include "vivi_code_if.h"
#include "vivi_code_init_object.h"
#include "vivi_code_loop.h"
#include "vivi_code_return.h"
#include "vivi_code_trace.h"
#include "vivi_code_unary.h"
#include "vivi_code_value_statement.h"
#include "vivi_compiler_empty_statement.h"

#include "vivi_code_text_printer.h"

typedef enum {
  STATUS_FAIL = -1,
  STATUS_OK = 0,
  STATUS_CANCEL = 1
} ParseStatus;

enum {
  TOKEN_FUNCTION = G_TOKEN_LAST + 1,
  TOKEN_PLUSPLUS = G_TOKEN_LAST + 2,
  TOKEN_MINUSMINUS = G_TOKEN_LAST + 3,
  TOKEN_NEW = G_TOKEN_LAST + 4,
  TOKEN_TRUE = G_TOKEN_LAST + 5,
  TOKEN_FALSE = G_TOKEN_LAST + 6,
  TOKEN_NULL = G_TOKEN_LAST + 7,
  TOKEN_THIS = G_TOKEN_LAST + 8
};

typedef enum {
  SYMBOL_NONE,
  // top
  SYMBOL_PROGRAM,
  SYMBOL_SOURCE_ELEMENT,
  // function
  SYMBOL_FUNCTION_DECLARATION,
  // statement
  SYMBOL_STATEMENT,
  SYMBOL_BLOCK,
  SYMBOL_VARIABLE_STATEMENT,
  SYMBOL_EMPTY_STATEMENT,
  SYMBOL_EXPRESSION_STATEMENT,
  SYMBOL_IF_STATEMENT,
  SYMBOL_ITERATION_STATEMENT,
  SYMBOL_CONTINUE_STATEMENT,
  SYMBOL_BREAK_STATEMENT,
  SYMBOL_RETURN_STATEMENT,
  SYMBOL_WITH_STATEMENT,
  SYMBOL_LABELLED_STATEMENT,
  SYMBOL_SWITCH_STATEMENT,
  SYMBOL_THROW_STATEMENT,
  SYMBOL_TRY_STATEMENT,
  // expression
  SYMBOL_EXPRESSION,
  SYMBOL_ASSIGNMENT_EXPRESSION,
  SYMBOL_CONDITIONAL_EXPRESSION,
  SYMBOL_LEFT_HAND_SIDE_EXPRESSION,
  SYMBOL_OPERATOR_EXPRESSION,
  SYMBOL_UNARY_EXPRESSION,
  SYMBOL_POSTFIX_EXPRESSION,
  SYMBOL_NEW_EXPRESSION,
  SYMBOL_CALL_EXPRESSION,
  SYMBOL_MEMBER_EXPRESSION,
  SYMBOL_FUNCTION_EXPRESSION,
  SYMBOL_PRIMARY_EXPRESSION,
  // values
  SYMBOL_IDENTIFIER,
  SYMBOL_LITERAL,
  SYMBOL_ARRAY_LITERAL,
  SYMBOL_OBJECT_LITERAL,
  // misc
  SYMBOL_ASSIGNMENT_OPERATOR,
  SYMBOL_ARGUMENTS,
} ParseSymbol;

typedef ParseStatus (*ParseStatementFunction) (GScanner *scanner, ViviCodeStatement **statement);
typedef ParseStatus (*ParseValueFunction) (GScanner *scanner, ViviCodeValue **value);

static ParseStatus
parse_statement (GScanner *scanner, ParseSymbol symbol,
    ViviCodeStatement **statement);

static ParseStatus
parse_value (GScanner *scanner, ParseSymbol symbol, ViviCodeValue **value);

static ParseStatus
parse_statement_list (GScanner *scanner, ParseSymbol symbol,
    ViviCodeStatement ***list, guint separator);

static ParseStatus
parse_value_list (GScanner *scanner, ParseSymbol symbol,
    ViviCodeValue ***list, guint separator);

// helpers

static gboolean
check_token (GScanner *scanner, guint token)
{
  g_scanner_peek_next_token (scanner);
  if (scanner->next_token != token)
    return FALSE;
  g_scanner_get_next_token (scanner);
  return TRUE;
}

static void
free_statement_list (ViviCodeStatement **list)
{
  int i;

  for (i = 0; list[i] != NULL; i++) {
    g_object_unref (list[i]);
  }
  g_free (list);
}

static void
free_value_list (ViviCodeValue **list)
{
  int i;

  for (i = 0; list[i] != NULL; i++) {
    g_object_unref (list[i]);
  }
  g_free (list);
}

static ViviCodeStatement *
create_block (ViviCodeStatement **list)
{
  ViviCodeBlock *block;
  int i;

  block = VIVI_CODE_BLOCK (vivi_code_block_new ());

  for (i = 0; list[i] != NULL; i++) {
    vivi_code_block_add_statement (block, list[i]);
  }

  return VIVI_CODE_STATEMENT (block);
}

// top

static ParseStatus
parse_program (GScanner *scanner, ViviCodeStatement **statement)
{
  ParseStatus status;
  ViviCodeStatement **list;

  *statement = NULL;

  status = parse_statement_list (scanner, SYMBOL_SOURCE_ELEMENT, &list,
      G_TOKEN_NONE);
  if (status != STATUS_OK)
    return status;

  *statement = create_block (list);

  return STATUS_OK;
}

static ParseStatus
parse_source_element (GScanner *scanner, ViviCodeStatement **statement)
{
  ParseStatus status;

  *statement = NULL;

  status = parse_statement (scanner, SYMBOL_FUNCTION_DECLARATION, statement);

  if (status == STATUS_CANCEL)
    status = parse_statement (scanner, SYMBOL_STATEMENT, statement);

  return status;
}

// function

static ParseStatus
parse_function_declaration (GScanner *scanner, ViviCodeStatement **statement)
{
  //ViviCodeStatement *function;
  ViviCodeValue *identifier;
  ViviCodeValue **arguments;
  ViviCodeStatement **body;

  *statement = NULL;

  identifier = NULL;
  arguments = NULL;
  body = NULL;

  if (!check_token (scanner, TOKEN_FUNCTION))
    return STATUS_CANCEL;

  if (parse_value (scanner, SYMBOL_IDENTIFIER, &identifier) != STATUS_OK)
    return STATUS_FAIL;

  if (!check_token (scanner, '('))
    goto fail;

  if (parse_value_list (scanner, SYMBOL_IDENTIFIER, &arguments, ',') ==
      STATUS_FAIL)
    goto fail;

  if (!check_token (scanner, ')'))
    goto fail;

  if (!check_token (scanner, '{'))
    goto fail;

  if (parse_statement_list (scanner, SYMBOL_SOURCE_ELEMENT, &body, G_TOKEN_NONE)
      == STATUS_FAIL)
    goto fail;

  if (!check_token (scanner, '}'))
    goto fail;

  /*function = vivi_code_function_new (arguments, body);
  *statement = vivi_code_assignment_new (NULL, VIVI_CODE_VALUE (identifier),
      VIVI_CODE_VALUE (function));*/
  *statement = vivi_compiler_empty_statement_new ();

  g_object_unref (identifier);
  if (arguments != NULL)
    free_value_list (arguments);
  if (body != NULL)
    free_statement_list (body);

  return STATUS_OK;

fail:
  if (identifier != NULL)
    g_object_unref (identifier);
  if (arguments != NULL)
    free_value_list (arguments);
  if (body != NULL)
    free_statement_list (body);

  return STATUS_FAIL;
}

// statement

static ParseStatus
parse_statement_symbol (GScanner *scanner, ViviCodeStatement **statement)
{
  int i, status;
  ParseSymbol options[] = {
    SYMBOL_BLOCK,
    SYMBOL_VARIABLE_STATEMENT,
    SYMBOL_EMPTY_STATEMENT,
    SYMBOL_EXPRESSION_STATEMENT,
    SYMBOL_IF_STATEMENT,
    SYMBOL_ITERATION_STATEMENT,
    SYMBOL_CONTINUE_STATEMENT,
    SYMBOL_BREAK_STATEMENT,
    SYMBOL_RETURN_STATEMENT,
    SYMBOL_WITH_STATEMENT,
    SYMBOL_LABELLED_STATEMENT,
    SYMBOL_SWITCH_STATEMENT,
    SYMBOL_THROW_STATEMENT,
    SYMBOL_TRY_STATEMENT,
    SYMBOL_NONE
  };

  *statement = NULL;

  for (i = 0; options[i] != SYMBOL_NONE; i++) {
    status = parse_statement (scanner, options[i], statement);
    if (status != STATUS_CANCEL)
      return status;
  }

  return STATUS_CANCEL;
}

static ParseStatus
parse_block (GScanner *scanner, ViviCodeStatement **statement)
{
  ViviCodeStatement **list;

  *statement = NULL;

  if (!check_token (scanner, '{'))
    return STATUS_CANCEL;

  g_scanner_peek_next_token (scanner);
  if (scanner->next_token != '}') {
    if (parse_statement_list (scanner, SYMBOL_STATEMENT, &list, G_TOKEN_NONE)
	!= STATUS_OK)
      return STATUS_FAIL;
  } else {
    list = g_new0 (ViviCodeStatement *, 1);
  }

  if (!check_token (scanner, '}')) {
    free_statement_list (list);
    return STATUS_FAIL;
  }

  *statement = create_block (list);
  free_statement_list (list);

  return STATUS_OK;
}

static ParseStatus
parse_empty_statement (GScanner *scanner, ViviCodeStatement **statement)
{
  *statement = NULL;

  if (!check_token (scanner, ';'))
    return STATUS_CANCEL;

  *statement = vivi_compiler_empty_statement_new ();

  return STATUS_OK;
}

static ParseStatus
parse_expression_statement (GScanner *scanner, ViviCodeStatement **statement)
{
  ParseStatus status;

  *statement = NULL;

  g_scanner_peek_next_token (scanner);
  if (scanner->next_token == '{' || scanner->next_token == TOKEN_FUNCTION)
    return STATUS_CANCEL;

  status = parse_statement (scanner, SYMBOL_EXPRESSION, statement);
  if (status != STATUS_OK)
    return status;

  if (!check_token (scanner, ';')) {
    g_object_unref (*statement);
    *statement = NULL;
    return STATUS_FAIL;
  }

  return STATUS_OK;
}

// expression

static ParseStatus
parse_expression (GScanner *scanner, ViviCodeStatement **statement)
{
  ViviCodeStatement **list;
  ParseStatus status;

  *statement = NULL;

  status =
    parse_statement_list (scanner, SYMBOL_ASSIGNMENT_EXPRESSION, &list, ',');
  if (status != STATUS_OK)
    return status;

  *statement = create_block (list);
  free_statement_list (list);

  return STATUS_OK;
}

static ParseStatus
parse_assignment_expression (GScanner *scanner, ViviCodeStatement **statement)
{
  *statement = NULL;

  // TODO

  return parse_statement (scanner, SYMBOL_CONDITIONAL_EXPRESSION, statement);
}

static ParseStatus
parse_conditional_expression (GScanner *scanner, ViviCodeStatement **statement)
{
  ParseStatus status;
  ViviCodeValue *value;
  ViviCodeStatement *if_statement, *else_statement;

  *statement = NULL;
  if_statement = NULL;

  status = parse_value (scanner, SYMBOL_OPERATOR_EXPRESSION, &value);
  if (status != STATUS_OK)
    return status;

  if (!check_token (scanner, '?')) {
    *statement = vivi_code_value_statement_new (VIVI_CODE_VALUE (value));
    g_object_unref (value);
    return STATUS_OK;
  }

  if (parse_statement (scanner, SYMBOL_ASSIGNMENT_EXPRESSION, &if_statement) !=
      STATUS_OK)
    goto fail;

  if (!check_token (scanner, ':'))
    goto fail;

  if (parse_statement (scanner, SYMBOL_ASSIGNMENT_EXPRESSION,
	&else_statement) != STATUS_OK)
    goto fail;

  *statement = vivi_code_if_new (value);
  vivi_code_if_set_if (VIVI_CODE_IF (*statement), if_statement);
  vivi_code_if_set_else (VIVI_CODE_IF (*statement), else_statement);

  g_object_unref (value);
  g_object_unref (if_statement);
  g_object_unref (else_statement);

  return STATUS_OK;

fail:

  g_object_unref (value);
  if (if_statement != NULL)
    g_object_unref (if_statement);

  return STATUS_FAIL;
}

static ParseStatus
parse_operator_expression (GScanner *scanner, ViviCodeValue **value)
{
  ParseStatus status;
  ViviCodeValue *left;
  ViviCodeValue *right;

  *value = NULL;

  status = parse_value (scanner, SYMBOL_UNARY_EXPRESSION, value);
  if (status != STATUS_OK)
    return status;

  do {
    if (!check_token (scanner, '+'))
      return STATUS_OK;

    if (parse_value (scanner, SYMBOL_UNARY_EXPRESSION, &right) != STATUS_OK) {
      g_object_unref (*value);
      *value = NULL;
      return STATUS_FAIL;
    }

    left = VIVI_CODE_VALUE (*value);
    *value = vivi_code_binary_new_name (left, VIVI_CODE_VALUE (right), "+");
    g_object_unref (left);
    g_object_unref (right);
  } while (TRUE);

  g_object_unref (*value);
  *value = NULL;

  return STATUS_FAIL;
}

static ParseStatus
parse_unary_expression (GScanner *scanner, ViviCodeValue **value)
{
  ViviCodeValue *tmp;

  *value = NULL;

  if (check_token (scanner, '!')) {
    parse_value (scanner, SYMBOL_UNARY_EXPRESSION, value);
    tmp = VIVI_CODE_VALUE (*value);
    *value = vivi_code_unary_new (tmp, '!');
    g_object_unref (tmp);
    return STATUS_OK;
  } else {
    return parse_value (scanner, SYMBOL_POSTFIX_EXPRESSION, value);
  }
}

static ParseStatus
parse_postfix_expression (GScanner *scanner, ViviCodeValue **value)
{
  ParseStatus status;

  status = parse_value (scanner, SYMBOL_LEFT_HAND_SIDE_EXPRESSION, value);
  if (status != STATUS_OK)
    return status;

  // FIXME: Don't allow new line here

  /*if (check_token (scanner, TOKEN_PLUSPLUS)) {
    ViviCodeValue *tmp = *value;
    *value = vivi_code_postfix_new (tmp, "++");
    g_object_unref (tmp);
  } else {*/
    return STATUS_OK;
  //}
}

static ParseStatus
parse_left_hand_side_expression (GScanner *scanner, ViviCodeValue **value)
{
  ParseStatus status;

  *value = NULL;

  status = parse_value (scanner, SYMBOL_NEW_EXPRESSION, value);
  if (status == STATUS_CANCEL)
    status = parse_value (scanner, SYMBOL_CALL_EXPRESSION, value);

  return status;
}

static ParseStatus
parse_new_expression (GScanner *scanner, ViviCodeValue **value)
{
  *value = NULL;

  if (check_token (scanner, TOKEN_NEW)) {
    if (parse_value (scanner, SYMBOL_NEW_EXPRESSION, value) != STATUS_OK)
      return STATUS_FAIL;
    if (!VIVI_IS_CODE_FUNCTION_CALL (*value)) {
      ViviCodeValue *tmp = VIVI_CODE_VALUE (*value);
      *value = vivi_code_function_call_new (NULL, tmp);
      g_object_unref (tmp);
    }
    vivi_code_function_call_set_construct (VIVI_CODE_FUNCTION_CALL (*value),
	TRUE);
    return STATUS_OK;
  } else {
    return parse_value (scanner, SYMBOL_MEMBER_EXPRESSION, value);
  }
}

static ParseStatus
parse_member_expression (GScanner *scanner, ViviCodeValue **value)
{
  ParseStatus status;
  ViviCodeValue *member;

  // TODO: new MemberExpression Arguments

  status = parse_value (scanner, SYMBOL_PRIMARY_EXPRESSION, value);
  if (status == STATUS_CANCEL)
    status = parse_value (scanner, SYMBOL_FUNCTION_EXPRESSION, value);

  if (status != 0)
    return status;

  do {
    ViviCodeValue *tmp;

    if (check_token (scanner, '[')) {
      if (parse_value (scanner, SYMBOL_EXPRESSION, &member) != STATUS_OK)
	return STATUS_FAIL;
      if (!check_token (scanner, ']'))
	return STATUS_FAIL;
    } else if (check_token (scanner, '.')) {
      if (parse_value (scanner, SYMBOL_IDENTIFIER, &member) != STATUS_OK)
	return STATUS_FAIL;
    } else {
      return STATUS_OK;
    }

    tmp = *value;
    *value = vivi_code_get_new (tmp, VIVI_CODE_VALUE (member));
    g_object_unref (tmp);
    g_object_unref (member);
  } while (TRUE);

  g_assert_not_reached ();
  return STATUS_FAIL;
}

static ParseStatus
parse_primary_expression (GScanner *scanner, ViviCodeValue **value)
{
  int i, status;
  ParseSymbol options[] = {
    SYMBOL_IDENTIFIER,
    SYMBOL_LITERAL,
    SYMBOL_ARRAY_LITERAL,
    SYMBOL_OBJECT_LITERAL,
    SYMBOL_NONE
  };

  *value = NULL;

  if (check_token (scanner, TOKEN_THIS)) {
    *value = vivi_code_get_new_name ("this");
    return STATUS_OK;
  }

  /*if (check_token (scanner, '(')) {
    return STATUS_OK;
  }*/

  for (i = 0; options[i] != SYMBOL_NONE; i++) {
    status = parse_value (scanner, options[i], value);
    if (status != STATUS_CANCEL)
      return status;
  }

  return STATUS_CANCEL;
}

// values

static ParseStatus
parse_identifier (GScanner *scanner, ViviCodeValue **value)
{
  *value = NULL;

  if (!check_token (scanner, G_TOKEN_IDENTIFIER))
    return STATUS_CANCEL;

  *value = vivi_code_get_new_name (scanner->value.v_identifier);

  return STATUS_OK;
}

static ParseStatus
parse_literal (GScanner *scanner, ViviCodeValue **value)
{
  *value = NULL;

  if (check_token (scanner, G_TOKEN_STRING)) {
    *value = vivi_code_constant_new_string (scanner->value.v_string);
    return STATUS_OK;
  } else if (check_token (scanner, G_TOKEN_FLOAT)) {
    *value = vivi_code_constant_new_number (scanner->value.v_float);
    return STATUS_OK;
  } else if (check_token (scanner, TOKEN_TRUE)) {
    *value = vivi_code_constant_new_boolean (TRUE);
    return STATUS_OK;
  } else if (check_token (scanner, TOKEN_FALSE)) {
    *value = vivi_code_constant_new_boolean (FALSE);
    return STATUS_OK;
  } else if (check_token (scanner, TOKEN_NULL)) {
    *value = vivi_code_constant_new_null ();
    return STATUS_OK;
  } else {
    return STATUS_CANCEL;
  }

  *value = vivi_code_get_new_name (scanner->value.v_identifier);

  return STATUS_OK;
}

// parsing

typedef struct {
  ParseSymbol			id;
  const char *			name;
  ParseStatementFunction	parse;
} SymbolStatementFunctionList;

static const SymbolStatementFunctionList statement_symbols[] = {
  // top
  { SYMBOL_PROGRAM, "Program", parse_program },
  { SYMBOL_SOURCE_ELEMENT, "SourceElement", parse_source_element },
  // function
  { SYMBOL_FUNCTION_DECLARATION, "FunctionDeclaration",
    parse_function_declaration },
  // statement
  { SYMBOL_STATEMENT, "Statement", parse_statement_symbol },
  { SYMBOL_BLOCK, "Block", parse_block },
  { SYMBOL_EMPTY_STATEMENT, "EmptyStatement", parse_empty_statement },
  { SYMBOL_EXPRESSION_STATEMENT, "ExpressionStatement",
    parse_expression_statement },
  // expression
  { SYMBOL_EXPRESSION, "Expression", parse_expression },
  { SYMBOL_ASSIGNMENT_EXPRESSION, "AssigmentExpression",
    parse_assignment_expression },
  { SYMBOL_CONDITIONAL_EXPRESSION, "ConditionalExpression",
    parse_conditional_expression },
  { SYMBOL_NONE, NULL, NULL }
};

typedef struct {
  ParseSymbol			id;
  const char *			name;
  ParseValueFunction		parse;
} SymbolValueFunctionList;

static const SymbolValueFunctionList value_symbols[] = {
  // expression
  { SYMBOL_OPERATOR_EXPRESSION, "OperatorExpression",
    parse_operator_expression },
  { SYMBOL_UNARY_EXPRESSION, "UnaryExpression", parse_unary_expression },
  { SYMBOL_POSTFIX_EXPRESSION, "PostfixExpression", parse_postfix_expression },
  { SYMBOL_LEFT_HAND_SIDE_EXPRESSION, "LeftHandSideExpression",
    parse_left_hand_side_expression },
  { SYMBOL_NEW_EXPRESSION, "NewExpression", parse_new_expression },
  { SYMBOL_MEMBER_EXPRESSION, "MemberExpression", parse_member_expression },
  { SYMBOL_PRIMARY_EXPRESSION, "PrimaryExpression", parse_primary_expression },
  // value
  { SYMBOL_IDENTIFIER, "Identifier", parse_identifier },
  { SYMBOL_LITERAL, "Literal", parse_literal },
  { SYMBOL_NONE, NULL, NULL }
};

static ParseStatus
parse_statement (GScanner *scanner, ParseSymbol symbol,
    ViviCodeStatement **statement)
{
  int i;

  g_return_val_if_fail (scanner != NULL, STATUS_FAIL);
  g_return_val_if_fail (symbol != SYMBOL_NONE, STATUS_FAIL);
  g_return_val_if_fail (statement != NULL, STATUS_FAIL);

  for (i = 0; statement_symbols[i].id != SYMBOL_NONE; i++) {
    if (statement_symbols[i].id == symbol) {
      ParseStatus status = statement_symbols[i].parse (scanner, statement);
      g_assert ((status == STATUS_OK && VIVI_IS_CODE_TOKEN (*statement)) ||
	  (status != STATUS_OK && *statement == NULL));
      return status;
    }
  }

  //g_assert_not_reached ();
  return STATUS_CANCEL;
}

static ParseStatus
parse_value (GScanner *scanner, ParseSymbol symbol, ViviCodeValue **value)
{
  int i;

  g_return_val_if_fail (scanner != NULL, STATUS_FAIL);
  g_return_val_if_fail (symbol != SYMBOL_NONE, STATUS_FAIL);
  g_return_val_if_fail (value != NULL, STATUS_FAIL);

  for (i = 0; value_symbols[i].id != SYMBOL_NONE; i++) {
    if (value_symbols[i].id == symbol) {
      ParseStatus status = value_symbols[i].parse (scanner, value);
      g_assert ((status == STATUS_OK && VIVI_IS_CODE_TOKEN (*value)) ||
	  (status != STATUS_OK && *value == NULL));
      return status;
    }
  }

  //g_assert_not_reached ();
  return STATUS_CANCEL;
}

static ParseStatus
parse_statement_list (GScanner *scanner, ParseSymbol symbol,
    ViviCodeStatement ***list, guint separator)
{
  GPtrArray *array;
  ViviCodeStatement *statement;
  ParseStatus status;

  g_return_val_if_fail (scanner != NULL, STATUS_FAIL);
  g_return_val_if_fail (symbol != SYMBOL_NONE, STATUS_FAIL);
  g_return_val_if_fail (list != NULL, STATUS_FAIL);

  status = parse_statement (scanner, symbol, &statement);
  if (status != STATUS_OK)
    return status;

  array = g_ptr_array_new ();

  do {
    g_ptr_array_add (array, statement);

    if (separator != G_TOKEN_NONE && !check_token (scanner, separator))
      break;

    status = parse_statement (scanner, symbol, &statement);
    if (status == STATUS_FAIL)
      return STATUS_FAIL;
  } while (status == STATUS_OK);
  g_ptr_array_add (array, NULL);

  *list = (ViviCodeStatement **)g_ptr_array_free (array, FALSE);

  return STATUS_OK;
}

static ParseStatus
parse_value_list (GScanner *scanner, ParseSymbol symbol, ViviCodeValue ***list,
    guint separator)
{
  GPtrArray *array;
  ViviCodeValue *value;
  ParseStatus status;

  g_return_val_if_fail (scanner != NULL, STATUS_FAIL);
  g_return_val_if_fail (symbol != SYMBOL_NONE, STATUS_FAIL);
  g_return_val_if_fail (list != NULL, STATUS_FAIL);

  status = parse_value (scanner, symbol, &value);
  if (status != STATUS_OK)
    return status;

  array = g_ptr_array_new ();

  do {
    g_ptr_array_add (array, value);

    if (separator != G_TOKEN_NONE && !check_token (scanner, separator))
      break;

    status = parse_value (scanner, symbol, &value);
    if (status == STATUS_FAIL)
      return STATUS_FAIL;
  } while (status == STATUS_OK);
  g_ptr_array_add (array, NULL);

  *list = (ViviCodeValue **)g_ptr_array_free (array, FALSE);

  return STATUS_OK;
}

// public

ViviCodeStatement *
vivi_compile_text (const char *text, gsize len, const char *input_name)
{
  GScanner *scanner;
  ViviCodeStatement *statement;
  ParseStatus status;

  g_return_val_if_fail (text != NULL, NULL);

  scanner = g_scanner_new (NULL);

  scanner->config->numbers_2_int = TRUE;
  scanner->config->int_2_float = TRUE;
  scanner->config->symbol_2_token = TRUE;
  // FIXME: Should allow other Unicode characters
  scanner->config->cset_identifier_first =
    g_strdup (G_CSET_A_2_Z G_CSET_a_2_z G_CSET_LATINS G_CSET_LATINC "_$");
  scanner->config->cset_identifier_nth = g_strdup (G_CSET_A_2_Z G_CSET_a_2_z
      G_CSET_LATINS G_CSET_LATINC "_$" G_CSET_DIGITS);
  scanner->config->scan_identifier_1char = TRUE;

  g_scanner_set_scope (scanner, 0);
  g_scanner_scope_add_symbol (scanner, 0, "function",
      GINT_TO_POINTER (TOKEN_FUNCTION));
  g_scanner_scope_add_symbol (scanner, 0, "++",
      GINT_TO_POINTER (TOKEN_PLUSPLUS));
  g_scanner_scope_add_symbol (scanner, 0, "--",
      GINT_TO_POINTER (TOKEN_MINUSMINUS));
  g_scanner_scope_add_symbol (scanner, 0, "new", GINT_TO_POINTER (TOKEN_NEW));

  scanner->input_name = input_name;
  g_scanner_input_text (scanner, text, len);

  status = parse_statement (scanner, SYMBOL_PROGRAM, &statement);
  g_assert ((status == STATUS_OK && VIVI_IS_CODE_STATEMENT (statement)) ||
	(status != STATUS_OK && statement == NULL));

  g_scanner_destroy (scanner);

  return statement;
}