/* Swfdec
 * Copyright (C) 2008 Benjamin Otte <otte@gnome.org>
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

#include "swfdec_abc_interpret.h"
#include "swfdec_abc_function.h"
#include "swfdec_as_context.h"
#include "swfdec_as_frame_internal.h"
#include "swfdec_as_internal.h"
#include "swfdec_as_stack.h"
#include "swfdec_bits.h"
#include "swfdec_debug.h"

G_BEGIN_DECLS

struct {
  const char *		name;
} opcodes[256] = {
  [SWFDEC_ABC_OPCODE_BREAKPOINT] = { "breakpoint" },
  [SWFDEC_ABC_OPCODE_NOP] = { "noop" },
  [SWFDEC_ABC_OPCODE_THROW] = { "throw" },
  [SWFDEC_ABC_OPCODE_GET_SUPER] = { "getsuper" },
  [SWFDEC_ABC_OPCODE_SET_SUPER] = { "setsuper" },
  [SWFDEC_ABC_OPCODE_DXNS] = { "dxns" },
  [SWFDEC_ABC_OPCODE_DXNS_LATE] = { "dxnslate" },
  [SWFDEC_ABC_OPCODE_KILL] = { "kill" },
  [SWFDEC_ABC_OPCODE_LABEL] = { "label" },
  [SWFDEC_ABC_OPCODE_IFNLT] = { "ifnlt" },
  [SWFDEC_ABC_OPCODE_IFNLE] = { "ifnle" },
  [SWFDEC_ABC_OPCODE_IFNGT] = { "ifngt" },
  [SWFDEC_ABC_OPCODE_IFNGE] = { "ifnge" },
  [SWFDEC_ABC_OPCODE_JUMP] = { "jump" },
  [SWFDEC_ABC_OPCODE_IFTRUE] = { "iftrue" },
  [SWFDEC_ABC_OPCODE_IFFALSE] = { "iffalse" },
  [SWFDEC_ABC_OPCODE_IFEQ] = { "ifeq" },
  [SWFDEC_ABC_OPCODE_IFNE] = { "ifne" },
  [SWFDEC_ABC_OPCODE_IFLT] = { "iflt" },
  [SWFDEC_ABC_OPCODE_IFLE] = { "ifle" },
  [SWFDEC_ABC_OPCODE_IFGT] = { "ifgt" },
  [SWFDEC_ABC_OPCODE_IFGE] = { "ifge" },
  [SWFDEC_ABC_OPCODE_IFSTRICTEQ] = { "ifstricteq" },
  [SWFDEC_ABC_OPCODE_IFSTRICTNE] = { "ifstrictne" },
  [SWFDEC_ABC_OPCODE_LOOKUP_SWITCH] = { "lookupswitch" },
  [SWFDEC_ABC_OPCODE_PUSH_WITH] = { "pushwith" },
  [SWFDEC_ABC_OPCODE_POP_SCOPE] = { "popscope" },
  [SWFDEC_ABC_OPCODE_NEXT_NAME] = { "nextname" },
  [SWFDEC_ABC_OPCODE_HAS_NEXT] = { "hasnext" },
  [SWFDEC_ABC_OPCODE_PUSH_NULL] = { "pushnull" },
  [SWFDEC_ABC_OPCODE_PUSH_UNDEFINED] = { "pushundefined" },
  [SWFDEC_ABC_OPCODE_NEXT_VALUE] = { "nextvalue" },
  [SWFDEC_ABC_OPCODE_PUSH_BYTE] = { "pushbyte" },
  [SWFDEC_ABC_OPCODE_PUSH_SHORT] = { "pushshort" },
  [SWFDEC_ABC_OPCODE_PUSH_TRUE] = { "pushtrue" },
  [SWFDEC_ABC_OPCODE_PUSH_FALSE] = { "pushfalse" },
  [SWFDEC_ABC_OPCODE_PUSH_NAN] = { "pushnan" },
  [SWFDEC_ABC_OPCODE_POP] = { "pop" },
  [SWFDEC_ABC_OPCODE_DUP] = { "dup" },
  [SWFDEC_ABC_OPCODE_SWAP] = { "swap" },
  [SWFDEC_ABC_OPCODE_PUSH_STRING] = { "pushstring" },
  [SWFDEC_ABC_OPCODE_PUSH_INT] = { "pushint" },
  [SWFDEC_ABC_OPCODE_PUSH_UINT] = { "pushuint" },
  [SWFDEC_ABC_OPCODE_PUSH_DOUBLE] = { "pushdouble" },
  [SWFDEC_ABC_OPCODE_PUSH_SCOPE] = { "pushscope" },
  [SWFDEC_ABC_OPCODE_PUSH_NAMESPACE] = { "pushnamespace" },
  [SWFDEC_ABC_OPCODE_HAS_NEXT2] = { "hasnext2" },
  [SWFDEC_ABC_OPCODE_NEW_FUNCTION] = { "newfunction" },
  [SWFDEC_ABC_OPCODE_CALL] = { "call" },
  [SWFDEC_ABC_OPCODE_CONSTRUCT] = { "construct" },
  [SWFDEC_ABC_OPCODE_CALL_METHOD] = { "callmethod" },
  [SWFDEC_ABC_OPCODE_CALL_STATIC] = { "callstatic" },
  [SWFDEC_ABC_OPCODE_CALL_SUPER] = { "callsuper" },
  [SWFDEC_ABC_OPCODE_CALL_PROPERTY] = { "callproperty" },
  [SWFDEC_ABC_OPCODE_RETURN_VOID] = { "returnvoid" },
  [SWFDEC_ABC_OPCODE_RETURN_VALUE] = { "returnvalue" },
  [SWFDEC_ABC_OPCODE_CONSTRUCT_SUPER] = { "constructsuper" },
  [SWFDEC_ABC_OPCODE_CONSTRUCT_PROP] = { "constructprop" },
  [SWFDEC_ABC_OPCODE_CALL_SUPER_ID] = { "callsuperid" },
  [SWFDEC_ABC_OPCODE_CALL_PROP_LEX] = { "callproplex" },
  [SWFDEC_ABC_OPCODE_CALL_INTERFACE] = { "callinterface" },
  [SWFDEC_ABC_OPCODE_CALL_SUPER_VOID] = { "callsupervoid" },
  [SWFDEC_ABC_OPCODE_CALL_PROP_VOID] = { "callpropvoid" },
  [SWFDEC_ABC_OPCODE_NEW_OBJECT] = { "newobject" },
  [SWFDEC_ABC_OPCODE_NEW_ARRAY] = { "newarray" },
  [SWFDEC_ABC_OPCODE_NEW_ACTIVATION] = { "newactivation" },
  [SWFDEC_ABC_OPCODE_NEW_CLASS] = { "newclass" },
  [SWFDEC_ABC_OPCODE_GET_DESCENDANTS] = { "getdescendants" },
  [SWFDEC_ABC_OPCODE_NEW_CATCH] = { "newcatch" },
  [SWFDEC_ABC_OPCODE_FIND_PROP_STRICT] = { "findpropstrict" },
  [SWFDEC_ABC_OPCODE_FIND_PROPERTY] = { "findproperty" },
  [SWFDEC_ABC_OPCODE_FIND_DEF] = { "finddef" },
  [SWFDEC_ABC_OPCODE_GET_LEX] = { "getlex" },
  [SWFDEC_ABC_OPCODE_SET_PROPERTY] = { "setproperty" },
  [SWFDEC_ABC_OPCODE_GET_LOCAL] = { "getlocal" },
  [SWFDEC_ABC_OPCODE_SET_LOCAL] = { "setlocal" },
  [SWFDEC_ABC_OPCODE_GET_GLOBAL_SCOPE] = { "getglobalscope" },
  [SWFDEC_ABC_OPCODE_GET_SCOPE_OBJECT] = { "getscopeobject" },
  [SWFDEC_ABC_OPCODE_GET_PROPERTY] = { "getproperty" },
  [SWFDEC_ABC_OPCODE_INIT_PROPERTY] = { "initproperty" },
  [SWFDEC_ABC_OPCODE_DELETE_PROPERTY] = { "deleteproperty" },
  [SWFDEC_ABC_OPCODE_GET_SLOT] = { "getslot" },
  [SWFDEC_ABC_OPCODE_SET_SLOT] = { "setslot" },
  [SWFDEC_ABC_OPCODE_GET_GLOBAL_SLOT] = { "getglobalslot" },
  [SWFDEC_ABC_OPCODE_SET_GLOBAL_SLOT] = { "setglobalslot" },
  [SWFDEC_ABC_OPCODE_CONVERT_S] = { "convert_s" },
  [SWFDEC_ABC_OPCODE_ESC_XELEM] = { "esc_xelem" },
  [SWFDEC_ABC_OPCODE_ESC_XATTR] = { "esc_xattr" },
  [SWFDEC_ABC_OPCODE_CONVERT_I] = { "convert_i" },
  [SWFDEC_ABC_OPCODE_CONVERT_U] = { "convert_u" },
  [SWFDEC_ABC_OPCODE_CONVERT_D] = { "convert_d" },
  [SWFDEC_ABC_OPCODE_CONVERT_B] = { "convert_b" },
  [SWFDEC_ABC_OPCODE_CONVERT_O] = { "convert_o" },
  [SWFDEC_ABC_OPCODE_CHECK_FILTER] = { "checkfilter" },
  [SWFDEC_ABC_OPCODE_COERCE] = { "coerce" },
  [SWFDEC_ABC_OPCODE_COERCE_B] = { "coerce_b" },
  [SWFDEC_ABC_OPCODE_COERCE_A] = { "coerce_a" },
  [SWFDEC_ABC_OPCODE_COERCE_I] = { "coerce_i" },
  [SWFDEC_ABC_OPCODE_COERCE_D] = { "coerce_d" },
  [SWFDEC_ABC_OPCODE_COERCE_S] = { "coerce_s" },
  [SWFDEC_ABC_OPCODE_AS_TYPE] = { "astype" },
  [SWFDEC_ABC_OPCODE_AS_TYPE_LATE] = { "astypelate" },
  [SWFDEC_ABC_OPCODE_COERCE_U] = { "coerce_u" },
  [SWFDEC_ABC_OPCODE_COERCE_O] = { "coerce_o" },
  [SWFDEC_ABC_OPCODE_NEGATE] = { "negate" },
  [SWFDEC_ABC_OPCODE_INCREMENT] = { "increment" },
  [SWFDEC_ABC_OPCODE_INC_LOCAL] = { "inclocal" },
  [SWFDEC_ABC_OPCODE_DECREMENT] = { "decrement" },
  [SWFDEC_ABC_OPCODE_DEC_LOCAL] = { "declocal" },
  [SWFDEC_ABC_OPCODE_TYPEOF] = { "typeof" },
  [SWFDEC_ABC_OPCODE_NOT] = { "not" },
  [SWFDEC_ABC_OPCODE_BITNOT] = { "bitnot" },
  [SWFDEC_ABC_OPCODE_CONCAT] = { "concat" },
  [SWFDEC_ABC_OPCODE_ADD_D] = { "add_d" },
  [SWFDEC_ABC_OPCODE_ADD] = { "add" },
  [SWFDEC_ABC_OPCODE_SUBTRACT] = { "subtract" },
  [SWFDEC_ABC_OPCODE_MULTIPLY] = { "multiply" },
  [SWFDEC_ABC_OPCODE_DIVIDE] = { "divide" },
  [SWFDEC_ABC_OPCODE_MODULO] = { "modulo" },
  [SWFDEC_ABC_OPCODE_LSHIFT] = { "lshift" },
  [SWFDEC_ABC_OPCODE_RSHIFT] = { "rshift" },
  [SWFDEC_ABC_OPCODE_URSHIFT] = { "urshift" },
  [SWFDEC_ABC_OPCODE_BITAND] = { "bitand" },
  [SWFDEC_ABC_OPCODE_BITOR] = { "bitor" },
  [SWFDEC_ABC_OPCODE_BITXOR] = { "bitxor" },
  [SWFDEC_ABC_OPCODE_EQUALS] = { "equals" },
  [SWFDEC_ABC_OPCODE_STRICT_EQUALS] = { "strictequals" },
  [SWFDEC_ABC_OPCODE_LESS_THAN] = { "lessthan" },
  [SWFDEC_ABC_OPCODE_LESS_EQUALS] = { "lessequals" },
  [SWFDEC_ABC_OPCODE_GREATER_THAN] = { "greaterthan" },
  [SWFDEC_ABC_OPCODE_GREATER_EQUALS] = { "greaterequals" },
  [SWFDEC_ABC_OPCODE_INSTANCE_OF] = { "instanceof" },
  [SWFDEC_ABC_OPCODE_IS_TYPE] = { "istype" },
  [SWFDEC_ABC_OPCODE_IS_TYPE_LATE] = { "istypelate" },
  [SWFDEC_ABC_OPCODE_IN] = { "in" },
  [SWFDEC_ABC_OPCODE_INCREMENT_I] = { "increment_i" },
  [SWFDEC_ABC_OPCODE_DECREMENT_I] = { "decrement_i" },
  [SWFDEC_ABC_OPCODE_INCLOCAL_I] = { "inclocal_i" },
  [SWFDEC_ABC_OPCODE_DECLOCAL_I] = { "declocal_i" },
  [SWFDEC_ABC_OPCODE_NEGATE_I] = { "negate_i" },
  [SWFDEC_ABC_OPCODE_ADD_I] = { "add_i" },
  [SWFDEC_ABC_OPCODE_SUBTRACT_I] = { "subtract_i" },
  [SWFDEC_ABC_OPCODE_MULTIPLY_I] = { "multiply_i" },
  [SWFDEC_ABC_OPCODE_GET_LOCAL_0] = { "getlocal0" },
  [SWFDEC_ABC_OPCODE_GET_LOCAL_1] = { "getlocal1" },
  [SWFDEC_ABC_OPCODE_GET_LOCAL_2] = { "getlocal2" },
  [SWFDEC_ABC_OPCODE_GET_LOCAL_3] = { "getlocal3" },
  [SWFDEC_ABC_OPCODE_SET_LOCAL_0] = { "setlocal0" },
  [SWFDEC_ABC_OPCODE_SET_LOCAL_1] = { "setlocal1" },
  [SWFDEC_ABC_OPCODE_SET_LOCAL_2] = { "setlocal2" },
  [SWFDEC_ABC_OPCODE_SET_LOCAL_3] = { "setlocal3" },
  [SWFDEC_ABC_OPCODE_ABS_JUMP] = { "absjump" },
  [SWFDEC_ABC_OPCODE_DEBUG] = { "debug" },
  [SWFDEC_ABC_OPCODE_DEBUG_LINE] = { "debugline" },
  [SWFDEC_ABC_OPCODE_DEBUG_FILE] = { "debugfile" },
  [SWFDEC_ABC_OPCODE_BREAKPOINT_LINE] = { "breakpointline" },
  [SWFDEC_ABC_OPCODE_TIMESTAMP] = { "timestamp" },
  [SWFDEC_ABC_OPCODE_VERIFY_PASS] = { "verifypass" },
  [SWFDEC_ABC_OPCODE_ALLOC] = { "alloc" },
  [SWFDEC_ABC_OPCODE_MARK] = { "mark" },
  [SWFDEC_ABC_OPCODE_WB] = { "wb" },
  [SWFDEC_ABC_OPCODE_PROLOGUE] = { "prologue" },
  [SWFDEC_ABC_OPCODE_SEND_ENTER] = { "sendenter" },
  [SWFDEC_ABC_OPCODE_DOUBLE_TO_ATOM] = { "doubletoatom" },
  [SWFDEC_ABC_OPCODE_SWEEP] = { "sweep" },
  [SWFDEC_ABC_OPCODE_CODEGEN] = { "codegen" },
  [SWFDEC_ABC_OPCODE_VERIFY] = { "verify" },
  [SWFDEC_ABC_OPCODE_DECODE] = { "decode" }
};

const char *
swfdec_abc_opcode_get_name (guint opcode)
{
  g_return_val_if_fail (opcode < 256, NULL);

  if (opcodes[opcode].name)
    return opcodes[opcode].name;
  return "unknown";
}

void
swfdec_abc_interpret (SwfdecAbcFunction *fun)
{
  SwfdecAsContext *context;
  SwfdecAsValue *locals, *scope;
  SwfdecAsFrame *frame;
  SwfdecBits bits;
  guint i, opcode;

  g_return_if_fail (SWFDEC_IS_ABC_FUNCTION (fun));
  g_return_if_fail (fun->verified);

  context = swfdec_gc_object_get_context (fun);
  frame = context->frame;

  if (!swfdec_as_context_check_continue (context))
    return;

  swfdec_as_stack_ensure_free (context, fun->n_locals + fun->n_scope + fun->n_stack);

  /* init local variables */
  locals = context->cur;
  SWFDEC_AS_VALUE_SET_OBJECT (swfdec_as_stack_push (context), frame->thisp);
  for (i = 0; i < MIN (frame->argc, fun->n_args); i++) {
    *swfdec_as_stack_push (context) = frame->argv[i];
  }
  for (; i < fun->n_args; i++) {
    *swfdec_as_stack_push (context) = fun->args[i].default_value;
  }
  if (fun->need_rest) {
    SWFDEC_FIXME ("implement rest argument");
  } else if (fun->need_arguments) {
    SWFDEC_FIXME ("implement arguments argument");
  }
  for (; i < fun->n_locals; i++) {
    SWFDEC_AS_VALUE_SET_UNDEFINED (swfdec_as_stack_push (context));
  }

  /* init scope */
  scope = context->cur;
  for (i = 0; i < fun->n_scope; i++) {
    SWFDEC_AS_VALUE_SET_UNDEFINED (swfdec_as_stack_push (context));
  }

  /* init pc */
  swfdec_bits_init (&bits, fun->code);
  g_print ("%u bytes of code\n", fun->code->length);
  while (TRUE) {
    opcode = swfdec_bits_get_u8 (&bits);
    switch (opcode) {
      case SWFDEC_ABC_OPCODE_BREAKPOINT:
      case SWFDEC_ABC_OPCODE_NOP:
      case SWFDEC_ABC_OPCODE_THROW:
      case SWFDEC_ABC_OPCODE_GET_SUPER:
      case SWFDEC_ABC_OPCODE_SET_SUPER:
      case SWFDEC_ABC_OPCODE_DXNS:
      case SWFDEC_ABC_OPCODE_DXNS_LATE:
      case SWFDEC_ABC_OPCODE_KILL:
      case SWFDEC_ABC_OPCODE_LABEL:
      case SWFDEC_ABC_OPCODE_IFNLT:
      case SWFDEC_ABC_OPCODE_IFNLE:
      case SWFDEC_ABC_OPCODE_IFNGT:
      case SWFDEC_ABC_OPCODE_IFNGE:
      case SWFDEC_ABC_OPCODE_JUMP:
      case SWFDEC_ABC_OPCODE_IFTRUE:
      case SWFDEC_ABC_OPCODE_IFFALSE:
      case SWFDEC_ABC_OPCODE_IFEQ:
      case SWFDEC_ABC_OPCODE_IFNE:
      case SWFDEC_ABC_OPCODE_IFLT:
      case SWFDEC_ABC_OPCODE_IFLE:
      case SWFDEC_ABC_OPCODE_IFGT:
      case SWFDEC_ABC_OPCODE_IFGE:
      case SWFDEC_ABC_OPCODE_IFSTRICTEQ:
      case SWFDEC_ABC_OPCODE_IFSTRICTNE:
      case SWFDEC_ABC_OPCODE_LOOKUP_SWITCH:
      case SWFDEC_ABC_OPCODE_PUSH_WITH:
      case SWFDEC_ABC_OPCODE_POP_SCOPE:
      case SWFDEC_ABC_OPCODE_NEXT_NAME:
      case SWFDEC_ABC_OPCODE_HAS_NEXT:
      case SWFDEC_ABC_OPCODE_PUSH_NULL:
      case SWFDEC_ABC_OPCODE_PUSH_UNDEFINED:
      case SWFDEC_ABC_OPCODE_NEXT_VALUE:
      case SWFDEC_ABC_OPCODE_PUSH_BYTE:
      case SWFDEC_ABC_OPCODE_PUSH_SHORT:
      case SWFDEC_ABC_OPCODE_PUSH_TRUE:
      case SWFDEC_ABC_OPCODE_PUSH_FALSE:
      case SWFDEC_ABC_OPCODE_PUSH_NAN:
      case SWFDEC_ABC_OPCODE_POP:
      case SWFDEC_ABC_OPCODE_DUP:
      case SWFDEC_ABC_OPCODE_SWAP:
      case SWFDEC_ABC_OPCODE_PUSH_STRING:
      case SWFDEC_ABC_OPCODE_PUSH_INT:
      case SWFDEC_ABC_OPCODE_PUSH_UINT:
      case SWFDEC_ABC_OPCODE_PUSH_DOUBLE:
      case SWFDEC_ABC_OPCODE_PUSH_SCOPE:
      case SWFDEC_ABC_OPCODE_PUSH_NAMESPACE:
      case SWFDEC_ABC_OPCODE_HAS_NEXT2:
      case SWFDEC_ABC_OPCODE_NEW_FUNCTION:
      case SWFDEC_ABC_OPCODE_CALL:
      case SWFDEC_ABC_OPCODE_CONSTRUCT:
      case SWFDEC_ABC_OPCODE_CALL_METHOD:
      case SWFDEC_ABC_OPCODE_CALL_STATIC:
      case SWFDEC_ABC_OPCODE_CALL_SUPER:
      case SWFDEC_ABC_OPCODE_CALL_PROPERTY:
      case SWFDEC_ABC_OPCODE_RETURN_VOID:
      case SWFDEC_ABC_OPCODE_RETURN_VALUE:
      case SWFDEC_ABC_OPCODE_CONSTRUCT_SUPER:
      case SWFDEC_ABC_OPCODE_CONSTRUCT_PROP:
      case SWFDEC_ABC_OPCODE_CALL_SUPER_ID:
      case SWFDEC_ABC_OPCODE_CALL_PROP_LEX:
      case SWFDEC_ABC_OPCODE_CALL_INTERFACE:
      case SWFDEC_ABC_OPCODE_CALL_SUPER_VOID:
      case SWFDEC_ABC_OPCODE_CALL_PROP_VOID:
      case SWFDEC_ABC_OPCODE_NEW_OBJECT:
      case SWFDEC_ABC_OPCODE_NEW_ARRAY:
      case SWFDEC_ABC_OPCODE_NEW_ACTIVATION:
      case SWFDEC_ABC_OPCODE_NEW_CLASS:
      case SWFDEC_ABC_OPCODE_GET_DESCENDANTS:
      case SWFDEC_ABC_OPCODE_NEW_CATCH:
      case SWFDEC_ABC_OPCODE_FIND_PROP_STRICT:
      case SWFDEC_ABC_OPCODE_FIND_PROPERTY:
      case SWFDEC_ABC_OPCODE_FIND_DEF:
      case SWFDEC_ABC_OPCODE_GET_LEX:
      case SWFDEC_ABC_OPCODE_SET_PROPERTY:
      case SWFDEC_ABC_OPCODE_GET_LOCAL:
      case SWFDEC_ABC_OPCODE_SET_LOCAL:
      case SWFDEC_ABC_OPCODE_GET_GLOBAL_SCOPE:
      case SWFDEC_ABC_OPCODE_GET_SCOPE_OBJECT:
      case SWFDEC_ABC_OPCODE_GET_PROPERTY:
      case SWFDEC_ABC_OPCODE_INIT_PROPERTY:
      case SWFDEC_ABC_OPCODE_DELETE_PROPERTY:
      case SWFDEC_ABC_OPCODE_GET_SLOT:
      case SWFDEC_ABC_OPCODE_SET_SLOT:
      case SWFDEC_ABC_OPCODE_GET_GLOBAL_SLOT:
      case SWFDEC_ABC_OPCODE_SET_GLOBAL_SLOT:
      case SWFDEC_ABC_OPCODE_CONVERT_S:
      case SWFDEC_ABC_OPCODE_ESC_XELEM:
      case SWFDEC_ABC_OPCODE_ESC_XATTR:
      case SWFDEC_ABC_OPCODE_CONVERT_I:
      case SWFDEC_ABC_OPCODE_CONVERT_U:
      case SWFDEC_ABC_OPCODE_CONVERT_D:
      case SWFDEC_ABC_OPCODE_CONVERT_B:
      case SWFDEC_ABC_OPCODE_CONVERT_O:
      case SWFDEC_ABC_OPCODE_CHECK_FILTER:
      case SWFDEC_ABC_OPCODE_COERCE:
      case SWFDEC_ABC_OPCODE_COERCE_B:
      case SWFDEC_ABC_OPCODE_COERCE_A:
      case SWFDEC_ABC_OPCODE_COERCE_I:
      case SWFDEC_ABC_OPCODE_COERCE_D:
      case SWFDEC_ABC_OPCODE_COERCE_S:
      case SWFDEC_ABC_OPCODE_AS_TYPE:
      case SWFDEC_ABC_OPCODE_AS_TYPE_LATE:
      case SWFDEC_ABC_OPCODE_COERCE_U:
      case SWFDEC_ABC_OPCODE_COERCE_O:
      case SWFDEC_ABC_OPCODE_NEGATE:
      case SWFDEC_ABC_OPCODE_INCREMENT:
      case SWFDEC_ABC_OPCODE_INC_LOCAL:
      case SWFDEC_ABC_OPCODE_DECREMENT:
      case SWFDEC_ABC_OPCODE_DEC_LOCAL:
      case SWFDEC_ABC_OPCODE_TYPEOF:
      case SWFDEC_ABC_OPCODE_NOT:
      case SWFDEC_ABC_OPCODE_BITNOT:
      case SWFDEC_ABC_OPCODE_CONCAT:
      case SWFDEC_ABC_OPCODE_ADD_D:
      case SWFDEC_ABC_OPCODE_ADD:
      case SWFDEC_ABC_OPCODE_SUBTRACT:
      case SWFDEC_ABC_OPCODE_MULTIPLY:
      case SWFDEC_ABC_OPCODE_DIVIDE:
      case SWFDEC_ABC_OPCODE_MODULO:
      case SWFDEC_ABC_OPCODE_LSHIFT:
      case SWFDEC_ABC_OPCODE_RSHIFT:
      case SWFDEC_ABC_OPCODE_URSHIFT:
      case SWFDEC_ABC_OPCODE_BITAND:
      case SWFDEC_ABC_OPCODE_BITOR:
      case SWFDEC_ABC_OPCODE_BITXOR:
      case SWFDEC_ABC_OPCODE_EQUALS:
      case SWFDEC_ABC_OPCODE_STRICT_EQUALS:
      case SWFDEC_ABC_OPCODE_LESS_THAN:
      case SWFDEC_ABC_OPCODE_LESS_EQUALS:
      case SWFDEC_ABC_OPCODE_GREATER_THAN:
      case SWFDEC_ABC_OPCODE_GREATER_EQUALS:
      case SWFDEC_ABC_OPCODE_INSTANCE_OF:
      case SWFDEC_ABC_OPCODE_IS_TYPE:
      case SWFDEC_ABC_OPCODE_IS_TYPE_LATE:
      case SWFDEC_ABC_OPCODE_IN:
      case SWFDEC_ABC_OPCODE_INCREMENT_I:
      case SWFDEC_ABC_OPCODE_DECREMENT_I:
      case SWFDEC_ABC_OPCODE_INCLOCAL_I:
      case SWFDEC_ABC_OPCODE_DECLOCAL_I:
      case SWFDEC_ABC_OPCODE_NEGATE_I:
      case SWFDEC_ABC_OPCODE_ADD_I:
      case SWFDEC_ABC_OPCODE_SUBTRACT_I:
      case SWFDEC_ABC_OPCODE_MULTIPLY_I:
      case SWFDEC_ABC_OPCODE_GET_LOCAL_0:
      case SWFDEC_ABC_OPCODE_GET_LOCAL_1:
      case SWFDEC_ABC_OPCODE_GET_LOCAL_2:
      case SWFDEC_ABC_OPCODE_GET_LOCAL_3:
      case SWFDEC_ABC_OPCODE_SET_LOCAL_0:
      case SWFDEC_ABC_OPCODE_SET_LOCAL_1:
      case SWFDEC_ABC_OPCODE_SET_LOCAL_2:
      case SWFDEC_ABC_OPCODE_SET_LOCAL_3:
      case SWFDEC_ABC_OPCODE_ABS_JUMP:
      case SWFDEC_ABC_OPCODE_DEBUG:
      case SWFDEC_ABC_OPCODE_DEBUG_LINE:
      case SWFDEC_ABC_OPCODE_DEBUG_FILE:
      case SWFDEC_ABC_OPCODE_BREAKPOINT_LINE:
      case SWFDEC_ABC_OPCODE_TIMESTAMP:
      case SWFDEC_ABC_OPCODE_VERIFY_PASS:
      case SWFDEC_ABC_OPCODE_ALLOC:
      case SWFDEC_ABC_OPCODE_MARK:
      case SWFDEC_ABC_OPCODE_WB:
      case SWFDEC_ABC_OPCODE_PROLOGUE:
      case SWFDEC_ABC_OPCODE_SEND_ENTER:
      case SWFDEC_ABC_OPCODE_DOUBLE_TO_ATOM:
      case SWFDEC_ABC_OPCODE_SWEEP:
      case SWFDEC_ABC_OPCODE_CODEGEN:
      case SWFDEC_ABC_OPCODE_VERIFY:
      case SWFDEC_ABC_OPCODE_DECODE:
      default:
	SWFDEC_ERROR ("opcode %02X %s not implemented",
	    opcode, swfdec_abc_opcode_get_name (opcode));
	goto out;
    }
  }

out:
  swfdec_as_frame_return (frame, NULL);
}

