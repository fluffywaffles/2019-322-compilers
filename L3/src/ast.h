#pragma once
#include <iostream>

#include "tao/pegtl/contrib/parse_tree.hpp"
#include "tao/pegtl/contrib/tracer.hpp"

#include "L2/ast.h"
#include "L2/helper.h"
#include "grammar.h"

namespace peg = tao::pegtl;

namespace ast::L3 {
  /** EXPLANATION(jordan): "Realized" nodes
   *
   * We need nodes to be able to take ownership of their own content,
   * allowing us to construct them at runtime from an ephemeral string
   * source. This is "realizing" a node: it becomes independently aware.
   *
   * A realized node can also be transformed, both in its type and in its
   * content. This makes it easier (and more efficient) to modify an AST
   * in-place, for example to spill a variable.
   *
   * We also must be able to reconstruct the original source match for a
   * node (if any). This way we can debug a node's transformation after
   * the fact, to make sure it came from a valid original match.
   *
   * peg::parse_tree::node is composed of only:
   *   std::string source         // where does the content come from?
   *   internal::iterator m_begin // start position in source
   *   internal::iterator m_end   // end position in source
   *   const std::type_info * id  // type_info of matched parse rule
   *
   * We add to that an "owned" realized_content; original iterators and
   * source data (copies of what was there before realization); and a
   * 'realized' flag.
   */
  struct node : peg::parse_tree::basic_node<node> {
    bool realized = false;
    std::string realized_content;
    // Copies of original source name, iterators
    std::string original_source;
    peg::internal::iterator original_m_begin;
    peg::internal::iterator original_m_end;
    /**
     * EXPLANATION(jordan): realizing a node will allow it to own its
     * content; however, its iterators will "lose their place" in the
     * original source input.
     *
     * This makes sense: we aren't pointing into a larger string anymore;
     * we're holding our own string.
     *
     * However: we do save our original source iterators in the
     * original_m_begin and original_m_end member fields.
     */
    void realize (std::string const & new_source = "realized") {
      assert(has_content());
      // Change our source to "realized" (instead of a file, string, etc.)
      original_source = source;
      source = new_source;
      // Copy current content into realized_content (take ownership)
      realized_content = content();
      // Save the source match iterators
      original_m_begin = m_begin;
      original_m_end   = m_end;
      // Update iterators
      reiterate();
      realized = true;
    }

    void reiterate () {
      // Reset match iterators
      m_begin.reset();
      m_end.reset();
      // Set match iterators to point into new, realized content
      m_begin.data = &realized_content[0];
      m_end.data   = (&realized_content[realized_content.size() - 1]) + 1;
    }

    /**
     * EXPLANATION(jordan): original_begin() and original_end() make it
     * possible to trace a realized node back to its original location &
     * data.
     *
     * original_match() will return a string copy of the originally
     * matched string. (Mostly useful for debugging.)
     */
    peg::position original_begin () const {
      assert(realized);
      return peg::position(original_m_begin, original_source);
    }
    peg::position original_end () const {
      assert(realized);
      return peg::position(original_m_end, original_source);
    }
    std::string original_match () const {
      assert(realized);
      return std::string(original_m_begin.data, original_m_end.data);
    }

    /**
     * EXPLANATION(jordan): once a node is realized, we can transform it
     * into a node matching a different rule and having different content.
     */
    template <typename CurrentRule, typename NewRule>
    void transform (std::string const & value) {
      assert(realized);
      assert(is<CurrentRule>());
      id = &typeid(NewRule);
      reset<NewRule>(value);
    }

    /**
     * EXPLANATION(jordan): once a node is realized, we can change its
     * content while ensuring the new content still matches the original
     * rule.
     */
    template <typename CurrentRule>
    void reset (std::string const & value) {
      assert(realized);
      assert(value.size() > 0);
      assert(is<CurrentRule>());
      assert(helper::L2::matches<CurrentRule>(value));
      realized_content = std::move(value);
      reiterate();
    }
  };
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
        operand::movable,
        operand::list::argument,
        operand::list::variable
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

namespace ast::L3 {
  template <typename Entry, typename Input>
  std::unique_ptr<node> parse (Input & in) {
    return peg::parse_tree::parse<Entry, node, filter::selector>(in);
  }
}

namespace ast::L3::construct {
  template <typename Rule>
  std::unique_ptr<node> free_node (std::string const & value) {
    peg::memory_input<> in(value, "<input over constructor string>");
    std::unique_ptr<node> root = parse<Rule>(in);
    std::unique_ptr<node> result = std::move(root->children.at(0));
    result->realize("<generated>");
    return result;
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
