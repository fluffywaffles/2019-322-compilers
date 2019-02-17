#pragma once
#include <iostream>

#include "tao/pegtl/contrib/parse_tree.hpp"
#include "tao/pegtl/contrib/tracer.hpp"

#include "L2/ast.h"
#include "grammar.h"

namespace peg = tao::pegtl;

namespace ast::L3 {
  using namespace peg::parse_tree;

  void print_node (node const & n, std::string const & indent = "") {
    ast::L2::print_node(n, indent);
  }

  namespace filter {
    using namespace grammar::L3;
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
          expression::load,
          expression::store,
          expression::arithmetic,
          expression::shift,
          expression::comparison,
          expression::call::any,
          expression::call::defined,
          expression::call::intrinsic,
          /* NOTE(jordan): we can write more generic code by keeping
           * around the contents of operators. Otherwise we're forced to
           * reconstruct their string representations, which is silly. The
           * same goes for intrinsic function names.
           */
          op::gets,
          // comparison
          op::less,
          op::equal,
          op::greater,
          op::less_equal,
          op::greater_equal,
          // arithmetic
          op::add,
          op::subtract,
          op::multiply,
          op::bitwise_and,
          // shift
          op::shift_left,
          op::shift_right,
          // intrinsic literals
          literal::intrinsic::print,
          literal::intrinsic::allocate,
          literal::intrinsic::array_error,
          /* NOTE(jordan): we want to do later string matching against
           * operands, so we need to keep their contents around even
           * though it's redundant. It makes the code easier.
           */
          operand::label,
          operand::variable,
          operand::callable,
          operand::value,
          operand::movable,
          operand::list::argument,
          operand::list::variable
        >,
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
          context::free,
          context::empty,
          context::started,
          context::terminated,
          context::complete,
          instruction::assign::variable::gets_movable,
          instruction::assign::variable::gets_comparison,
          instruction::assign::variable::gets_load,
          instruction::assign::variable::gets_call,
          instruction::assign::variable::gets_arithmetic,
          instruction::assign::variable::gets_shift,
          instruction::assign::address::gets_movable,
          instruction::branch::variable,
          instruction::branch::unconditional,
          instruction::define::label,
          instruction::ret::nothing,
          instruction::ret::value,
          instruction::call,
          // Argument and variable lists are redundant with their contents
          operand::list::arguments,
          operand::list::variables,
          // Function containers are just containers
          function::contexts,
          function::define,
          // Same with program; just a list of functions
          program
        >
      >;
  }
}

namespace ast::L3::debug {
  template <
    typename Entry,
    template <typename...> class Selector = filter::selector,
    typename Input
  > auto trace (Input & in) {
    return parse<Entry, Selector, peg::nothing, peg::tracer>(in);
  }
}
