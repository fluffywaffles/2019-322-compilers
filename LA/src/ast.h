#pragma once

#include "tao/pegtl/contrib/parse_tree.hpp"
#include "tao/pegtl/contrib/tracer.hpp"

#include "IR/ast.h"
#include "grammar.h"

namespace ast::LA {
  namespace peg = tao::pegtl;
}

namespace ast::LA::filter {
  using namespace grammar::LA;
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
        /* NOTE(jordan): keep around the contents of types so that we can
         * tell what a variable is supposed to be.
         */
        literal::type::tuple_,
        literal::type::scalar::code_,
        literal::type::scalar::int64_,
        literal::type::multiarray::int64_,
        literal::type::function::void_,
        /* NOTE(jordan): keep around the [] at the end of multiarray types
         * so it's trivial to figure out dimensionality.
         */
        meta::literal::type::multiarray::suffix,
        meta::literal::type::multiarray::dimensions,
        /* NOTE(jordan): constructible object literals.
         */
        literal::object::array,
        literal::object::tuple,
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
        expression::shift,
        expression::arithmetic,
        expression::comparison,
        expression::call::any,
        expression::call::defined,
        expression::call::intrinsic,
        expression::make::any,
        expression::make::array,
        expression::make::tuple,
        expression::length,
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
        /* NOTE(jordan): we want to do later string matching against
         * operands, so we need to keep their contents around even
         * though it's redundant. It makes the code easier.
         */
        operand::label,
        operand::variable,
        operand::value,
        operand::typed,
        operand::index,
        operand::array::accessor,
        operand::array::accessors,
        operand::movable,
        /* NOTE(jordan): instructions are really just containers, but if
         * we keep around their content it's easier to transfer them
         * immediately to their equivalent L3 variants in cases where
         * translation is essentially a no-op.
         */
        instruction::assign::variable::gets_movable,
        instruction::assign::variable::gets_call,
        instruction::assign::variable::gets_new,
        instruction::assign::variable::gets_length,
        instruction::assign::variable::gets_index,
        instruction::assign::variable::gets_comparison,
        instruction::assign::variable::gets_arithmetic,
        instruction::assign::variable::gets_shift,
        instruction::assign::array::gets_movable,
        instruction::declare::variable,
        instruction::branch::if_else,
        instruction::branch::unconditional,
        instruction::define::label,
        instruction::ret::nothing,
        instruction::ret::value,
        instruction::call,
        /* NOTE(jordan): type wrapper nodes for identifying where/how a
         * type is being used - is it a function type, or a variable type?
         * Is it a scalar type, or a multiarray type, or a tuple type?
         */
        literal::type::variable::any,
        literal::type::function::any,
        literal::type::scalar::any,
        literal::type::multiarray::any
      >,
      peg::parse_tree::apply_remove_content::to<
        // instruction wrapper
        instruction::any,
        // Argument and variable lists are redundant with their contents
        operand::list::argument,
        operand::list::arguments,
        operand::list::parameter,
        operand::list::parameters,
        // Function containers are just containers
        function::instructions,
        function::define,
        // Same with program; just a list of functions
        program
      >
    >;
}

namespace ast::LA {
  template <
    typename Entry,
    template <class...> class Selector = filter::selector,
    typename Input
  > std::unique_ptr<node> parse (Input & in) {
    return peg::parse_tree::parse<Entry, node, Selector>(in);
  }
}

namespace ast::LA::debug {
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

namespace ast::LA::construct {
  template <typename Rule>
  std::unique_ptr<node> from_string (std::string const & value) {
    peg::memory_input<> in(value, value);
    std::unique_ptr<node> root = parse<peg::must<Rule>>(in);
    ast::construct::realize_tree(root, source_type::ephemeral);
    return root;
  }
  using strings = std::vector<std::string>;
  template <typename Rule>
  std::unique_ptr<node> from_strings (strings const && strings) {
    return from_string<Rule>(helper::string::from_strings(strings));
  }
}
