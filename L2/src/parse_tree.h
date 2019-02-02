#pragma once
#include <iostream>
#include "tao/pegtl/contrib/parse_tree.hpp"
#include "grammar.h"

/*
 * NOTE(jordan): really needed to cut the gordian knot here, so this
 * is based on a character-for-character copy of L1/parse_tree.h.
 *
 * Ideally it could be simplified into a version where we save a generic
 * L1Instruction node in the parse tree. We could then handle all the L1
 * instructions by reemitting them without modification in a single case.
 */

namespace peg = tao::pegtl;

namespace L2::parse_tree {
  using namespace peg::parse_tree;

  void print_node (const node & n, const std::string & indent = "") {
    // node cases
    if (n.is_root()) {
      std::cout << "ROOT\n";
    } else if (n.has_content()) {
      std::cout << indent << n.name() << " \"" << n.content() << "\"\n";
    } else {
      std::cout << indent << n.name() << "\n";
    }
    // recursion
    if (!n.children.empty()) {
      for (auto & up : n.children) print_node(*up, "  " + indent);
    }
  }

  namespace filter {
    using namespace L2::grammar;
    template <typename Rule>
      using selector = selector<
        Rule,
        apply_store_content::to<
          literal::number::integer::any,
          literal::number::integer::positive,
          literal::number::integer::negative,
          literal::number::special::scale,
          literal::number::special::divisible_by8,
          identifier::name,
          op::less,
          op::equal,
          op::less_equal,
          operand::shift,
          operand::assignable,
          operand::memory,
          operand::movable,
          operand::callable,
          operand::comparable,
          function::arg_count,
          function::local_count>,
        apply_remove_content::to<
          literal::intrinsic::print,
          literal::intrinsic::allocate,
          literal::intrinsic::array_error,
          identifier::label,
          identifier::variable,
          op::add,
          op::subtract,
          op::multiply,
          op::increment,
          op::decrement,
          op::address_at,
          op::shift_left,
          op::bitwise_and,
          op::shift_right,
          expression::mem,
          expression::cmp,
          expression::stack_arg,
          instruction::any,
          instruction::assign::assignable::gets_movable,
          instruction::assign::assignable::gets_relative,
          instruction::assign::assignable::gets_stack_arg,
          instruction::assign::relative::gets_movable,
          instruction::update::assignable::arithmetic::comparable,
          instruction::update::assignable::shift::shift,
          instruction::update::assignable::shift::number,
          instruction::update::relative::arithmetic::add_comparable,
          instruction::update::relative::arithmetic::subtract_comparable,
          instruction::update::assignable::arithmetic::add_relative,
          instruction::update::assignable::arithmetic::subtract_relative,
          instruction::assign::assignable::gets_comparison,
          instruction::jump::cjump::if_else,
          instruction::jump::cjump::when,
          instruction::define::label,
          instruction::jump::go2,
          instruction::invoke::ret,
          instruction::invoke::call::callable,
          instruction::invoke::call::intrinsic::print,
          instruction::invoke::call::intrinsic::allocate,
          instruction::invoke::call::intrinsic::array_error,
          instruction::update::assignable::arithmetic::increment,
          instruction::update::assignable::arithmetic::decrement,
          instruction::assign::assignable::gets_address,
          function::instructions,
          function::define,
          program::functions,
          program::define>>;
  }
}
