#pragma once

#include "L2/helper.h"
#include "ast.h"
#include "grammar.h"

namespace helper {
  namespace vector {
    template <typename Item>
    void append (std::vector<Item> & a, std::vector<Item> const & b) {
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

  int integer (node const & n) {
    return meta::integer<node, grammar::literal::number::integer::any>(n);
  }

  node & unwrap_assert (node const & parent) {
    return meta::unwrap_assert<node>(parent);
  }

  std::string strip_variable_prefix (std::string const & v) {
    return meta::match_substring<grammar::operand::variable, 1>(v);
  }

  /*
   * FIXME(jordan): cannot be refactored with helper::L2::definition_for
   * because of L2 instruction::any wrappers.
   *
   * Get rid of instruction::any wrappers and template on node pointer;
   * then can refactor into one helper.
   */
  node const & definition_for (
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
}
