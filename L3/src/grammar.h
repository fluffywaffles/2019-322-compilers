#pragma once

#include "tao/pegtl.hpp"
#include "L2/grammar.h"

namespace grammar::L3 {
  namespace peg = tao::pegtl;

  //
  // Comments
  //

  using comment = L2::comment;

  //
  // Utilities
  //

  namespace util {
    using namespace L2::util;
    using nothing = peg::success;
    template <char left, typename content, char right>
      using bookended = spaced<util::a<left>, content, util::a<right>>;
    // Helper for wrapping lists in parentheses.
    template <typename content>
      struct parenthesized : bookended<'(', content, ')'> {};
    // Helper for wrapping statements in braces.
    template <typename content>
      struct braced : bookended<'{', content, '}'> {};
  }

  template <typename... Rules> using spaced = util::spaced<Rules...>;

  //
  // Characters
  //

  namespace character {
    using namespace L2::character;
    using comma = util::a<','>;
  }

  //
  // Literals: Numbers and Identifiers
  //

  namespace literal {
    namespace number {
      namespace integer = L2::literal::number::integer;
    }
    namespace identifier {
      using name     = L2::literal::identifier::name;
      using label    = L2::literal::identifier::label;
      using variable = L2::literal::identifier::variable;
    }
  }

  namespace identifier = literal::identifier;

  //
  // Operators (All of them are binary! That's easy.)
  //

  // Arithmetic operators
  //

  namespace op::binary::arithmetic {
    struct add         : TAO_PEGTL_STRING("+") {};
    struct subtract    : TAO_PEGTL_STRING("-") {};
    struct multiply    : TAO_PEGTL_STRING("*") {};
    struct bitwise_and : TAO_PEGTL_STRING("&") {};
    using any = peg::sor<add, subtract, multiply, bitwise_and>;
  }

  // Shift operators
  //

  namespace op::binary::shift {
    struct left  : TAO_PEGTL_STRING("<<") {};
    struct right : TAO_PEGTL_STRING(">>") {};
    using any = peg::sor<left, right>;
  }

  // Comparisons
  //

  namespace op::binary::comparison {
    struct less          : TAO_PEGTL_STRING("<")  {};
    struct greater       : TAO_PEGTL_STRING(">")  {};
    struct equal         : TAO_PEGTL_STRING("=")  {};
    struct less_equal    : TAO_PEGTL_STRING("<=") {};
    struct greater_equal : TAO_PEGTL_STRING(">=") {};
    using any = peg::sor<less_equal, greater_equal, less, greater, equal>;
  }

  namespace op::binary::assignment {
    using L2::op::binary::assignment::gets;
  }

  // Aliases
  //

  namespace op {
    using add           = binary::arithmetic::add;
    using subtract      = binary::arithmetic::subtract;
    using multiply      = binary::arithmetic::multiply;
    using bitwise_and   = binary::arithmetic::bitwise_and;
    using shift_left    = binary::shift::left;
    using shift_right   = binary::shift::right;
    using less          = binary::comparison::less;
    using greater       = binary::comparison::greater;
    using less_equal    = binary::comparison::less_equal;
    using greater_equal = binary::comparison::greater_equal;
    using equal         = binary::comparison::equal;
    using gets          = binary::assignment::gets;
  }

  //
  // Operands
  //

  namespace operand {
    using number = literal::number::integer::any;
    // label ::= :name
    // var   ::= %name
    // u     ::= var | label
    // t     ::= var | N
    // s     ::= t   | label
    struct label    : identifier::label {};
    struct variable : identifier::variable {};
    struct callable : peg::sor<label, variable> {};
    struct value    : peg::sor<variable, number> {};
    struct movable  : peg::sor<label, number, variable> {};
  }

  // Operand lists
  //

  namespace operand::list {
    using comma = character::comma;
    // args ::=   | t   | t(, t)*
    // argument  ::= <t>,*
    // arguments ::= <argument>*
    struct argument  : spaced<value, peg::opt<comma>> {};
    struct arguments : peg::star<argument> {};
    // vars ::=   | var | var(, var)*
    // variable  ::= <var>,*
    // variables ::= <variable>*
    struct variable  : spaced<variable, peg::opt<comma>> {};
    struct variables : peg::star<variable> {};
  }

  //
  // Intrinsics
  //

  namespace literal::intrinsic {
    using namespace L2::literal::intrinsic;
    using any = peg::sor<print, allocate, array_error>;
  }

  //
  // Instruction Literals
  //

  namespace literal::instruction {
    struct ret    : TAO_PEGTL_STRING("return") {};
    struct call   : TAO_PEGTL_STRING("call")   {};
    struct load   : TAO_PEGTL_STRING("load")   {};
    struct store  : TAO_PEGTL_STRING("store")  {};
    struct branch : TAO_PEGTL_STRING("br")     {};
    struct define : TAO_PEGTL_STRING("define") {};
  }

  //
  // Expressions
  //

  namespace meta::expression {
    namespace binop {
      // NOTE(jordan): binops in L3 are always the same type on each side
      template <typename op, typename operand>
        using e = L2::meta::statement::binop::s<operand, op, operand>;
    }
    namespace load {
      using load = literal::instruction::load;
      template <typename src> struct e : spaced<load, src> {};
    }
    namespace store {
      using store = literal::instruction::store;
      template <typename src> struct e : spaced<store, src> {};
    }
    namespace call {
      // NOTE(jordan): can pass peg::nothing for args to get empty list.
      using call = literal::instruction::call;
      template <typename callable, typename args>
        struct e : spaced<call, callable, args> {};
    }
  }

  namespace expression {
    namespace meta = meta::expression;
    // load  <var>
    // store <var>
    using load  = meta::load::e<operand::variable>;
    using store = meta::store::e<operand::variable>;
    // t <aop> t
    namespace {
      using any_aop = op::binary::arithmetic::any;
      using arithmetic = meta::binop::e<any_aop, operand::value>;
    }
    // t <sop> t
    namespace {
      using any_sop = op::binary::shift::any;
      using shift = meta::binop::e<any_sop, operand::value>;
    }
    // t <cmp> t
    namespace {
      using any_cmp = op::binary::comparison::any;
      using comparison = meta::binop::e<any_cmp, operand::value>;
    }
    // call <callee> (<args>)
    namespace call {
      using arguments = util::parenthesized<operand::list::arguments>;
      using any_intrinsic = literal::intrinsic::any;
      struct defined   : meta::call::e<operand::callable, arguments> {};
      struct intrinsic : meta::call::e<any_intrinsic, arguments> {};
      struct any       : peg::sor<intrinsic, defined> {};
    }
  }

  //
  // Statements
  //

  namespace meta::statement {
    namespace gets {
      /* NOTE(jordan): this is technically redundant, but more obvious
       * than if we using-imported L2::meta::statement::gets.
       */
      template <typename dest, typename src>
        struct s : spaced<dest, op::gets, src> {};
    }
    namespace branch {
      using branch = literal::instruction::branch;
      template <typename label, typename var>
        struct s : spaced<branch, var, label> {};
    }
    namespace ret {
      // NOTE(jordan): can pass peg::nothing for value to get void return.
      using ret = literal::instruction::ret;
      template <typename value> struct s : spaced<ret, value> {};
    }
  }

  namespace statement {
    namespace meta = meta::statement;
    // NOTE(jordan): call is also a statement.
    namespace call = expression::call;
    // Re-export meta::ret
    template <typename value>
      using ret = meta::ret::s<value>;
    // Re-export meta::gets
    template <typename d, typename s>
      using gets = meta::gets::s<d, s>;
    namespace branch {
      using label = operand::label;
      template <typename condition>
        struct on : meta::branch::s<label, condition> {};
    }
  }

  //
  // Instructions
  //

  namespace instruction::define {
    struct label : spaced<operand::label> {};
  }

  namespace instruction::assign::variable {
    namespace stmt = statement;
    namespace expr = expression;
    using variable = operand::variable;
    struct gets_load       : stmt::gets<variable, expr::load>       {};
    struct gets_call       : stmt::gets<variable, expr::call::any>  {};
    struct gets_shift      : stmt::gets<variable, expr::shift>      {};
    struct gets_movable    : stmt::gets<variable, operand::movable> {};
    struct gets_arithmetic : stmt::gets<variable, expr::arithmetic> {};
    struct gets_comparison : stmt::gets<variable, expr::comparison> {};
  }

  namespace instruction::assign::address {
    namespace stmt = statement;
    namespace expr = expression;
    struct gets_movable : stmt::gets<expr::store, operand::movable> {};
  }

  namespace instruction::ret {
    struct nothing : statement::ret<util::nothing>  {};
    struct value   : statement::ret<operand::value> {};
  }

  namespace instruction::branch {
    struct variable      : statement::branch::on<operand::variable> {};
    struct unconditional : statement::branch::on<util::nothing> {};
  }

  namespace instruction { // call
    struct call : statement::call::any {};
  }

  namespace instruction::context {
    // Context "body" instructions
    struct body : peg::sor<
      // Instructions with a unique prefix
      instruction::assign::address::gets_movable,
      instruction::ret::value,
      instruction::ret::nothing,
      instruction::call,
      // "gets" instructions
      // ... with unique rhs prefix
      instruction::assign::variable::gets_load,
      instruction::assign::variable::gets_call,
      // ... with rhs binary operator expression
      instruction::assign::variable::gets_arithmetic,
      instruction::assign::variable::gets_shift,
      instruction::assign::variable::gets_comparison,
      // ... with rhs is a simple movable value
      instruction::assign::variable::gets_movable
    > {};
    // Context "entrypoint" instructions
    struct entry : instruction::define::label {};
    // Context "terminator" instructions
    struct terminator : peg::sor<
      // branching instructions
      instruction::branch::variable,
      instruction::branch::unconditional
    > {};
    // Landing/launch pads (folded entries / terminators)
    struct landing_pad : peg::plus<spaced<entry>> {};
    struct launch_pad  : peg::plus<spaced<terminator>> {};
  }

  //
  // Context
  //

  namespace context {
    struct free : peg::plus<spaced<instruction::context::body>> {};
    struct empty : peg::sor<
      spaced<
        instruction::context::entry,
        instruction::context::terminator
       >,
      spaced<instruction::context::entry>,
      spaced<instruction::context::terminator>
    > {};
    struct started : spaced<
      instruction::context::entry,
      peg::plus<spaced<instruction::context::body>>
    > {};
    struct terminated : spaced<
      instruction::context::body,
      peg::plus<spaced<instruction::context::terminator>>
    > {};
    struct complete : spaced<
      instruction::context::entry,
      peg::plus<spaced<instruction::context::body>>,
      instruction::context::terminator
    > {};
    // NOTE(jordan): ORDER MATTERS: complete must come first
    struct any : peg::sor<complete, started, terminated, free, empty> {};
  }

  //
  // Function & Program
  //

  namespace function {
    struct contexts : peg::plus<spaced<context::any>> {};
    struct define : spaced<
      literal::instruction::define,
      operand::label,
      util::parenthesized<operand::list::arguments>,
      util::braced<contexts>
    > {};
  }

  struct program : peg::plus<function::define> {};
}
