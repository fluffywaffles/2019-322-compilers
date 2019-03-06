#pragma once

#include "tao/pegtl.hpp"
#include "IR/grammar.h"

namespace grammar::LA {
  namespace peg = tao::pegtl;

  //
  // Comments
  //

  using comment = IR::comment;

  //
  // Utilities
  //

  namespace util { using namespace IR::util; }
  template <typename... Rules> using spaced = util::spaced<Rules...>;

  //
  // Characters
  //

  namespace character { using namespace IR::character; }

  //
  // Literals: Numbers, Identifiers, Types
  //

  namespace meta::literal::type::multiarray {
    using namespace IR::meta::literal::type::multiarray;
  }
  namespace literal { using namespace IR::literal; }
  namespace literal::identifier { using namespace IR::identifier; }
  namespace identifier = literal::identifier;

  //
  // Intrinsics
  //

  namespace literal::intrinsic {
    using print = IR::literal::intrinsic::print;
    struct any : peg::sor<print> {};
  }

  //
  // Instruction literals
  //

  namespace literal::instruction {
    using ret    = IR::literal::instruction::ret;
    using new_   = IR::literal::instruction::new_;
    using branch = IR::literal::instruction::branch;
    using length = IR::literal::instruction::length;
  }

  //
  // Operators
  //

  namespace op { using namespace IR::op; }

  //
  // Operands
  //

  namespace operand {
    using label    = IR::operand::label;
    using number   = IR::operand::number;
    using variable = IR::operand::variable;
    struct name    : literal::identifier::name {};
    struct value   : peg::sor<name, number> {};
    struct movable : peg::sor<label, name, number> {};
    struct typed   : spaced<literal::type::variable::any, name> {};
    namespace array {
      struct accessor   : util::bracketed<operand::value> {};
      struct accessors  : peg::plus<accessor> {};
    }
    struct index : spaced<operand::name, array::accessors> {};
  }

  namespace operand::list {
    using comma = character::comma;
    // arguments are value operands
    struct argument  : spaced<operand::value, peg::opt<comma>> {};
    struct arguments : peg::star<argument> {};
    // parameters are typed operands
    struct parameter  : spaced<operand::typed, peg::opt<comma>> {};
    struct parameters : peg::star<parameter> {};
  }

  //
  // Expressions
  //

  namespace meta::expression {
    namespace make  = IR::meta::expression::make;
    namespace binop = L3::meta::expression::binop;
    namespace call {
      template <typename callable, typename args>
        struct e : spaced<callable, args> {};
    }
  }

  namespace expression {
    namespace meta = meta::expression;
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
    // name <- new {Array,Tuple}(args...)
    namespace make {
      using Array = literal::object::array;
      using Tuple = literal::object::tuple;
      using arguments = util::parenthesized<operand::list::arguments>;
      struct array : meta::make::e<Array, arguments> {};
      struct tuple : meta::make::e<Tuple, arguments> {};
      struct any   : peg::sor<array, tuple> {};
    }
    // name <- length name t
    struct length : spaced<
      literal::instruction::length,
      operand::variable,
      operand::value
    > {};
    // name(arguments)
    namespace call {
      using arguments = util::parenthesized<operand::list::arguments>;
      using any_intrinsic = literal::intrinsic::any;
      struct defined   : meta::call::e<operand::name, arguments> {};
      struct intrinsic : meta::call::e<any_intrinsic, arguments> {};
      struct any       : peg::sor<intrinsic, defined> {};
    }
  }

  //
  // Statements
  //

  namespace meta::statement {
    namespace ret    = IR::meta::statement::ret;
    namespace gets   = IR::meta::statement::gets;
    namespace branch = IR::meta::statement::branch;
  }

  namespace statement {
    namespace meta = meta::statement;
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
    struct label : operand::label {};
  }

  namespace instruction::declare {
    struct variable : operand::typed {};
  }

  namespace instruction::assign::variable {
    namespace stmt = statement;
    namespace expr = expression;
    using name = operand::name;
    struct gets_new        : stmt::gets<name, expr::make::any>  {};
    struct gets_call       : stmt::gets<name, expr::call::any>  {};
    struct gets_index      : stmt::gets<name, operand::index>   {};
    struct gets_shift      : stmt::gets<name, expr::shift>      {};
    struct gets_length     : stmt::gets<name, expr::length>     {};
    struct gets_movable    : stmt::gets<name, operand::movable> {};
    struct gets_arithmetic : stmt::gets<name, expr::arithmetic> {};
    struct gets_comparison : stmt::gets<name, expr::comparison> {};
  }

  namespace instruction::assign::array {
    namespace stmt = statement;
    namespace expr = expression;
    using movable = operand::movable;
    struct gets_movable : stmt::gets<operand::index, movable> {};
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

  namespace instruction {
    struct any : peg::sor<
      // NOTE(jordan): ORDER MATTERS
      instruction::define::label,
      instruction::ret::value,
      instruction::ret::nothing,
      instruction::branch::if_else,
      instruction::branch::unconditional,
      instruction::declare::variable,
      instruction::call,
      instruction::assign::variable::gets_new,
      instruction::assign::variable::gets_call,
      instruction::assign::variable::gets_length,
      instruction::assign::variable::gets_index,
      instruction::assign::variable::gets_shift,
      instruction::assign::variable::gets_comparison,
      instruction::assign::variable::gets_arithmetic,
      instruction::assign::variable::gets_movable,
      instruction::assign::array::gets_movable
    > {};
  }

  //
  // Functions & Program
  //

  namespace function {
    struct instructions : peg::plus<spaced<instruction::any>> {};
    struct define : spaced<
      literal::type::function::any,
      operand::name,
      util::parenthesized<operand::list::parameters>,
      util::braced<instructions>
    > {};
  }

  struct program : peg::plus<function::define> {};
}
