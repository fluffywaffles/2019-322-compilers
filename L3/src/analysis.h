#pragma once

#include "L2/analysis.h"

#include "grammar.h"
#include "helper.h"

namespace analysis::L3 {
  namespace view = helper::view;
  using node = helper::L3::node;
  using up_node  = helper::L3::up_node;
  using up_nodes = helper::L3::up_nodes;
  using variable = helper::L3::variable;
  using successor_map = std::map<node const *, view::set<node>>;
  using liveness_map  = std::map<node const *, view::set<variable>>;
}

namespace analysis::L3::successor {
  struct result {
    view::vec<node> const instructions;
    successor_map map;
  };

  void set (node const & n, node const & successor, result & result) {
    result.map[&n].insert(&successor);
  }

  void instruction (node const & n, int index, result & result) {
    namespace instruction = grammar::L3::instruction;
    using namespace instruction;

    view::vec<node> const & siblings = result.instructions;

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

  void compute (result & result) {
    for (int index = 0; index < result.instructions.size(); index++) {
      node const & node = *result.instructions.at(index);
      // NOTE(jordan): make sure every node has at least an empty set
      result.map[&node] = {};
      successor::instruction(node, index, result);
    }
    return;
  }

  result compute (view::vec<node> const & instructions) {
    result result = { instructions };
    successor::compute(result);
    return result;
  }
}

namespace analysis::L3::liveness {
  struct result {
    view::vec<node> instructions;
    std::set<variable> variables;
    successor::result successor;
    liveness_map in;
    liveness_map out;
    liveness_map gen;
    liveness_map kill;
  };
}

namespace analysis::L3::liveness::gen_kill {
  namespace {
    static bool DBG = true;
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
        return genkill(which, n, helper::L3::unwrap_assert(v), result);
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
      auto const & variables = result.variables;
      std::string const variable_string = v.content();
      // NOTE(jordan): produce error for gen/kill undefined variable
      // TODO(jordan): This makes sense as its own pre-analysis pass
      if (!helper::has(variable_string, variables)) {
        std::cerr
          << "ERR   gen/kill nonexistent variable"
          << " " << variable_string << "\n";
        assert(false && "gen/kill variable is not defined!");
      }
      auto const * variable = &*helper::find(variable_string, variables);
      switch (which) { // NOTE(jordan): fallthrough is a bitch.
        case GenKill::gen  : result.gen .at(&n).insert(variable); return;
        case GenKill::kill : result.kill.at(&n).insert(variable); return;
      }
    }
  }

  void gen  (node const & n, node const & variable_node, result & result) {
    return genkill(GenKill::gen, n, variable_node, result);
  }
  void kill (node const & n, node const & variable_node, result & result) {
    return genkill(GenKill::kill, n, variable_node, result);
  }

  void instruction (node const & n, result & result) {
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

  void compute (result & result) {
    for (node const * instruction_ptr : result.instructions) {
      // NOTE(jordan): make sure every node has at least empty gens/kills
      result.gen  [instruction_ptr] = {};
      result.kill [instruction_ptr] = {};
      node const & instruction = *instruction_ptr;
      gen_kill::instruction(instruction, result);
    }
    return;
  }
}

namespace analysis::L3::liveness {
  void in_out (result & result) {
    view::vec<node> const & instructions = result.instructions;
    bool fixed_state;
    do {
      fixed_state = true;
      for (int index = instructions.size() - 1; index >= 0; index--) {
        node const & instruction = *instructions.at(index);
        auto const & gen         = result.gen.at(&instruction);
        auto const & kill        = result.kill.at(&instruction);
        auto const & successors  = result.successor.map.at(&instruction);
        // copy in and out sets
        auto in  = result.in.at(&instruction);
        auto out = result.out.at(&instruction);
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

        auto const & in_original  = result.in.at(&instruction);
        auto const & out_original = result.out.at(&instruction);
        fixed_state = fixed_state
          && helper::set_equal(in_original, in)
          && helper::set_equal(out_original, out);

        result.in.at(&instruction) = in;
        result.out.at(&instruction) = out;
      }
    } while(!fixed_state);
  }

  void compute (result & result) {
    liveness::gen_kill::compute(result);
    // NOTE(jordan): make sure every instruction has at least empty in/out
    for (node const * instruction_ptr : result.instructions) {
      result.in[instruction_ptr] = {};
      result.out[instruction_ptr] = {};
    }
    liveness::in_out(result);
    return;
  }

  result compute (node const & function) {
    assert(function.is<grammar::L3::function::define>());
    auto const instructions = helper::L3::collect_instructions(function);
    // TODO(jordan): collecting variables is its own analysis
    // Gather up all nodes containing variables
    node const & parameters = *function.children.at(1);
    assert(parameters.is<grammar::L3::operand::list::parameters>());
    view::vec<node> nodes = instructions;
    for (up_node const & parameter : parameters.children)
      nodes.push_back(&*parameter);
    // Collect defined variables
    auto const variables = helper::L3::collect_variables(nodes);
    std::cout << "variables\n\t";
    for (auto const & variable : variables) {
      std:: cout << variable << " ";
    }
    std::cout << "\n";
    auto const successor = successor::compute(instructions);
    result result = { instructions, variables, successor };
    liveness::compute(result);
    return result;
  }

  void print (std::ostream & os, result const & result ) {
    std::cout << "variables\n\t";
    for (auto const & variable : result.variables) {
      std:: cout << variable << " ";
    }
    std::cout << "\n";
    std::cout << "successors\n";
    for (int i = 0; i < result.instructions.size(); i++) {
      node const * instruction_ptr = result.instructions.at(i);
      auto const & successors = result.successor.map.at(instruction_ptr);
      std::cout << "\t[" << i << "] ";
      for (node const * successor : successors) {
        auto const & it = helper::find(successor, result.instructions);
        assert(it != std::end(result.instructions));
        int successor_index = it - std::begin(result.instructions);
        std::cout << successor_index << " ";
      }
      std::cout << "\n";
    }
    std::cout << "\n";
    std::cout << "gen\n";
    for (int i = 0; i < result.instructions.size(); i++) {
      node const * instruction_ptr = result.instructions.at(i);
      view::set<variable> const & gens = result.gen.at(instruction_ptr);
      std::cout << "\t[" << i << "] ";
      for (variable const * gen : gens) std::cout << *gen << " ";
      std::cout << "\n";
    }
    std::cout << "\n";
    std::cout << "kill\n";
    for (int i = 0; i < result.instructions.size(); i++) {
      node const * instruction_ptr = result.instructions.at(i);
      view::set<variable> const & kills = result.kill.at(instruction_ptr);
      std::cout << "\t[" << i << "] ";
      for (variable const * kill : kills)
        std::cout << *kill << " ";
      std::cout << "\n";
    }
    std::cout << "\n";
    std::cout << "in\n";
    for (int i = 0; i < result.instructions.size(); i++) {
      node const * instruction_ptr = result.instructions.at(i);
      view::set<variable> const & ins = result.in.at(instruction_ptr);
      std::cout << "\t[" << i << "] ";
      for (variable const * in : ins)
        std::cout << *in << " ";
      std::cout << "\n";
    }
    std::cout << "\n";
    std::cout << "out\n";
    for (int i = 0; i < result.instructions.size(); i++) {
      node const * instruction_ptr = result.instructions.at(i);
      view::set<variable> const & outs = result.out.at(instruction_ptr);
      std::cout << "\t[" << i << "] ";
      for (variable const * out : outs)
        std::cout << *out << " ";
      std::cout << "\n";
    }
    std::cout << "\n";
    view::vec<node> instructions = result.instructions;
  }
}
