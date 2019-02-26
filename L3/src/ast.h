#pragma once
#include <iostream>

#include "tao/pegtl/contrib/parse_tree.hpp"
#include "tao/pegtl/contrib/tracer.hpp"

#include "L2/ast.h"
#include "grammar.h"

namespace peg = tao::pegtl;

namespace ast {
  template <typename Statistic, typename Collection, typename Result>
  void walk (Collection const & nodes, Result & result) {
    for (typename Collection::value_type const & node : nodes) {
      bool walk_children = Statistic::compute(*node, result);
      if (walk_children) walk<Statistic>(node->children, result);
    }
  }
}

namespace ast::L3::filter {
  using namespace grammar::L3;
  template <typename Rule>
    using selector = peg::parse_tree::selector<
      Rule,
      peg::parse_tree::apply_store_content::to<
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
        operand::movable
      >,
      peg::parse_tree::apply_remove_content::to<
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
        operand::list::argument,
        operand::list::parameter,
        operand::list::arguments,
        operand::list::parameters,
        // Function containers are just containers
        function::contexts,
        function::define,
        // Same with program; just a list of functions
        program
      >
    >;
}

namespace ast::L3 {
  template <
    typename Entry,
    template <class...> class Selector = filter::selector,
    typename Input
  > up_node parse (Input & in) {
    return peg::parse_tree::parse<Entry, node, Selector>(in);
  }
}

namespace ast::L3::debug {
  void print_node (node const & n, std::string const & indent = "") {
    ast::L2::print_node(n, indent);
  }
  template <
    typename Entry,
    template <typename...> class Selector = filter::selector,
    typename Input
  > auto trace (Input & in) {
    return peg::parse_tree::parse<
      Entry,
      node,
      Selector,
      peg::nothing, // Action
      peg::tracer   // Control
    >(in);
  }
}

namespace ast::construct {
  void realize_tree (std::unique_ptr<node> & start, source_type type) {
    if (start->has_content()) start->realize(type);
    for (up_node & child : start->children)
      realize_tree(child, type);
  }
}

namespace ast::L3::construct {
  template <typename Rule>
  up_node from_string (std::string const & value) {
    peg::memory_input<> in(value, value);
    up_node root = parse<peg::must<Rule>>(in);
    ast::construct::realize_tree(root, source_type::ephemeral);
    return root;
  }
  using strings = std::vector<std::string>;
  template <typename Rule>
  up_node from_strings (strings const && strings) {
    std::stringstream concatenation;
    for (std::string const & string : strings)
      concatenation << string;
    return from_string<Rule>(concatenation.str());
  }
}

// REFACTOR(jordan): figure out how to combine with the above L3 version
namespace ast::L2::construct {
  template <typename Rule>
  up_node from_string (std::string const & value) {
    peg::memory_input<> in(value, value);
    up_node root = parse<peg::must<Rule>>(in);
    ast::construct::realize_tree(root, source_type::ephemeral);
    return root;
  }
  using strings = std::vector<std::string>;
  template <typename Rule>
  up_node from_strings (strings const && strings) {
    std::stringstream concatenation;
    for (std::string const & string : strings)
      concatenation << string;
    return from_string<Rule>(concatenation.str());
  }
}
