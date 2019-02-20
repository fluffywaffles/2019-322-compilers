#include "ast.h"
#include "helper.h"
#include "grammar.h"
#include "analysis.h"

namespace transform::L3::globalize {
  using node = ast::node;
  void apply (
    std::string const & suffix,
    analysis::L3::labels::result const & labels
  ) {
    int monotonic_index = 0;
    // NOTE(jordan): Globalize the names of all definitions.
    for (auto & definition_entry : labels.definitions) {
      // NOTE(jordan): only handle singly-defined labels.
      assert(definition_entry.second.size() == 1);
      node const * definition = *definition_entry.second.begin();
      node & label_node = helper::L3::unwrap_assert(*definition);
      node & name_node  = helper::L3::unwrap_assert(label_node);
      std::string const name  = name_node.content();
      std::stringstream stream;
      stream << name << "_" << suffix << "_" << monotonic_index++;
      std::string const new_name = std::move(stream.str());
      if (!label_node.realized) label_node.realize();
      label_node.reset<grammar::L3::operand::label>(":" + new_name);
      if (!name_node.realized) name_node.realize();
      name_node.reset<grammar::L3::identifier::name>(new_name);
    }
    // NOTE(jordan): Copy the new definition name to each use-site.
    for (auto & use_entry : labels.uses) {
      analysis::L3::label const * label = use_entry.first;
      // NOTE(jordan): only handle labels that were defined in this scope.
      assert(helper::collection::has(label, labels.definitions));
      // NOTE(jordan): only handle singly-defined labels.
      assert(labels.definitions.at(label).size() == 1);
      node const * definition = *labels.definitions.at(label).begin();
      node const & def_label = helper::L3::unwrap_assert(*definition);
      node const & def_name  = helper::L3::unwrap_assert(def_label);
      for (node const * const_use : use_entry.second) {
        node & use = const_cast<node &>(*const_use);
        node & use_name = helper::L3::unwrap_assert(use);
        if (!use.realized) use.realize();
        use.reset<grammar::L3::operand::label>(def_label.content());
        if (!use_name.realized) use_name.realize();
        use_name.reset<grammar::L3::identifier::name>(def_name.content());
      }
    }
  }
}
