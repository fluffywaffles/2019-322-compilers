#pragma once

#include "ast.h"
#include "helper.h"

namespace analysis::LA {
  namespace view = helper::view;
  namespace collection = helper::collection;
  using node     = ast::node;
  using up_node  = ast::up_node;
  using up_nodes = ast::up_nodes;
  using name     = std::string;
}

namespace analysis::LA::functions {
  struct result {
    std::vector<name> names;
    std::map<node const *, name const *> function_name;
  };

  result const summarize (up_nodes const & functions) {
    result summary = {};
    // [0] type [1] name [2] parameters
    for (up_node const & up_function : functions) {
      node const & function = *up_function;
      assert(function.is<grammar::LA::function::define>());
      // node const & type = *function.children.at(0);
      node const & name_node = *function.children.at(1);
      assert(name_node.is<grammar::LA::operand::name>());
      assert(!collection::has(&function, summary.function_name));
      summary.names.push_back(name_node.content());
      name const * p_name
        = &*collection::find(name_node.content(), summary.names);
      summary.function_name.emplace(&function, p_name);
    }
    return std::move(summary);
  }
}
