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
  struct declaration_result {
    node const & instruction;
    node const & typed_operand;
  };
  using up_declaration = std::unique_ptr<declaration_result>;
  struct result {
    std::set<variable> variables;
    std::map<variable const *, up_declaration> declaration;
    std::map<variable const *, view::set<node>> definitions;
    std::map<variable const *, view::set<node>> uses;
  };
  // AST walk computations
  namespace declaration {
    struct gather { static bool compute (node const &, result &); };
    namespace helper {
      void handle (node const &, node const &, result &);
    }
  }
  namespace definitions {
    namespace helper {
      void handle (node const &, node const &, result &);
    }
    struct gather { static bool compute (node const &, result &); };
  }
  namespace uses {
    struct gather { static bool compute (node const &, result &); };
  }
}

namespace analysis::IR::variables {
  void declaration::helper::handle (
    node const & n,
    node const & typed_operand,
    result & result
  ) {
    node const & type_node     = *typed_operand.children.at(0);
    node const & variable_node = *typed_operand.children.at(1);
    variable v = variable_node.content();
    auto const insertion = result.variables.insert(v);
    bool const inserted  = insertion.second;
    variable const * variable = &*insertion.first;
    if (!inserted) { // debug
      std::cerr << "multiple declarations of variable " << v;
      if (collection::has(variable, result.declaration)) {
        std::cerr
          << "\n\t"
          << result.declaration.at(variable)->instruction.begin()
          << "\n";
      } else {
        std::cerr
          << "\n\tunknown original declaration site (internal bug?)\n";
      }
      assert(false && "multiple declarations of the same variable");
    }
    result.declaration.emplace(variable, up_declaration(
      new declaration_result({ n, typed_operand })
    ));
    // Initialize definitions and uses so that we can use set::at()
    result.definitions.emplace(variable, view::set<node>({}));
    result.uses.emplace(variable, view::set<node>({}));
  }
  bool declaration::gather::compute (node const & n, result & result) {
    if (n.is<grammar::IR::instruction::declare::variable>()) {
      helper::handle(n, n, result);
      return false;
    } else if (n.is<grammar::IR::operand::list::parameter>()) {
      node const & typed = *n.children.at(0);
      helper::handle(n, typed, result);
      return false;
    } else {
      return true;
    }
  }
  void definitions::helper::handle (
    node const & definition,
    node const & variable_node,
    result & result
  ) {
    auto const content = variable_node.content();
    // FIXME(jordan): inserted to allow bad tests.
    if (!collection::has(content, result.variables)) {
      std::cerr
        << "WARN: definition of undeclared variable"
        << " " << content << "\n";
      auto const insertion = result.variables.insert(content);
      variable const * variable = &*insertion.first;
      result.uses.emplace(variable, view::set<node>({}));
      result.definitions.emplace(variable, view::set<node>({}));
      result.definitions.at(variable).insert(&definition);
      return;
    }
    auto const * variable = &*collection::find(content, result.variables);
    result.definitions.at(variable).insert(&definition);
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
      helper::handle(n, variable_node, result);
      return false;
    } else if (false
      || n.is<instruction::assign::array::gets_movable>()
    ) {
      node const & index_node    = *n.children.at(0);
      node const & variable_node = *index_node.children.at(0);
      helper::handle(n, variable_node, result);
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
      if (!collection::has(&v, result.declaration)) {
        std::cerr << "\n\tWARN: variable is never declared.";
      } else {
        std::cerr
          << "\n\tdeclared"
          << "\n\t\t" << result.declaration.at(&v)->instruction.begin();
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
    ast::walk< variables::declaration::gather >(nodes, summary);
    ast::walk< variables::definitions::gather >(nodes, summary);
    ast::walk< variables::uses::gather        >(nodes, summary);
    return summary;
  }
}

namespace analysis::IR::variables::arrays {
  namespace array {
    struct result {
      view::vec<node> dimensions;
    };
  }
  struct result {
    struct { variables::result const & variables_summary; } temporary;
    std::map<node const *, array::result> array;
  };
}

namespace analysis::IR::variables::arrays {
  void for_definition (node const & definition, result & result) {
    // [0] variable     [1] gets        [2] make
    node const & any_make = *definition.children.at(2);
    node const & make     = helper::unwrap_assert(any_make);
    if (!make.is<grammar::IR::expression::make::array>()) {
      // NOTE(jordan): This is not a multiarray; it has no dimensions.
      return;
    }
    if (collection::has(&definition, result.array)) { // debug
      std::cerr
        << "ERROR: definition already has an array analysis!"
        << "\n\t@ " << definition.begin();
      assert(false && "definition corresponds to >1 array!");
    }
    auto & array_result
      = result.array[&definition]
      = {};
    node const & arguments = *make.children.at(1);
    for (up_node const & up_argument : arguments.children) {
      node const & argument = *up_argument;
      node const & value = helper::unwrap_assert(argument);
      array_result.dimensions.push_back(&value);
    }
  }
  void for_variable (variable const & v, result & result) {
    auto const & variables_summary = result.temporary.variables_summary;
    // FIXME(jordan): this is a hack.
    if (!collection::has(&v, variables_summary.declaration))
      return; // Skip undeclared variables, assuming they aren't arrays.
    auto const & declaration = variables_summary.declaration.at(&v);
    auto const & any_type = *declaration->typed_operand.children.at(0);
    auto const & type = helper::unwrap_assert(any_type);
    if (!type.is<grammar::IR::literal::type::multiarray::any>())
      return;
    auto const & definitions = variables_summary.definitions.at(&v);
    for (node const * p_definition : definitions) {
      node const & definition = *p_definition;
      if (!definition.is<grammar::IR::instruction::assign::variable::gets_new>())
        continue; // skip
      arrays::for_definition(definition, result);
    }
  }
  void print (arrays::result const & arrays_summary) {
    for (auto const & array_entry : arrays_summary.array) {
      auto const & definition    = array_entry.first;
      auto const & array_result  = array_entry.second;
      auto const & variable_node = *definition->children.at(0);
      std::cerr
        << variable_node.content() << " is an array of"
        << " " << std::to_string(array_result.dimensions.size())
        << " dimension(s), [";
      for (auto const & dim : array_result.dimensions) {
        std::cerr << " " << dim->content();
      }
      std::cerr << " ]; this array is defined at"
        << "\n\t" << definition->begin()
        << "\n";
    }
  }
  result const summarize (variables::result const & variables_summary) {
    result result = { { variables_summary } };
    for (variable const & v : variables_summary.variables)
      arrays::for_variable(v, result);
    return result;
  }
}
