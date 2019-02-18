#pragma once

#include "L2/helper.h"
#include "ast.h"
#include "grammar.h"

namespace helper {
  namespace vector {
    template <typename Item>
    std::vector<Item> append (std::vector<Item> a, std::vector<Item> b) {
      a.insert(std::end(a), std::begin(b), std::end(b));
    }
  }
  namespace view {
    /* NOTE(jordan): a view is a collection of constant non-null pointers.
     * Because we only ever derive views from collections of
     * unique_ptr, which we assume to be very long-lived, we can safely
     * assume that the non-reference view pointers will not be null. (Will
     * be at least as long-lived as the view.)
     *
     * The alternative would be to change things to `shared_ptr`s,
     * which... would be a good deal more work, and have higher overhead.
     * But it would be safer. So... worth it, maybe?
     */
    template <template <class...> class Container, typename Item>
      using base = Container<Item const *>;
    template <typename Item>
      using vec = base<std::vector, Item>;
    template <typename Item>
      using set = base<std::set, Item>;
  }
  template <typename Collection, typename Item>
  auto find (Item const & item, Collection const & collection) {
    return std::find(std::begin(collection), std::end(collection), item);
  }
  template <typename Collection, typename Item>
  bool has (Item const & value, Collection const & collection) {
    return find(value, collection) != std::end(collection);
  }
}

namespace helper::L3 {
  namespace grammar = grammar::L3;

  using node     = ast::L3::node;
  using up_node  = std::unique_ptr<node>;
  using up_nodes = std::vector<up_node>;
  using variable = std::string;

  int integer (node const & n) {
    return meta::integer<node, grammar::literal::number::integer::any>(n);
  }

  // NOTE(jordan): collects variables one level deep in tree
  std::set<variable> collect_variables (view::vec<node> const & nodes) {
    std::set<variable> variables = {};
    for (node const * node : nodes) {
      for (up_node const & child : node->children) {
        if (child->is<grammar::operand::variable>()) {
          variables.insert(child->content());
        }
      }
    }
    return variables;
  }

  node const & definition_for(
    node            const & label,
    view::vec<node> const & instructions
  ) {
    assert(label.is<grammar::operand::label>());
    std::cout << "seeking " << label.content() << "\n";
    for (node const * instruction : instructions) {
      std::cout << instruction->name() << "\n";
      if (instruction->is<grammar::instruction::define::label>()) {
        up_node const & defined_label = instruction->children.at(0);
        std::cout << defined_label->name() << " " << defined_label->content() << "\n";
        if (defined_label->content() == label.content()) {
          return *instruction;
        }
      }
    }
    std::cerr << "Could not find " << label.content() << "\n";
    assert(false && "definition_for: could not find label");
  }

  // NOTE(jordan): copy/paste from l2/helper.
  std::string strip_variable_prefix (std::string const & s) {
    if (matches<grammar::identifier::variable>(s)) {
      return std::string(s.begin() + 1, s.end());
    } else {
      return s;
    }
  }

  node const & unwrap_assert (node const & parent) {
    return meta::unwrap_assert<node>(parent);
  }

  view::vec<node> collect_instructions (node const & function) {
    assert(function.is<grammar::function::define>());
    // [0] label    [1] parameters   [2] contexts
    node const & contexts = *function.children.at(2);
    assert(contexts.is<grammar::function::contexts>());
    view::vec<node> instructions = {};
    for (up_node const & up_context : contexts.children) {
      for (up_node const & up_instruction : up_context->children) {
        std::cout << up_instruction->name() << "\n";
        instructions.push_back(&*up_instruction);
      }
    }
    return instructions;
  }
}
