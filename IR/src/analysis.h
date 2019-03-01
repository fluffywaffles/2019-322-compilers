#pragma once

#include "L3/analysis.h"

#include "grammar.h"
#include "ast.h"
#include "helper.h"

namespace analysis::IR {
  namespace view = helper::view;
  namespace collection = helper::collection;
  using node     = ast::node;
  using up_node  = ast::up_node;
  using variable = std::string;
}

namespace analysis::IR::variables {
  struct result {
    std::set<variable> variables;
    std::map<variable const *, node const *> declarations;
    std::map<variable const *, view::set<node>> definitions;
    std::map<variable const *, view::set<node>> uses;
  };
  namespace declarations {
    struct gather { static bool compute (node const &, result &); };
    namespace helper { void handle (node const &, result &); }
  }
  namespace definitions {
    struct gather { static bool compute (node const &, result &); };
    namespace helper { void handle (node const &, result &); }
  }
  namespace uses {
    struct gather { static bool compute (node const &, result &); };
  }
}

namespace analysis::IR::variables {
  void declarations::helper::handle (node const & n, result & result) {
    node const & type_node     = *n.children.at(0);
    node const & variable_node = *n.children.at(1);
    variable v = variable_node.content();
    auto const insertion = result.variables.insert(v);
    bool const inserted  = insertion.second;
    variable const * variable = &*insertion.first;
    if (!inserted) { // debug
      std::cerr << "multiple declarations of variable " << v;
      if (collection::has(variable, result.declarations)) {
        std::cerr
          << "\n\t" << result.declarations.at(variable)->begin() << "\n";
      } else {
        std::cerr
          << "\n\tunknown original declaration site (internal bug?)\n";
      }
      assert(false && "multiple declarations of the same variable");
    }
    result.declarations[variable] = &n;
    // Initialize definitions and uses so that we can use set::at()
    result.definitions[variable] = {};
    result.uses[variable] = {};
  }
  bool declarations::gather::compute (node const & n, result & result) {
    if (n.is<grammar::IR::instruction::declare::variable>()) {
      helper::handle(n, result);
      return false;
    } else if (n.is<grammar::IR::operand::list::parameter>()) {
      node const & typed = *n.children.at(0);
      helper::handle(typed, result);
      return false;
    } else {
      return true;
    }
  }
  void definitions::helper::handle (node const & n, result & result) {
    auto const content = n.content();
    // FIXME(jordan): inserted to allow bad tests.
    if (!collection::has(content, result.variables)) {
      std::cerr
        << "\nWARN: definition of undeclared variable"
        << " " << content << "\n";
      auto const insertion = result.variables.insert(content);
      variable const * variable = &*insertion.first;
      result.uses[variable] = {};
      result.definitions[variable] = {};
      result.definitions.at(variable).insert(&n);
      return;
    }
    auto const * variable = &*collection::find(content, result.variables);
    result.definitions.at(variable).insert(&n);
  }
  bool definitions::gather::compute (node const & n, result & result) {
    using namespace grammar::IR;
    if (false
      || n.is<instruction::assign::variable::gets_arithmetic>()
      || n.is<instruction::assign::variable::gets_comparison>()
      || n.is<instruction::assign::variable::gets_call>()
      || n.is<instruction::assign::variable::gets_shift>()
      || n.is<instruction::assign::variable::gets_index>()
      || n.is<instruction::assign::variable::gets_movable>()
      || n.is<instruction::assign::variable::gets_length>()
      || n.is<instruction::assign::variable::gets_new>()
    ) {
      node const & variable_node = *n.children.at(0);
      helper::handle(variable_node, result);
      return false;
    } else if (false
      || n.is<instruction::assign::array::gets_movable>()
    ) {
      node const & index_node    = *n.children.at(0);
      node const & variable_node = *index_node.children.at(0);
      helper::handle(variable_node, result);
      return false;
    } else {
      return true;
    }
  }
  bool uses::gather::compute (node const & n, result & result) {
    if (n.is<grammar::IR::operand::variable>()) {
      if (!collection::has(n.content(), result.variables)) { // debug
        std::cerr << "use of undeclared variable " << n.content() << "\n";
        assert(false && "use of undeclared variable");
      }
      auto const * v = &*collection::find(n.content(), result.variables);
      result.uses.at(v).insert(&n);
      return false;
    } else {
      return true;
    }
  }
  void print (variables::result const & result) {
    for (variable const & v : result.variables) {
      std::cerr << "variable " << v << " (" << &v << ")";
      if (!collection::has(&v, result.declarations)) {
        std::cerr << "\n\tWARN: variable is never declared.";
      } else {
        std::cerr
          << "\n\tdeclared"
          << "\n\t\t" << result.declarations.at(&v)->begin();
      }
      std::cerr
        << "\n\tdefined";
      for (auto const & def : result.definitions.at(&v))
        std::cerr << "\n\t\t" << def->begin();
      std::cerr
        << "\n\tused";
      for (auto const & use : result.uses.at(&v))
        std::cerr << "\n\t\t" << use->begin();
      std::cerr << "\n";
    }
  }
  result const summarize (view::vec<node> const & nodes) {
    result summary = {};
    ast::walk< variables::declarations::gather >(nodes, summary);
    ast::walk< variables::definitions::gather  >(nodes, summary);
    ast::walk< variables::uses::gather         >(nodes, summary);
    return summary;
  }
}

namespace analysis::IR::function {
  namespace instructions {
    using result = view::vec<node>;
    struct gather { static bool compute (node const &, result &); };
  }
}

namespace analysis::IR::function {
  bool instructions::gather::compute (
    node const & n,
    instructions::result & result
  ) {
    if (n.is<grammar::IR::function::basic_block>()) {
      for (up_node const & child : n.children)
        result.push_back(&*child);
      return false;
    } else {
      return true;
    }
  }
  instructions::result gather_instructions (node const & function) {
    instructions::result result = {};
    // [0] type::function [1] label [2] parameters [3] basic_blocks
    node const & basic_blocks = *function.children.at(3);
    ast::walk< instructions::gather >(basic_blocks.children, result);
    return result;
  }
}
