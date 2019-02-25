#pragma once

#include "tao/pegtl.hpp"
#include "L3/grammar.h"

namespace grammar::IR {
  namespace peg = tao::pegtl;

  //
  // Comments
  //

  using comment = L3::comment;

  //
  // Utilities
  //

  namespace util {
    using namespace L3::util;
    template <typename content>
      struct bracketed : bookended<'[', content, ']'> {};
  }

  template <typename... Rules> using spaced = util::spaced<Rules...>;

  //
  // Characters
  //

  namespace character { using namespace L3::character; }

  //
  // Literals: Numbers, Identifiers, and Types
  //

  namespace literal { using namespace L3::literal; }
  namespace literal::identifier { using namespace L3::identifier; }

  namespace meta::literal::type {
    using suffix = spaced<util::bracketed<util::nothing>>;
    template <typename type>
      struct multiarray : spaced<type, peg::plus<suffix>> {};
  }

  namespace literal::type::scalar {
    struct code_  : TAOCPP_PEGTL_STRING("code")  {};
    struct int64_ : TAOCPP_PEGTL_STRING("int64") {};
    struct any    : peg::sor<code_, int64_> {};
  }

  namespace literal::type { // tuple
    struct tuple_ : TAOCPP_PEGTL_STRING("tuple") {};
  }

  namespace literal::type::multiarray {
    struct int64_ : meta::literal::type::multiarray<scalar::int64_> {};
    struct any    : peg::sor<int64_> {};
  }

  namespace literal::type::function {
    struct void_ : TAOCPP_PEGTL_STRING("void")  {};
    struct any : peg::sor<
      void_,
      tuple_,
      multiarray::any,
      scalar::any
    > {};
  }

  namespace literal::type::variable {
    // NOTE(jordan): ORDER MATTERS
    struct any : peg::sor<tuple_, multiarray::any, scalar::any> {};
  }

  namespace literal::object {
    struct array : TAO_PEGTL_STRING("Array") {};
    struct tuple : TAO_PEGTL_STRING("Tuple") {};
  }

  namespace literal::identifier {
    struct typed : spaced<literal::type::variable::any, variable> {};
  }

  namespace identifier = literal::identifier;

  //
  // Operators
  //

  namespace op { using namespace L3::op; }

  //
  // Operands
  //

  namespace operand {
    using namespace L3::operand;
    struct typed : literal::identifier::typed {};
    using accessor = util::bracketed<operand::value>;
    struct array_item : spaced<variable, peg::plus<accessor>> {};
  }

  namespace operand::list {
    using comma = character::comma;
    using argument   = L3::operand::list::argument;
    using arguments  = L3::operand::list::arguments;
    struct parameter  : spaced<operand::typed, peg::opt<comma>> {};
    struct parameters : peg::star<parameter> {};
  }

  //
  // Intrinsics
  //

  namespace literal::intrinsic {
    using print = L3::literal::intrinsic::print;
    using array_error = L3::literal::intrinsic::array_error;
    struct any : peg::sor<print, array_error> {};
  }

  //
  // Instruction Literals
  //

  namespace literal::instruction {
    using ret    = L3::literal::instruction::ret;
    using call   = L3::literal::instruction::call;
    using branch = L3::literal::instruction::branch;
    using define = L3::literal::instruction::define;
    struct new_   : TAOCPP_PEGTL_STRING("new")    {};
    struct length : TAOCPP_PEGTL_STRING("length") {};
  }

  //
  // Expressions
  //

  namespace meta::expression {
    namespace call = L3::meta::expression::call;
    namespace binop = L3::meta::expression::binop;
    namespace make {
      using new_ = IR::literal::instruction::new_;
      template <typename object, typename args>
        struct e : spaced<new_, object, args> {};
    }
  }

  namespace expression {
    namespace meta = meta::expression;
    namespace { // ops
      using any_aop = L3::expression::any_aop;
      using any_sop = L3::expression::any_sop;
      using any_cmp = L3::expression::any_cmp;
      using shift = L3::expression::shift;
      using arithmetic = L3::expression::arithmetic;
      using comparison = L3::expression::comparison;
    }
    namespace make {
      using Array = literal::object::array;
      using Tuple = literal::object::tuple;
      using arguments = util::parenthesized<operand::list::arguments>;
      struct array : meta::make::e<Array, arguments> {};
      struct tuple : meta::make::e<Tuple, arguments> {};
      struct any   : peg::sor<array, tuple> {};
    }
    struct length : spaced<
      literal::instruction::length,
      operand::variable,
      operand::value
    > {};
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
    namespace ret    = L3::meta::statement::ret;
    namespace gets   = L3::meta::statement::gets;
    namespace branch = L3::meta::statement::branch;
  }

  namespace statement {
    namespace meta = meta::statement;
    // Call is also a statement
    namespace call = expression::call;
    // Re-export meta::ret
    template <typename value>
      using ret = meta::ret::s<value>;
    // Re-export meta::gets
    template <typename d, typename s>
      using gets = meta::gets::s<d, s>;
    // Branches are now allowed to have literals, and are 2-armed
    template <typename value, typename... label>
      using branch = meta::branch::s<value, label...>;
  }

  //
  // Instructions
  //

  namespace instruction::define {
    struct label : spaced<operand::label> {};
  }

  namespace instruction::declare {
    struct variable : operand::typed {};
  }

  namespace instruction::assign::variable {
    namespace stmt = statement;
    namespace expr = expression;
    using variable = operand::variable;
    struct gets_new        : stmt::gets<variable, expr::make::any>     {};
    struct gets_call       : stmt::gets<variable, expr::call::any>     {};
    struct gets_shift      : stmt::gets<variable, expr::shift>         {};
    struct gets_length     : stmt::gets<variable, expr::length>        {};
    struct gets_movable    : stmt::gets<variable, operand::movable>    {};
    struct gets_arithmetic : stmt::gets<variable, expr::arithmetic>    {};
    struct gets_comparison : stmt::gets<variable, expr::comparison>    {};
    struct gets_array_item : stmt::gets<variable, operand::array_item> {};
  }

  namespace instruction::assign::array {
    namespace stmt = statement;
    namespace expr = expression;
    using movable = operand::movable;
    struct gets_movable : stmt::gets<operand::array_item, movable> {};
  }

  namespace instruction::ret {
    namespace stmt = statement;
    struct value   : stmt::ret<operand::value> {};
    struct nothing : stmt::ret<util::nothing> {};
  }

  namespace instruction::branch {
    namespace stmt = statement;
    using value = operand::value;
    using label = operand::label;
    struct if_else       : stmt::branch<value, label, label> {};
    struct unconditional : stmt::branch<label> {};
  }

  namespace instruction { // call
    struct call : statement::call::any {};
  }

  namespace instruction::basic_block {
    struct body : peg::sor<
      // NOTE(jordan): ORDER MATTERS
      instruction::declare::variable,
      instruction::assign::variable::gets_new,
      instruction::assign::variable::gets_call,
      instruction::assign::variable::gets_length,
      instruction::assign::variable::gets_array_item,
      instruction::assign::variable::gets_shift,
      instruction::assign::variable::gets_comparison,
      instruction::assign::variable::gets_arithmetic,
      instruction::assign::variable::gets_movable,
      instruction::assign::array::gets_movable,
      instruction::call
    > {};
    struct entry : peg::sor<
      instruction::define::label
    > {};
    struct terminator : peg::sor<
      // NOTE(jordan): ORDER MATTERS
      instruction::ret::value,
      instruction::ret::nothing,
      instruction::branch::if_else,
      instruction::branch::unconditional
    > {};
    struct landing_pad : peg::plus<spaced<entry>> {};
    struct launch_pad  : peg::plus<spaced<terminator>> {};
  }

  //
  // Basic Blocks, Function & Program
  //

  namespace function {
    struct basic_block : spaced<
      instruction::basic_block::landing_pad,
      peg::star<instruction::basic_block::body>,
      instruction::basic_block::launch_pad
    > {};
    struct basic_blocks : peg::plus<spaced<basic_block>> {};
    struct define : spaced<
      literal::instruction::define,
      literal::type::function::any,
      operand::label,
      util::parenthesized<operand::list::parameters>,
      util::braced<basic_blocks>
    > {};
  }

  struct program : peg::plus<function::define> {};
}
