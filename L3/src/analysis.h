#pragma once

#include "grammar.h"
#include "helper.h"

namespace analysis::L3 {
  namespace view = helper::view;
  namespace collection = helper::collection;
  using label    = std::string;
  using variable = std::string;
  using node     = helper::L3::node;
  using up_node  = helper::L3::up_node;
  using up_nodes = helper::L3::up_nodes;
}

namespace analysis::L3::walk {
  template <typename Statistic, typename Collection, typename Result>
  void compute (Collection const & nodes, Result & result) {
    for (typename Collection::value_type const & node : nodes) {
      bool walk_children = Statistic::compute(*node, result);
      if (walk_children) compute<Statistic>(node->children, result);
    }
  }
}

namespace analysis::L3::variables {
  struct result {
    std::set<variable> variables;
  };
  struct gather {
    static bool compute (node const &, result &);
  };
}

namespace analysis::L3::variables {
  bool gather::compute (node const & n, result & result) {
    if (n.is<grammar::L3::operand::variable>()) {
      result.variables.insert(n.content());
      return false; // NOTE(jordan): don't walk our children.
    } else {
      return true;
    }
  }
  result const summarize (view::vec<node> const & nodes) {
    result summary = {};
    walk::compute<variables::gather>(nodes, summary);
    return summary;
  }
  void print (result const & result) {
    std::cout << "variables\n\t";
    for (auto const & variable : result.variables) {
      std::cout << variable << " ";
    }
    std::cout << "\n";
  }
}

namespace analysis::L3::labels {
  struct result {
    std::set<label> labels;
    std::map<label const *, view::set<node>> definitions;
    std::map<label const *, view::set<node>> uses;
  };
  struct definitions {
    static bool compute (node const &, result &);
  };
  struct uses {
    static bool compute (node const &, result &);
  };
}

namespace analysis::L3::labels {
  bool definitions::compute (node const & n, result & result) {
    if (n.is<grammar::L3::instruction::define::label>()) {
      node const & label_node = helper::L3::unwrap_assert(n);
      std::string content = label_node.content();
      result.labels.insert(content);
      auto const * label = &*collection::find(content, result.labels);
      result.definitions[label].insert(&n);
      return false; // NOTE(jordan): don't bother with our children.
    } else {
      return true;
    }
  }
  bool uses::compute (node const & n, result & result) {
    if (n.is<grammar::L3::operand::label>()) {
      std::string content = n.content();
      // NOTE(jordan): we might use a label defined in an outer scope.
      if (!collection::has(content, result.labels))
        result.labels.insert(content);
      auto const * label = &*collection::find(content, result.labels);
      result.uses[label].insert(&n);
      return false; // NOTE(jordan): don't bother with our children.
    } else if (n.is<grammar::L3::instruction::define::label>()) {
      return false; // NOTE(jordan): skip over defined labels.
    } else {
      return true;
    }
  }
  result const summarize (view::vec<node> const & nodes) {
    result summary = {};
    walk::compute<labels::definitions>(nodes, summary);
    walk::compute<labels::uses>(nodes, summary);
    return summary;
  }
  void print (result const & result) {
    std::cout << "labels\n\t";
    for (auto const & label : result.labels) {
      std::cout << label << " ";
    }
    std::cout << "\n";
    std::cout << "label definitions\n";
    for (auto const & def_entry : result.definitions) {
      auto const & defs = def_entry.second;
      assert(defs.size() == 1 && "function: label defined >1 time!");
      node const * def  = *defs.begin();
      node const & defined_label = helper::L3::unwrap_assert(*def);
      std::cout
        << "\t" << *def_entry.first
        << " (as " << defined_label.content() << ") defined at"
        << " " << (*defs.begin())->begin()
        << "\n";
    }
    std::cout << "\n";
    std::cout << "label uses\n";
    for (auto const & use : result.uses) {
      auto const & uses = use.second;
      std::cout
        << "\t" << *use.first << " used...";
      for (auto const & use_site : uses) {
        node const & label = *use_site;
        std::cout
          << "\n\t\tas " << label.content()
          << " at "
          << (use_site->realized
                ? use_site->original_begin()
                : use_site->begin());
      }
      std::cout << "\n";
    }
    std::cout << "\n";
  }
}

namespace analysis::L3::successor {
  struct result {
    std::map<node const *, view::set<node>> map;
  };
}

namespace analysis::L3::successor {
  void set (node const & n, node const & successor, result & result) {
    result.map[&n].insert(&successor);
  }

  void instruction (
    node const & n,
    int index,
    view::vec<node> const & siblings,
    result & result
  ) {
    namespace instruction = grammar::L3::instruction;
    using namespace instruction;

    if (false
      || n.is<instruction::assign::variable::gets_movable>()
      || n.is<instruction::assign::variable::gets_comparison>()
      || n.is<instruction::assign::variable::gets_load>()
      || n.is<instruction::assign::variable::gets_call>()
      || n.is<instruction::assign::variable::gets_arithmetic>()
      || n.is<instruction::assign::variable::gets_shift>()
      || n.is<instruction::assign::address::gets_movable>()
      || n.is<instruction::call>()
      || n.is<instruction::define::label>()
    ) {
      // Successor is next sibling
      assert(siblings.size() > index + 1);
      node const & next = *siblings.at(index + 1);
      return successor::set(n, next, result);
    }

    if (false
      || n.is<instruction::branch::variable>()
      || n.is<instruction::branch::unconditional>()
    ) {
      if (n.is<instruction::branch::variable>()) {
        // Successor is definition of branched-to label or falls through
        // br <var> <label>
        assert(siblings.size() > index + 1);
        node const & label = *n.children.at(1);
        node const & next  = *siblings.at(index + 1);
        node const & jump  = helper::L3::definition_for(label, siblings);
        successor::set(n, next, result);
        successor::set(n, jump, result);
        return;
      }
      if (n.is<instruction::branch::unconditional>()) {
        // Successor is definition of branched-to label
        // br <label>
        node const & label = *n.children.at(0);
        node const & jump  = helper::L3::definition_for(label, siblings);
        return successor::set(n, jump, result);
      }
    }

    if (false
      || n.is<instruction::ret::nothing>()
      || n.is<instruction::ret::value>()
    ) {
      // A `return` has no successor; BUT an empty set should exist
      return;
    }

    std::cerr << "Something went wrong at\n\t" << n.name() << "\n";
    assert(false && "successor::instruction: unreachable!");
  }

  result compute (view::vec<node> const & instructions) {
    successor::result result = {};
    for (int index = 0; index < instructions.size(); index++) {
      node const & node = *instructions.at(index);
      // NOTE(jordan): make sure every node has at least an empty set
      result.map[&node] = {};
      successor::instruction(node, index, instructions, result);
    }
    return result;
  }

  void print (
    view::vec<node> const & instructions,
    successor::result const & result
  ) {
    std::cout << "\n";
    std::cout << "successors\n";
    for (int i = 0; i < instructions.size(); i++) {
      node const * instruction_ptr = instructions.at(i);
      auto const & successors = result.map.at(instruction_ptr);
      std::cout << "\t[" << i << "] ";
      for (node const * successor : successors) {
        auto const & it = collection::find(successor, instructions);
        assert(it != std::end(instructions));
        int successor_index = it - std::begin(instructions);
        std::cout << successor_index << " ";
      }
      std::cout << "\n";
    }
    std::cout << "\n";
  }
}

namespace analysis::L3::liveness::gen_kill {
  struct result {
    struct { variables::result const & variables_summary; } temporary;
    std::map<node const *, view::set<variable>> gen;
    std::map<node const *, view::set<variable>> kill;
  };
}

namespace analysis::L3::liveness::gen_kill {
  namespace generic {
    static bool DBG = false;
    enum class GenKill { gen, kill };
    void genkill (
      GenKill which,
      node const & n,
      node const & v,
      result & result
    ) {
      if (DBG) {
        std::cerr
          << "TRY  gen/kill " << v.name() << "\n";
      }
      if (v.is<grammar::L3::operand::list::argument>()) {
        if (DBG) {
          std::cerr
            << "UWRP gen/kill unwrapping a(n) " << v.name() << "\n";
        }
        node const & unwrapped = helper::L3::unwrap_assert(v);
        return genkill(which, n, unwrapped, result);
      }
      if (!helper::matches<grammar::L3::operand::variable>(v)) {
        if (DBG) {
          std::cerr
            << "FAIL gen/kill on a(n) " << v.name() << "\n";
        }
        return;
      }
      if (DBG) {
        std::cerr
          << "SXS  gen/kill " << v.name()
          << "\n     " << (which == GenKill::gen ? "GEN " : "KILL")
          << " "  << v.content() << "\n";
      }
      auto const & variables_summary = result.temporary.variables_summary;
      auto const & variables = variables_summary.variables;
      std::string const content = v.content();
      // NOTE(jordan): produce error for gen/kill undefined variable
      // TODO(jordan): This makes sense as its own pre-analysis pass
      if (!collection::has(content, variables)) {
        std::cerr
          << "ERR   gen/kill nonexistent variable"
          << " " << content << "\n";
        assert(false && "gen/kill variable is not defined!");
      }
      auto const * variable = &*collection::find(content, variables);
      switch (which) { // NOTE(jordan): fallthrough is a bitch.
        case GenKill::gen  : result.gen .at(&n).insert(variable); return;
        case GenKill::kill : result.kill.at(&n).insert(variable); return;
      }
    }
  }

  void gen  (node const & n, node const & v, result & result) {
    using namespace generic;
    return genkill(GenKill::gen, n, v, result);
  }
  void kill (node const & n, node const & v, result & result) {
    using namespace generic;
    return genkill(GenKill::kill, n, v, result);
  }

  void instruction (
    node const & n,
    result & result
  ) {
    namespace instruction = grammar::L3::instruction;
    using namespace instruction;

    if (n.is<instruction::assign::variable::gets_movable>()) {
      node const & variable = *n.children.at(0);
      /* node const & gets = *n.children.at(1); */
      node const & movable = *n.children.at(2);
      gen_kill::kill (n, variable, result);
      gen_kill::gen  (n, movable, result);
      return;
    }
    if (n.is<instruction::assign::variable::gets_comparison>()) {
      node const & variable = *n.children.at(0);
      /* node const & gets = *n.children.at(1); */
      node const & cmp = *n.children.at(2);
      node const & lhs = *cmp.children.at(0);
      /* node const & op  = *cmp.children.at(1); */
      node const & rhs = *cmp.children.at(2);
      gen_kill::kill (n, variable, result);
      gen_kill::gen  (n, lhs, result);
      gen_kill::gen  (n, rhs, result);
      return;
    }
    if (n.is<instruction::assign::variable::gets_load>()) {
      node const & variable = *n.children.at(0);
      /* node const & gets = *n.children.at(1); */
      node const & load = *n.children.at(2);
      node const & loaded = helper::L3::unwrap_assert(load);
      gen_kill::kill (n, variable, result);
      gen_kill::gen  (n, loaded, result);
      return;
    }
    if (n.is<instruction::assign::variable::gets_call>()) {
      node const & variable = *n.children.at(0);
      /* node const & gets = *n.children.at(1); */
      node const & call   = helper::L3::unwrap_assert(*n.children.at(2));
      node const & called = *call.children.at(0);
      node const & args   = *call.children.at(1);
      gen_kill::kill (n, variable, result);
      gen_kill::gen  (n, called, result);
      for (up_node const & arg : args.children) {
        gen_kill::gen (n, *arg, result);
        gen_kill::kill(n, *arg, result);
      }
      return;
    }
    if (n.is<instruction::assign::variable::gets_arithmetic>()) {
      node const & variable = *n.children.at(0);
      /* node const & gets = *n.children.at(1); */
      node const & arithmetic = *n.children.at(2);
      node const & lhs = *arithmetic.children.at(0);
      /* node const & op  = *arithmetic.children.at(1); */
      node const & rhs = *arithmetic.children.at(2);
      gen_kill::kill (n, variable, result);
      gen_kill::gen  (n, lhs, result);
      gen_kill::gen  (n, rhs, result);
      return;
    }
    if (n.is<instruction::assign::variable::gets_shift>()) {
      node const & variable = *n.children.at(0);
      /* node const & gets = *n.children.at(1); */
      node const & shift = *n.children.at(2);
      node const & lhs = *shift.children.at(0);
      /* node const & op  = *shift.children.at(1); */
      node const & rhs = *shift.children.at(2);
      gen_kill::kill (n, variable, result);
      gen_kill::gen  (n, lhs, result);
      gen_kill::gen  (n, rhs, result);
      return;
    }
    if (n.is<instruction::assign::address::gets_movable>()) {
      node const & store  = *n.children.at(0);
      node const & stored = helper::L3::unwrap_assert(store);
      /* node const & gets = *n.children.at(1); */
      node const & movable = *n.children.at(2);
      gen_kill::gen (n, stored, result);
      gen_kill::gen (n, movable, result);
      return;
    }
    if (n.is<instruction::call>()) {
      // NOTE(jordan): the call instruction wraps a call expression
      node const & call   = helper::L3::unwrap_assert(n);
      node const & called = *call.children.at(0);
      node const & args   = *call.children.at(1);
      gen_kill::gen(n, called, result);
      for (up_node const & arg : args.children) {
        gen_kill::gen (n, *arg, result);
        gen_kill::kill(n, *arg, result);
      }
      return;
    }
    if (n.is<instruction::branch::variable>()) {
      node const & variable = *n.children.at(0);
      gen_kill::gen  (n, variable, result);
      return;
    }
    if (n.is<instruction::ret::value>()) {
      node const & value = helper::L3::unwrap_assert(n);
      gen_kill::gen  (n, value, result);
      return;
    }
    if (n.is<instruction::define::label>()) {
      // defining a label does not gen or kill any variables.
      return;
    }
    if (n.is<instruction::branch::unconditional>()) {
      // unconditionally branching does not gen or kill any variables.
      return;
    }
    if (n.is<instruction::ret::nothing>()) {
      // returning nothing does not gen or kill any variables.
      return;
    }

    std::cerr << "Something went wrong at\n\t" << n.name() << "\n";
    assert(false && "liveness::gen_kill::instruction: unreachable!");
  }

  result compute (
    view::vec<node> const & instructions,
    variables::result const & variables_summary
  ) {
    gen_kill::result result = {
      { variables_summary },
    };
    for (node const * instruction_ptr : instructions) {
      // NOTE(jordan): make sure every node has at least empty gens/kills
      result.gen  [instruction_ptr] = {};
      result.kill [instruction_ptr] = {};
      node const & instruction = *instruction_ptr;
      gen_kill::instruction(instruction, result);
    }
    return result;
  }

  void print (
    view::vec<node> const & instructions,
    result const & result
  ) {
    std::cout << "gen\n";
    for (int i = 0; i < instructions.size(); i++) {
      node const * instruction_ptr = instructions.at(i);
      auto const & gens = result.gen.at(instruction_ptr);
      std::cout << "\t[" << i << "] ";
      for (variable const * gen : gens) std::cout << *gen << " ";
      std::cout << "\n";
    }
    std::cout << "\n";
    std::cout << "kill\n";
    for (int i = 0; i < instructions.size(); i++) {
      node const * instruction_ptr = instructions.at(i);
      auto const & kills = result.kill.at(instruction_ptr);
      std::cout << "\t[" << i << "] ";
      for (variable const * kill : kills)
        std::cout << *kill << " ";
      std::cout << "\n";
    }
    std::cout << "\n";
  }
}

namespace analysis::L3::liveness {
  struct result {
    struct { successor::result const & successor; } temporary;
    gen_kill::result  const gen_kill;
    std::map<node const *, view::set<variable>> in;
    std::map<node const *, view::set<variable>> out;
  };
}

namespace analysis::L3::liveness {
  void in_out (view::vec<node> const & instructions, result & result) {
    auto const & gen_kill  = result.gen_kill;
    auto const & successor = result.temporary.successor;
    bool fixed_state;
    do {
      fixed_state = true;
      for (node const * p_instruction : instructions) {
        auto const & gen        = gen_kill.gen.at(p_instruction);
        auto const & kill       = gen_kill.kill.at(p_instruction);
        auto const & successors = successor.map.at(p_instruction);
        // copy in and out sets
        auto in  = result.in.at(p_instruction);
        auto out = result.out.at(p_instruction);
        // out_minus_kill = OUT[i] - KILL[i]
        view::set<variable> out_minus_kill = {};
        helper::set_difference(out, kill, out_minus_kill);
        // IN[i] = GEN[i] U (out_minus_kill)
        helper::set_union(gen, out_minus_kill, in);
        // OUT[i] = U(s : successor of(i)) IN[s]
        for (auto const & successor : successors) {
          auto const & in_s = result.in.at(successor);
          helper::set_union(in_s, out, out);
        }

        auto const & in_original  = result.in.at(p_instruction);
        auto const & out_original = result.out.at(p_instruction);
        fixed_state = fixed_state
          && helper::set_equal(in_original, in)
          && helper::set_equal(out_original, out);

        result.in.at(p_instruction) = in;
        result.out.at(p_instruction) = out;
      }
    } while(!fixed_state);
  }

  result compute (
    view::vec<node> const & instructions,
    variables::result const & variables_summary,
    successor::result const & successor
  ) {
    liveness::result result = {
      { successor },
      gen_kill::compute(instructions, variables_summary),
    };
    // NOTE(jordan): make sure every instruction has at least empty in/out
    for (node const * instruction_ptr : instructions) {
      result.in  [instruction_ptr] = {};
      result.out [instruction_ptr] = {};
    }
    liveness::in_out(instructions, result);
    return result;
  }

  void print (
    std::ostream & os,
    view::vec<node> const & instructions,
    result const & result
  ) {
    std::cout << "in\n";
    for (int i = 0; i < instructions.size(); i++) {
      node const * instruction_ptr = instructions.at(i);
      view::set<variable> const & ins = result.in.at(instruction_ptr);
      std::cout << "\t[" << i << "] ";
      for (variable const * in : ins)
        std::cout << *in << " ";
      std::cout << "\n";
    }
    std::cout << "\n";
    std::cout << "out\n";
    for (int i = 0; i < instructions.size(); i++) {
      node const * instruction_ptr = instructions.at(i);
      view::set<variable> const & outs = result.out.at(instruction_ptr);
      std::cout << "\t[" << i << "] ";
      for (variable const * out : outs)
        std::cout << *out << " ";
      std::cout << "\n";
    }
    std::cout << "\n";
  }
}

namespace analysis::L3::function {
  struct result {
    liveness::result  const liveness;
    labels::result    const labels_summary;
    successor::result const successors;
    view::vec<node>   const parameters;
    view::vec<node>   const instructions;
    variables::result const variables_summary;
    std::string       const name;
  };
  namespace instructions {
    struct gather {
      static bool compute (node const &, view::vec<node> &);
    };
  }
}

namespace analysis::L3::function {
  bool instructions::gather::compute (
    node const & n,
    view::vec<node> & nodes
  ) {
    if (false // NOTE(jordan): get instructions from all contexts.
      || n.is<grammar::L3::context::free>()
      || n.is<grammar::L3::context::empty>()
      || n.is<grammar::L3::context::started>()
      || n.is<grammar::L3::context::terminated>()
      || n.is<grammar::L3::context::complete>()
    ) {
      for (up_node const & child : n.children)
        nodes.push_back(&*child);
      return false; // NOTE(jordan): don't bother with our children.
    } else {
      return true;  // NOTE(jordan): look for contexts in our children.
    }
  }
  view::vec<node> collect_instructions (node const & function) {
    assert(function.is<grammar::L3::function::define>());
    // [0] label    [1] parameters   [2] contexts
    node const & contexts = *function.children.at(2);
    view::vec<node> instructions_view = {};
    walk::compute<instructions::gather>(
      contexts.children,
      instructions_view
    );
    return instructions_view;
  }
  view::vec<node> collect_parameters (node const & function) {
    node const & parameters = *function.children.at(1);
    view::vec<node> nodes = {};
    for (up_node const & p : parameters.children) nodes.push_back(&*p);
    return nodes;
  }
  result const summarize (node const & function) {
    auto instructions = function::collect_instructions(function);
    auto parameters   = function::collect_parameters(function);
    auto successors   = successor::compute(instructions);
    auto labels       = labels::summarize(instructions);
    auto variables    = variables::summarize(
      collection::concat(instructions, parameters)
    );
    auto liveness = liveness::compute(instructions, variables, successors);
    node const & entry = *function.children.at(0);
    node const & name  = helper::L3::unwrap_assert(entry);
    // NOTE(jordan): *move* everything; otherwise copies & corruption.
    return {
      std::move(liveness),
      // NOTE(jordan): DON'T DO THIS. Moving an rvalue copies its content!
      /* std::move(labels::summarize(instructions)), */
      std::move(labels),
      std::move(successors),
      std::move(parameters),
      std::move(instructions),
      std::move(variables),
      name.content(),
    };
  }
}
