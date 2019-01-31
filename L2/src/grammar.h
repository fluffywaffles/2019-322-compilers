#pragma once

#include "./L1/grammar.h"

#include "tao/pegtl.hpp"

namespace L2::grammar {
  namespace peg  = tao::pegtl;

  namespace util = L1::grammar::util;
  using namespace util;

  //
  // Extended character set
  //

  inline namespace character {
    using namespace L1::grammar::character;
    struct percent : util::a<'%'> {};
  }

  //
  // Extended Identifiers
  //

  namespace literal::identifier {
    using namespace L1::grammar::literal::identifier;
    // var ::= %name
    struct var : peg::seq<percent, name> {};
  }
  namespace identifier = literal::identifier;

  //
  // Extended Literals
  //

  namespace literal { using namespace L1::grammar::literal; }

  //
  // Extended operators
  //

  namespace op { using namespace L1::grammar::op; }

  //
  // Extended register_set
  //

  namespace register_set {
    using namespace L1::grammar::register_set;
    using var = literal::identifier::var;
    // sx ::= <L1::sx> | var
    struct shift : peg::sor<var, L1::grammar::register_set::shift> {};
  }

  //
  // Extended Operands
  //

  namespace operand { using namespace L1::grammar::operand; }

  //
  // Extended Instruction Literals
  //

  namespace literal::instruction {
    using namespace L1::grammar::literal::instruction;
    struct stack_arg : TAO_PEGTL_STRING("stack-arg") {};
  }

  //
  // Extended expressions
  //

  namespace expression {
    using namespace L1::grammar::expression;
    // stack-arg M
    namespace stack_arg_ {
      using M         = literal::number::special::divisible_by8;
      using stack_arg = literal::instruction::stack_arg;
      struct e : spaced<stack_arg, M> {};
    }
    using stack_arg = stack_arg_::e;
  }

  //
  // Extended Instructions
  //

  namespace instruction { using namespace L1::grammar::instruction; }

  namespace instruction::assign::usable {
    using namespace L1::grammar::instruction::assign::usable;
    namespace expr = expression;
    // w <- stack-arg M
    struct gets_stack_arg : expr::gets<w, expr::stack_arg> {};
  }

  namespace instruction {
    struct any : peg::sor<
      // w <- stack-arg M
      assign::usable::gets_stack_arg,
      // any L1 instruction
      L1::grammar::instruction::any
    > {};
  }

  //
  // Redefine parts of function, program, & entry in the new L2 context
  //

  namespace function {
    using namespace L1::grammar::function;

    struct instructions : peg::plus<instruction::any> {};

    struct define : sexp<spaced<
      literal::identifier::label,
      arg_count,
      local_count,
      instructions
    >> {};
  }

  namespace program {
    struct functions : peg::plus<spaced<function::define>> {};

    struct define : sexp<spaced<
      literal::identifier::label,
      functions
    >> {};
  }

  struct entry : spaced<program::define> {};
}
