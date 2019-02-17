#pragma once
#include <iostream>

#include "tao/pegtl/contrib/parse_tree.hpp"
#include "tao/pegtl/contrib/tracer.hpp"

#include "grammar.h"

/*
 * NOTE(jordan): really needed to cut the gordian knot here, so this
 * is based on a character-for-character copy of L1/ast.h.
 *
 * Ideally it could be simplified into a version where we save a generic
 * L1Instruction node in the parse tree. We could then handle all the L1
 * instructions by reemitting them without modification in a single case.
 */

namespace peg = tao::pegtl;

namespace ast::L2 {
  using namespace peg::parse_tree;

  template <typename Node>
  void print_node (
    basic_node<Node> const & n,
    std::string const & indent = ""
  ) {
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
    using namespace grammar::L2;
    template <typename Rule>
      using selector = selector<
        Rule,
        apply_store_content::to<
          /* NOTE(jordan): keep around literal nodes' contents so that we
           * can always unwrap their parent operands and operate on the
           * literals directly.
           */
          literal::number::integer::any,
          literal::number::integer::positive,
          literal::number::integer::negative,
          literal::number::special::scale,
          literal::number::special::divisible_by8,
          identifier::x86_64_register::rax,
          identifier::x86_64_register::rbx,
          identifier::x86_64_register::rcx,
          identifier::x86_64_register::rdx,
          identifier::x86_64_register::rsi,
          identifier::x86_64_register::rdi,
          identifier::x86_64_register::rbp,
          identifier::x86_64_register::rsp,
          identifier::x86_64_register::r8,
          identifier::x86_64_register::r9,
          identifier::x86_64_register::r10,
          identifier::x86_64_register::r11,
          identifier::x86_64_register::r12,
          identifier::x86_64_register::r13,
          identifier::x86_64_register::r14,
          identifier::x86_64_register::r15,
          /* NOTE(jordan): if we store both the labels/variables and their
           * name, we don't have to parse the name out later: the name
           * (without a prefix) is the label/variable child.
           */
          identifier::name,
          identifier::label,
          identifier::variable,
          /* NOTE(jordan): we don't strictly *need* to keep around the
           * contents of expressions, because they're easy to reconstruct,
           * but the redundant storage is worth the insurance /
           * debuggability.
           */
          expression::comparison,
          expression::memory,
          expression::stack_arg,
          /* NOTE(jordan): we can write more generic code by keeping
           * around the contents of operators. Otherwise we're forced to
           * reconstruct their string representations, which is silly. The
           * same goes for intrinsic function names.
           */
          op::gets,
          op::less,
          op::equal,
          op::less_equal,
          op::add,
          op::subtract,
          op::multiply,
          op::increment,
          op::decrement,
          op::address_at,
          op::shift_left,
          op::bitwise_and,
          op::shift_right,
          literal::intrinsic::print,
          literal::intrinsic::allocate,
          literal::intrinsic::array_error,
          /* NOTE(jordan): we want to do later string matching against
           * operands, so we need to keep their contents around even
           * though it's redundant. It makes the code easier.
           */
          operand::shift,
          operand::assignable,
          operand::memory,
          operand::movable,
          operand::callable,
          operand::comparable,
          function::arg_count,
          function::local_count>,
        apply_remove_content::to<
          /* NOTE(jordan): these nodes are just containers; they don't
           * have any content of their own. Keeping their content would be
           * purely redundant.
           */
          /* NOTE(jordan): THAT SAID, the way PEGTL nodes "hold" content
           * is with a pair of iterators into a single referenced source.
           * So, it would not take up any additional memory to store the
           * contents of instructions. It would just clutter the output of
           * an AST print dump. (A lot, in fact.)
           */
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

namespace ast::L2::debug {
  template <
    typename Entry,
    template <typename...> class Selector = filter::selector,
    typename Input
  > auto trace (Input & in) {
    return parse<Entry, Selector, peg::nothing, peg::tracer>(in);
  }
}
