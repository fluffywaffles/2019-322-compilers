#pragma once
#include <iostream>
#include "tao/pegtl/contrib/parse_tree.hpp"
#include "grammar.h"

namespace peg = tao::pegtl;

namespace parse_tree {
  using namespace peg::parse_tree;

  void print_node (const node & n, const std::string & s = "" ) {
    // detect the root node:
    if (n.is_root()) {
      std::cout << "ROOT" << std::endl;
    }
    else {
      if (n.has_content()) {
        std::cout
          << s << n.name()
          << " \"" << n.content() << "\""
          /* << " at " << n.begin() << " to " << n.end() */
          << std::endl;
      }
      else {
        std::cout
          << s << n.name()
          /* << " at " << n.begin() */
          << std::endl;
      }
    }
    // print all child nodes
    if (!n.children.empty()) {
      const auto s2 = s + "  ";
      for (auto & up : n.children) {
        print_node(*up, s2);
      }
    }
  }

  namespace filter {
    using namespace L1::grammar;
    template< typename Rule >
      using selector = selector<
        Rule,
        apply_store_content::to<
          arg_count,
          local_count,
          identifier::label,
          register_set::any,
          register_set::shift,
          register_set::usable,
          register_set::memory,
          op::add,
          op::less,
          op::equal,
          op::subtract,
          op::multiply,
          op::less_equal,
          op::shift_left,
          op::bitwise_and,
          op::shift_right,
          operand::movable,
          operand::callable,
          operand::comparable,
          literal::number::integer::any,
          literal::number::integer::positive,
          literal::number::integer::negative,
          instruction::invoke::call::arity,
          literal::number::special::scale,
          literal::number::special::divisible_by8>,
        apply_remove_content::to<
          expression::mem,
          expression::cmp,
          op::increment,
          op::decrement,
          op::address_at,
          literal::intrinsic::print,
          literal::intrinsic::allocate,
          literal::intrinsic::array_error,
          instruction::any,
          instruction::assign::usable::gets_movable,
          instruction::assign::usable::gets_relative,
          instruction::assign::relative::gets_movable,
          instruction::update::usable::arithmetic::comparable,
          instruction::update::usable::shift::shift_register,
          instruction::update::usable::shift::number,
          instruction::update::relative::arithmetic::add_comparable,
          instruction::update::relative::arithmetic::subtract_comparable,
          instruction::update::usable::arithmetic::add_relative,
          instruction::update::usable::arithmetic::subtract_relative,
          instruction::assign::usable::gets_comparison,
          instruction::jump::cjump::if_else,
          instruction::jump::cjump::when,
          instruction::define::label,
          instruction::jump::go2,
          instruction::invoke::ret,
          instruction::invoke::call::callable,
          instruction::invoke::call::intrinsic::print,
          instruction::invoke::call::intrinsic::allocate,
          instruction::invoke::call::intrinsic::array_error,
          instruction::update::usable::arithmetic::increment,
          instruction::update::usable::arithmetic::decrement,
          instruction::assign::usable::gets_address,
          program,
          functions,
          function,
          instructions>>;
  }
}
