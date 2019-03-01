#pragma once

#include "L2/helper.h"
#include "ast.h"
#include "grammar.h"

namespace helper {
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
  namespace collection {
    template <typename Collection>
    void append (Collection & a, Collection const & b) {
      a.insert(std::end(a), std::begin(b), std::end(b));
    }
    template <typename Collection>
    auto concat (Collection const & a, Collection const & b) {
      Collection result = a;
      append(result, b);
      return result;
    }
    template <typename Collection>
    auto find (
      typename Collection::value_type const & item,
      Collection const & colln
    ) {
      return std::find(std::begin(colln), std::end(colln), item);
    }
    template <typename Collection>
    bool has (
      typename Collection::value_type const & item,
      Collection const & colln
    ) {
      return find(item, colln) != std::end(colln);
    }
    template <typename K, typename V>
    bool has (K const & key, std::map<K, V> const & map) {
      return map.find(key) != map.end();
    }
  }
  namespace string {
    // NOTE(jordan): concatenate several strings with a single allocation.
    std::string from_strings (std::vector<std::string> const & strings) {
      int length = 0;
      for (std::string const & string : strings)
        length += string.size();
      std::string result;
      result.reserve(length);
      for (std::string const & string : strings)
        result += string;
      return result;
    }
    std::string from_strings (std::vector<std::string> const && strings) {
      return from_strings(strings);
    }
  }
}

namespace helper::L3 {
  namespace grammar = grammar::L3;

  using node     = ast::node;
  using up_node  = ast::up_node;
  using up_nodes = ast::up_nodes;

  int integer (node const & n) {
    return meta::integer<node, grammar::literal::number::integer::any>(n);
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
    /* std::cout << "seeking " << label.content() << "\n"; */
    for (node const * instruction : instructions) {
      /* std::cout << instruction->name() << "\n"; */
      if (instruction->is<grammar::instruction::define::label>()) {
        up_node const & defined_label = instruction->children.at(0);
        /* std::cout << defined_label->name() << " " << defined_label->content() << "\n"; */
        if (defined_label->content() == label.content()) {
          return *instruction;
        }
      }
    }
    std::cerr << "Could not find " << label.content() << "\n";
    assert(false && "definition_for: could not find label");
  }
}
