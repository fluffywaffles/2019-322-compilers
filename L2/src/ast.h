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

namespace ast {
  template <typename Rule>
  bool matches (std::string const string) {
    // WAFKQUSKWLZAQWAAAA YES I AM THE T<EMPL>ATE RELEASER OF Z<ALGO>
    using namespace peg;
    memory_input<> string_input (string, "<call to matches>");
    return normal<Rule>::template
      match<apply_mode::NOTHING, rewind_mode::DONTCARE, nothing, normal>
      (string_input);
  }
  template <typename Rule, typename Node>
  bool matches (peg::parse_tree::basic_node<Node> const & n) {
    assert(n.has_content() && "matches: must have content!");
    const std::string & content = n.content();
    return matches<Rule>(content);
  }
}

namespace ast {
  enum class source_type {
      input,
      ephemeral,
      other_node,
  };
  std::string const source_type_to_string (source_type const & type) {
    switch (type) {
      case source_type::input      : return "<realized>";
      case source_type::ephemeral  : return "<ephemeral>";
      case source_type::other_node : return "<copied>";
    }
    std::cerr << "source_type (as int): " << (int)type << "\n";
    assert(false && "source_type_to_string: unrecognized source_type!");
  }
  struct node : peg::parse_tree::basic_node<node> {
    bool realized = false;
    std::string realized_content;
    // Copies of original source name, iterators
    std::string original_source;
    source_type original_source_type = source_type::input;
    peg::internal::iterator original_m_begin;
    peg::internal::iterator original_m_end;
    std::type_info const * original_id;
    /**
     * EXPLANATION(jordan): realizing a node will allow it to own its
     * content; however, its iterators will "lose their place" in the
     * original source input.
     *
     * This makes sense: we aren't pointing into a larger string anymore;
     * we're holding our own string.
     *
     * However: we do save our original source iterators in the
     * original_m_begin and original_m_end member fields; and, if the
     * original source was not ephemeral (i.e. it may still exist, and so
     * the iterators *MAY* still be valid) the original match can be
     * obtained via original_content().
     */
    void realize (source_type original_type = source_type::input) {
      assert(!realized && "node::realize: already realized!");
      assert(has_content());
      // Save original type id (in case we transform<>() later)
      original_id = id;
      // Save our old source and its type
      original_source = source;
      original_source_type = original_type;
      // Change source string based on original_type
      source = source_type_to_string(original_source_type);
      // Save the source match iterators
      original_m_begin = m_begin;
      original_m_end   = m_end;
      // Copy current content into realized_content (take ownership)
      realized_content = content();
      // Update iterators
      reiterate();
      realized = true;
    }

    std::unique_ptr<node> clone (bool recursive = true) const {
      auto up_clone = std::unique_ptr<node>(new node);
      node & clone  = *up_clone;
      clone.id      = id;
      clone.source  = source;
      clone.m_begin = m_begin;
      clone.m_end   = m_end;
      if (has_content()) clone.realize(source_type::other_node);
      if (recursive)
        for (std::unique_ptr<node> const & child : children)
          clone.children.push_back(std::move(child->clone()));
      return up_clone;
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
     * EXPLANATION(jordan): original_content() will return a string copy
     * of the originally matched string, if that string may still exist.
     * (Mostly useful for debugging.) If that string was ephemeral, it
     * instead returns "<gone>".
     *
     * original_name() will always return the name of the originally
     * constructed peg parse rule (i.e. node type).
     */
    std::string original_content () const {
      assert(realized);
      if (original_source_type == source_type::ephemeral)
        return original_source;
      return std::string(original_m_begin.data, original_m_end.data);
    }
    std::string original_name () const {
      assert(realized);
      return peg::internal::demangle(original_id->name());
    }
    peg::position original_begin () const {
      return peg::position(original_m_begin, original_source);
    }
    peg::position original_end () const {
      return peg::position(original_m_end, original_source);
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
      assert(matches<CurrentRule>(value));
      realized_content = value; // NOTE(jordan): copy new value
      reiterate();
    }
  };
  using up_node  = std::unique_ptr<node>;
  using up_nodes = std::vector<up_node>;
}

namespace ast::L2::filter {
  using namespace grammar::L2;
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

namespace ast::L2 {
  // NOTE(jordan): wrapped parse invocation using custom node
  template <
    typename Entry,
    template <class...> class Selector = filter::selector,
    typename Input
  > up_node parse (Input & in) {
    return peg::parse_tree::parse<Entry, node, Selector>(in);
  }

  template <typename Node>
  void print_node (
    peg::parse_tree::basic_node<Node> const & n,
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

}

namespace ast::L2::debug {
  template <
    typename Entry,
    template <typename...> class Selector = filter::selector,
    typename Input
  > auto trace (Input & in) {
    return peg::parse_tree::parse<
      Entry,
      Selector,
      peg::nothing,
      peg::tracer
    >(in);
  }
}
