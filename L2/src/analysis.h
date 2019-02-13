// vim: set foldmethod=marker:
#pragma once
#include <set>
#include <cassert>
#include <iostream>

#include "L1/codegen.h"
#include "grammar.h"
#include "ast.h"
#include "helper.h"

// NOTE(jordan): toggle at compile time whether to always connect OUT/KILL
// TODO(jordan): implement register coalescing and remove this.
#define HACK_ALWAYS_OUT_KILL true

namespace analysis::L2 {
  using node   = ast::L2::node;
  using up_nodes  = helper::L2::up_nodes;
  using string = std::string;
  using successor_map    = std::map<const node *, std::set<const node *>>;
  using liveness_map     = std::map<const node *, std::set<string>>;
  using interference_map = std::map<const string, std::set<string>>;
}

// successor analysis {{{
/**
 *
 * succ[cjump_when    ] = then_label && i + 1
 * succ[cjump_if_else ] = then_label && else_label
 * succ[call_u_N      ] = i + 1 // intraprocedural
 * succ[call_intrinsic] = i + 1 // intraprocedural
 * succ[<else>...     ] = i + i
 *
 */
namespace analysis::L2::successor {
  static const bool DBG = false;
  struct result {
    const node & instructions;
    successor_map map;
  };

  void set (const node & n, const node & s, result & result) {
    result.map[&n].insert(&s);
  }

  // instruction {{{
  void instruction (const node & n, int index, result & result) {
    namespace instruction = grammar::L2::instruction;
    using namespace instruction;

    const up_nodes & siblings = result.instructions.children;

    if (n.is<instruction::any>()) {
      assert(n.children.size() == 1);
      const node & actual_instruction = *n.children.at(0);
      return successor::instruction(actual_instruction, index, result);
    }

    if (false
      || n.is<assign::assignable::gets_movable>()
      || n.is<assign::assignable::gets_relative>()
      || n.is<assign::relative::gets_movable>()
      || n.is<update::assignable::arithmetic::comparable>()
      || n.is<update::assignable::shift::shift>()
      || n.is<update::assignable::shift::number>()
      || n.is<update::relative::arithmetic::add_comparable>()
      || n.is<update::relative::arithmetic::subtract_comparable>()
      || n.is<update::assignable::arithmetic::add_relative>()
      || n.is<update::assignable::arithmetic::subtract_relative>()
      || n.is<assign::assignable::gets_comparison>()
      || n.is<define::label>()
      || n.is<update::assignable::arithmetic::increment>()
      || n.is<update::assignable::arithmetic::decrement>()
      || n.is<assign::assignable::gets_address>()
      || n.is<assign::assignable::gets_stack_arg>()
    ) {
      // Our successor is the next instruction. Nice!
      assert(siblings.size() > index + 1);
      const node & next_wrapper = *siblings.at(index + 1);
      const node & next = *next_wrapper.children.at(0);
      return successor::set(n, next, result);
    }

    if (false
      || n.is<jump::go2>()
      || n.is<jump::cjump::when>()
      || n.is<jump::cjump::if_else>()
    ) {
      // Ugh. Jumps.
      if (n.is<jump::go2>()) {
        const node & label = *n.children.at(0);
        const node & instruction
          = helper::L2::definition_for(label, siblings);
        return successor::set(n, instruction, result);
      }
      if (n.is<jump::cjump::when>()) {
        /* const node & cmp        = *n.children.at(0); */
        const node & then_label = *n.children.at(1);
        const node & then_instruction
          = helper::L2::definition_for(then_label, siblings);
        assert(siblings.size() > index + 1);
        const node & next_wrapper = *siblings.at(index + 1);
        const node & next = *next_wrapper.children.at(0);
        successor::set(n, next, result);
        successor::set(n, then_instruction, result);
        return;
      }
      if (n.is<jump::cjump::if_else>()) {
        /* const node & cmp        = *n.children.at(0); */
        const node & then_label = *n.children.at(1);
        const node & then_instruction
          = helper::L2::definition_for(then_label, siblings);
        const node & else_label = *n.children.at(2);
        const node & else_instruction
          = helper::L2::definition_for(else_label, siblings);
        successor::set(n, then_instruction, result);
        successor::set(n, else_instruction, result);
        return;
      }
      assert(false && "successor: jump::*: unreachable!");
    }

    if (false
      || n.is<invoke::call::callable>()
      || n.is<invoke::call::intrinsic::print>()
      || n.is<invoke::call::intrinsic::allocate>()
      || n.is<invoke::call::intrinsic::array_error>()
    ) {
      // Calls! Our analysis is intraprocedural, so the successor is easy.
      assert(siblings.size() > index + 1);
      const node & next_wrapper = *siblings.at(index + 1);
      const node & next = *next_wrapper.children.at(0);
      return successor::set(n, next, result);
    }

    if (n.is<invoke::ret>()) {
      // Do nothing. A `return` has no successor in an our analysis.
      return;
    }

    std::cout << "something went wrong at\n\t" << n.name() << "\n";
    assert(false && "liveness::successor: unreachable!");
  }
  // }}}

  void instructions (const node & instructions, result & result) {
    for (int index = 0; index < instructions.children.size(); index++) {
      const node & instruction = *instructions.children.at(index);
      successor::instruction(instruction, index, result);
    }
    return;
  }

  // print {{{
  void print (result & result) {
    const up_nodes & instructions = result.instructions.children;
    for (int index = 0; index < instructions.size(); index++) {
      const node & wrapper = *instructions.at(index);
      const node & instruction = helper::L2::unwrap_assert(wrapper);
      std::cout << "succ[" << index << "] = ";
      for (auto successor : result.map[&instruction]) {
        std::cout << successor->name();
      }
      std::cout << "\n";
    }
    std::cout << "\n";
  }
  // }}}

  void compute (result & result) {
    successor::instructions(result.instructions, result);
    if (DBG) successor::print(result);
    return;
  }
}
// }}}

// liveness {{{
namespace analysis::L2::liveness {
  const int DBG_SUCC                       = 0b000001;
  const int DBG_GEN_KILL                   = 0b000010;
  const int DBG_IN_OUT                     = 0b000100;
  const int DBG_IN_OUT_LOOP_CND            = 0b001000;
  const int DBG_IN_OUT_LOOP_ORIG           = 0b010000;
  const int DBG_IN_OUT_LOOP_OUT_MINUS_KILL = 0b100000;

  struct result {
    const node & instructions;
    successor::result successor;
    liveness_map in;
    liveness_map out;
    liveness_map gen;
    liveness_map kill;
  };
}

/**
 *
 * GEN[i]  = { <read variables> }
 * KILL[i] = { <defined variables> }
 *
 */
/* TODO(jordan): this  unwrapping logic is reusable, just not the gen/kill
 * logic. Separate out those 2 things so that other code can benefit from
 * the smart wrappers.
 */
// gen/kill helpers {{{
namespace helper::L2::liveness::gen_kill {
  enum struct GenKill { gen, kill };
  struct variable {
    using result = analysis::L2::liveness::result;
    static const bool DBG = false;
    static void generic (
      GenKill which,
      const node & i,
      const node & v,
      result & result
    ) {
      if (DBG) {
        std::cout
          << "gen/kill: inst : " << i.name()
          << "\n"
          << "gen/kill: var  : " << v.name()
          << " (" << (v.has_content() ? v.content() : "") << ")"
          << "\n";
      }
      bool is_variable = v.is<grammar::operand::variable>();
      assert(
        is_variable || matches<grammar::register_set::any>(v)
        && "gen/kill: node is not register or variable!"
      );
      // NOTE(jordan): generalized handling of rsp being out of scope
      if (matches<grammar::register_set::unanalyzable>(v)) {
        if (DBG)
          std::cout
            << "gen/kill: ignoring unanalyzable register:"
            << " " << v.name() << "\n";
        return;
      }
      std::string content = v.content();
      if (DBG) std::cout << "gen/kill: of var: " << content << "\n";
      switch (which) {
        case GenKill::gen  : result.gen [&i].insert(content); return;
        case GenKill::kill : result.kill[&i].insert(content); return;
        default: assert(false && "gen/kill: unreachable!");
      }
    }
    template <typename Reg>
    static void generic (GenKill which, const node & i, result & result) {
      assert(!matches<Reg>("rsp") && "gen/kill: cannot gen rsp!");
      namespace register_helper = helper::L2::x86_64_register;
      const std::string & reg = register_helper::convert<Reg>::string;
      if (DBG) std::cout << "gen/kill<Register>: " << reg << "\n";
      switch (which) {
        case GenKill::gen  : result.gen [&i].insert(reg); return;
        case GenKill::kill : result.kill[&i].insert(reg); return;
        default: assert(false && "gen/kill<Register>: unreachable!");
      }
    }
    static void gen  (const node & i, const node & v, result & result) {
      return generic(GenKill::gen, i, v, result);
    }
    static void kill (const node & i, const node & v, result & result) {
      return generic(GenKill::kill, i, v, result);
    }
    template <typename Reg>
    static void gen  (const node & i, result & result) {
      return generic<Reg>(GenKill::gen, i, result);
    }
    template <typename Reg>
    static void kill (const node & i, result & result) {
      return generic<Reg>(GenKill::kill, i, result);
    }
  };
}

namespace helper::L2::liveness::gen_kill::operand {
  using result = analysis::L2::liveness::result;
  template<typename Operand>
  struct base {
    static const bool DBG = false;
    static bool accept (const node & v) {
      if (DBG)
        std::cout
          << "accept " << v.name() << "?"
          << " " << Operand::accept(v)
          << "\n";
      return Operand::accept(v);
    };
    static void generic (
      GenKill which,
      const node & n,
      const node & v,
      result & result
    ) {
      using variable = gen_kill::variable;
      const node & value = Operand::unwrap(v);
      if (accept(value)) {
        if (GenKill::gen  == which) variable::gen(n, value, result);
        if (GenKill::kill == which) variable::kill(n, value, result);
      }
    }
    static void gen  (const node & n, const node & v, result & result) {
      return generic(GenKill::gen, n, v, result);
    }
    static void kill (const node & n, const node & v, result & result) {
      return generic(GenKill::kill, n, v, result);
    }
  };
  /**
   * EXPLANATION(jordan): Templates are a strange, dark, dark magic.
   *
   * This pattern is called the CRTP - Curiously Recursive Template
   * Pattern. It's 'curiously recursive' because in the declaration:
   *
   *   struct x : tpl<x> {};
   *
   * the 2nd x is not an incomplete type; rather, it's concrete. But in:
   *
   *   template<> struct tpl<x> {};
   *
   * x is an incomplete type. The template is fully specialized; but,
   * the type is not complete. With CRTP, the type *is* complete, *and*
   * the template is fully specialized.
   *
   * Type completeness corresponds (afaict) to the definition of the
   * type being *concrete*, not meta (templated) or abstract or etc.
   *
   * This pattern lends itself to something called "type traits". In the
   * templated "base type," a prototype is provided but not implemented.
   * In our case, 'static bool accept (const node & v)'. Then, every
   * concrete instantiation (every CRTP instance) implements the method.
   *
   */
  template <typename Parent, typename Child>
  struct may_wrap {
    static const bool DBG = false;
    static const node & unwrap (const node & parent) {
      if (DBG)
        std::cout << "unwrap: parent type: " << parent.name() << "\n";
      // NOTE(jordan): whoah dependent type names? cool.
      assert(parent.is<typename Parent::rule>());
      const node & child = helper::L2::unwrap_assert(parent);
      if (DBG)
        std::cout << "unwrap: child  type: " << child.name() << "\n";
      if (child.is<typename Child::rule>())
        return Child::unwrap(child);
      else return child;
    }
  };
  template <>
  // NOTE(jordan): here, 'false_type' is taken to mean 'cannot wrap'
  struct may_wrap <std::false_type, std::false_type> {
    static const bool DBG = false;
    static const node & unwrap (const node & parent) {
      if (DBG) std::cout << "unwrap: " << parent.name() << "\n";
      return helper::L2::unwrap_assert(parent);
    }
  };
  using leaf_operand = may_wrap<std::false_type, std::false_type>;
  // NOTE(jordan): "Variable" operands
  struct memory : base<memory>, leaf_operand {
    using rule = grammar::operand::memory;
    static bool accept (const node & v) { return true; }
  };
  struct assignable : base<assignable>, leaf_operand {
    using rule = grammar::operand::assignable;
    static bool accept (const node & v) { return true; }
  };
  struct shift : base<shift>, leaf_operand {
    using rule = grammar::operand::shift;
    static bool accept (const node & v) {
      using namespace grammar::operand;
      return !matches<number>(v);
    }
  };
  // NOTE(jordan): "Variable-or-Value" operands
  struct movable : base<movable>, may_wrap<movable, memory> {
    using rule = grammar::operand::movable;
    static bool accept (const node & v) {
      using namespace grammar::operand;
      return !v.is<label>() && !matches<number>(v);
    }
  };
  struct comparable : base<comparable>, may_wrap<comparable, memory> {
    using rule = grammar::operand::comparable;
    static bool accept (const node & v) {
      using namespace grammar::operand;
      return !matches<number>(v);
    }
  };
  struct callable : base<callable>, may_wrap<callable, assignable> {
    using rule = grammar::operand::callable;
    static bool accept (const node & v) {
      using namespace grammar::operand;
      return !v.is<label>();
    }
  };
  struct relative : base<relative>, may_wrap<relative, memory> {
    using rule = grammar::expression::mem;
    static bool accept (const node & v) { return true; }
    static const node & unwrap (const node & v) {
      assert(v.children.size() == 2);
      const node & base = *v.children.at(0);
      assert(base.is<memory::rule>());
      return memory::unwrap(base);
    }
  };
}
// }}}

// gen/kill analysis {{{
namespace analysis::L2::liveness::gen_kill {
  // instruction {{{
  void instruction (const node & n, liveness::result & result) {
    namespace helper = helper::L2::liveness::gen_kill;
    namespace instruction = grammar::L2::instruction;

    if (n.is<instruction::any>()) {
      assert(n.children.size() == 1);
      const node & actual_instruction = *n.children.at(0);
      return gen_kill::instruction(actual_instruction, result);
    }

    // macros {{{
    // TODO(jordan): These would be better as templates and/or functions.
    // binary_kill_gen
    #define binary_kill_gen(N, DEST, SRC)                                \
      assert(N.children.size() >= 3);                                    \
      const node & dest = *N.children.at(0);                             \
      /* const node & op   = *N.children.at(1); */                       \
      const node & src  = *N.children.at(2);                             \
      helper::operand::SRC::gen(N, src, result);                         \
      helper::operand::DEST::kill(N, dest, result)

    #define binary_kill_nop(N, DEST)                                     \
      assert(N.children.size() >= 3);                                    \
      const node & dest = *N.children.at(0);                             \
      /* const node & op   = *N.children.at(1); */                       \
      /* const node & src  = *N.children.at(2); */                       \
      helper::operand::DEST::kill(N, dest, result)

    #define binary_genkill_nop(N, DEST)                                  \
      assert(N.children.size() >= 3);                                    \
      const node & dest = *N.children.at(0);                             \
      /* const node & op   = *N.children.at(1); */                       \
      /* const node & src  = *N.children.at(2); */                       \
      helper::operand::DEST::gen(N, dest, result);                       \
      helper::operand::DEST::kill(N, dest, result)

    // binary_genkill_gen
    #define binary_genkill_gen(N, DEST, SRC)                             \
      assert(N.children.size() >= 3);                                    \
      const node & dest = *N.children.at(0);                             \
      /* const node & op   = *N.children.at(1); */                       \
      const node & src  = *N.children.at(2);                             \
      helper::operand::SRC::gen(N, src, result);                         \
      helper::operand::DEST::gen(N, dest, result);                       \
      helper::operand::DEST::kill(N, dest, result)

    // unary_genkill
    #define unary_genkill(N, DEST)                                       \
      assert(N.children.size() >= 2);                                    \
      const node & dest = *N.children.at(0);                             \
      /* const node & op   = *N.children.at(1); */                       \
      helper::operand::DEST::gen(N, dest, result);                       \
      helper::operand::DEST::kill(N, dest, result)

    // binary_gen_gen
    #define binary_gen_gen(N, DEST, SRC)                                 \
      assert(N.children.size() >= 3);                                    \
      const node & lhs = *N.children.at(0);                              \
      /* const node & op  = *N.children.at(1); */                        \
      const node & rhs = *N.children.at(2);                              \
      helper::operand::SRC::gen(n, rhs, result);                         \
      helper::operand::DEST::gen(n, lhs, result)
    // }}}

    using namespace grammar::L2::operand;
    using namespace grammar::L2::instruction;

    if (n.is<assign::assignable::gets_movable>()) {
      binary_kill_gen(n, assignable, movable);
      return;
    }

    if (n.is<assign::assignable::gets_relative>()) {
      binary_kill_gen(n, assignable, relative);
      return;
    }

    if (n.is<assign::relative::gets_movable>()) {
      binary_genkill_gen(n, relative, movable);
      return;
    }

    if (n.is<update::assignable::arithmetic::comparable>()) {
      binary_genkill_gen(n, assignable, comparable);
      return;
    }

    if (n.is<update::assignable::shift::shift>()) {
      binary_genkill_gen(n, assignable, shift);
      return;
    }

    if (n.is<update::assignable::shift::number>()) {
      binary_genkill_nop(n, assignable);
      return;
    }

    if (false
      || n.is<update::relative::arithmetic::add_comparable>()
      || n.is<update::relative::arithmetic::subtract_comparable>()
    ) {
      binary_genkill_gen(n, relative, comparable);
      return;
    }

    if (false
      || n.is<update::assignable::arithmetic::add_relative>()
      || n.is<update::assignable::arithmetic::subtract_relative>()
    ) {
      binary_genkill_gen(n, assignable, relative);
      return;
    }

    if (false
      || n.is<update::assignable::arithmetic::increment>()
      || n.is<update::assignable::arithmetic::decrement>()
    ) {
      unary_genkill(n, assignable);
      return;
    }

    if (false
      || n.is<jump::cjump::if_else>()
      || n.is<jump::cjump::when>()
    ) {
      const node & cmp  = *n.children.at(0);
      /* const node & then = *n.children.at(1); */
      /* const node & els  = *n.children.at(2); // for if_else */
      binary_gen_gen(cmp, comparable, comparable);
      return;
    }

    // NOTE(jordan): binary_kill_nop
    if (n.is<assign::assignable::gets_stack_arg>()) {
      binary_kill_nop(n, assignable);
      return;
    }

    // NOTE(jordan): binary_kill_nop and binary_gen_gen
    if (n.is<assign::assignable::gets_comparison>()) {
      /* const node & dest = *n.children.at(0); */
      /* const node & op  = *n.children.at(1); */
      const node & cmp  = *n.children.at(2);
      binary_kill_nop(n, assignable);
      binary_gen_gen(cmp, comparable, comparable);
      return;
    }

    if (n.is<assign::assignable::gets_address>()) {
      assert(n.children.size() == 5);
      const node & dest   = *n.children.at(0);
      /* const node & _op    = *n.children.at(1); // ignore '@' */
      const node & base   = *n.children.at(2);
      const node & offset = *n.children.at(3);
      /* const node & scale  = *n.children.at(4); */
      helper::operand::assignable::gen(n, base, result);
      helper::operand::assignable::gen(n, offset, result);
      helper::operand::assignable::kill(n, dest, result);
      return;
    }

    if (n.is<invoke::ret>()) {
      namespace calling_convention = grammar::L2::calling_convention;
      helper::variable::gen<calling_convention::call::out>(n, result);
      namespace callee = calling_convention::callee_save;
      helper::variable::gen<callee::r12>(n, result);
      helper::variable::gen<callee::r13>(n, result);
      helper::variable::gen<callee::r14>(n, result);
      helper::variable::gen<callee::r15>(n, result);
      helper::variable::gen<callee::rbp>(n, result);
      helper::variable::gen<callee::rbx>(n, result);
      return;
    }

    if (false
      || n.is<invoke::call::callable>()
      || n.is<invoke::call::intrinsic::print>()
      || n.is<invoke::call::intrinsic::allocate>()
      || n.is<invoke::call::intrinsic::array_error>()
    ) {
      namespace calling_convention = grammar::L2::calling_convention;
      namespace arg                = calling_convention::call::argument;
      assert(n.children.size() == 2);
      const node & integer  = *n.children.at(1);
      int args  = ::helper::L2::integer(integer);
      if (args > 0) {
        if (args >= 1) helper::variable::gen<arg::rdi>(n, result);
        if (args >= 2) helper::variable::gen<arg::rsi>(n, result);
        if (args >= 3) helper::variable::gen<arg::rdx>(n, result);
        if (args >= 4) helper::variable::gen<arg::rcx>(n, result);
        if (args >= 5) helper::variable::gen<arg::r8 >(n, result);
        if (args >= 6) helper::variable::gen<arg::r9 >(n, result);
      }
      helper::variable::kill<calling_convention::call::out>(n, result);
      namespace caller = calling_convention::caller_save;
      helper::variable::kill<caller::r8 >(n, result);
      helper::variable::kill<caller::r9 >(n, result);
      helper::variable::kill<caller::r10>(n, result);
      helper::variable::kill<caller::r11>(n, result);
      helper::variable::kill<caller::rax>(n, result);
      helper::variable::kill<caller::rcx>(n, result);
      helper::variable::kill<caller::rdi>(n, result);
      helper::variable::kill<caller::rdx>(n, result);
      helper::variable::kill<caller::rsi>(n, result);
      if (n.is<invoke::call::callable>()) {
        const node & callable_node = *n.children.at(0);
        helper::operand::callable::gen(n, callable_node, result);
      }
      return;
    }

    // NOTE(jordan): explicitly catch the no-op case! Helps prevent bugs.
    if (false
      || n.is<define::label>()
      || n.is<jump::go2>()
    ) {
      assert(n.children.size() == 1);
      /* const node & label = *n.children.at(0); */
      return;
    }

    #undef binary_kill_gen
    #undef unary_genkill
    #undef binary_genkill_gen
    #undef binary_gen_gen

    std::cout << "something went wrong at\n\t" << n.name() << "\n";
    assert(false && "liveness::instruction: unreachable!");
  }
  // }}}

  void instructions (const node & instructions, result & result) {
    for (const auto & wrapper : instructions.children)
      gen_kill::instruction(*wrapper, result);
    return;
  }

  void compute (result & result) {
    return gen_kill::instructions(result.instructions, result);
  }
}
// }}}

/**
 *
 * IN[i]  = GEN[i] U (OUT[i] - KILL[i])
 * OUT[i] = U(s : successor of(i)) IN[s]
 *
 */
// in/out calculation {{{
namespace analysis::L2::liveness {
  void in_out (result & result, unsigned debug = 0) {
    const up_nodes & instructions = result.instructions.children;
    // FIXME(jordan): This code is still a little gross.
    int iteration_limit = -1; // NOTE(jordan): used for debugging.
    bool fixed_state;
    do {
      fixed_state = true;
      for (int index = instructions.size() - 1; index >= 0; index--) {
        const node & wrapper = *instructions.at(index);
        const node & instruction = *wrapper.children.at(0);
        // load gen, kill sets and successors
        auto & gen        = result.gen [&instruction];
        auto & kill       = result.kill[&instruction];
        auto & successors = result.successor.map[&instruction];
        // copy in and out sets
        auto in  = result.in [&instruction];
        auto out = result.out[&instruction];
        // out_minus_kill = OUT[i] - KILL[i]
        std::set<std::string> out_minus_kill;
        helper::L2::set_difference(out, kill, out_minus_kill);
        // debug {{{
        if (debug & DBG_IN_OUT_LOOP_OUT_MINUS_KILL) {
          std::cout << "OUT[" << index << "] - KILL[" << index << "] = ";
          for (auto x : out_minus_kill) {
            std::cout << x << " ";
          }
          std::cout << "\n";
        } // }}}
        // IN[i] = GEN[i] U (out_minus_kill)
        helper::L2::set_union(gen, out_minus_kill, in);
        // OUT[i] = U(s : successor of(i)) IN[s]
        for (const node * successor : successors) {
          auto & in_s = result.in[successor];
          helper::L2::set_union(in_s, out, out);
        }

        const auto & in_original  = result.in [&instruction];
        const auto & out_original = result.out[&instruction];
        // debug {{{
        if (debug & DBG_IN_OUT_LOOP_ORIG) {
          std::cout << "in_original[" << index << "]  = ";
          for (auto i : in_original) std::cout << i << " ";
          std::cout << "\n";
          std::cout << "out_original[" << index << "] = ";
          for (auto o : out_original) std::cout << o << " ";
          std::cout << "\n";
          std::cout << "in[" << index << "]  = ";
          for (auto i : in) std::cout << i << " ";
          std::cout << "\n";
          std::cout << "out[" << index << "] = ";
          for (auto o : out) std::cout << o << " ";
          std::cout << "\n";
        }
        // }}}
        fixed_state = fixed_state
          && helper::L2::set_equal(in_original, in)
          && helper::L2::set_equal(out_original, out);

        result.in [&instruction] = in;
        result.out[&instruction] = out;
      }
      // debug {{{
      if (debug & DBG_IN_OUT_LOOP_CND) {
        std::cout << "fixed_state?    " << fixed_state << "\n";
        std::cout << "hit iter limit? " << ((iteration_limit - 1) > 0) << "\n";
        std::cout << "cond: " << (!fixed_state || ((iteration_limit - 1) > 0)) << "\n";
      } // }}}
    } while (!fixed_state || (--iteration_limit > 0));
  }
}
// }}}

// full liveness analysis {{{
namespace analysis::L2::liveness {
  void compute (liveness::result & result, unsigned debug = 0) {
    if (debug & DBG_SUCC) { // {{{
      analysis::L2::successor::print(result.successor);
    } // }}}
    // 1. Compute GEN, KILL
    analysis::L2::liveness::gen_kill::compute(result);
    if (debug & DBG_GEN_KILL) { // {{{
      const up_nodes & instructions = result.instructions.children;
      for (int index = 0; index < instructions.size(); index++) {
        const node & wrapper = *instructions.at(index);
        const node & instruction = helper::L2::unwrap_assert(wrapper);
        std::cout << "gen[" << index << "]  = ";
        for (auto g : result.gen[&instruction]) std::cout << g << " ";
        std::cout << "\n";
        std::cout << "kill[" << index << "] = ";
        for (auto r : result.kill[&instruction]) std::cout << r << " ";
        std::cout << "\n";
      }
      std::cout << "\n";
    } // }}}
    // 3. Iteratively compute IN/OUT sets
    in_out(result, debug);
    if (debug & DBG_IN_OUT) { // {{{
      std::cout << "\n";
      for (int index = 0; index < result.instructions.children.size(); index++) {
        const node & wrapper = *result.instructions.children.at(index);
        const node & instruction = *wrapper.children.at(0);
        std::cout << "in[" << index << "]  = ";
        for (auto i : result.in[&instruction]) std::cout << i << " ";
        std::cout << "\n";
        std::cout << "out[" << index << "] = ";
        for (auto o : result.out[&instruction]) std::cout << o << " ";
        std::cout << "\n";
      }
      std::cout << "\n";
    } // }}}
    return;
  }
}
// }}}

// wrapper api {{{
namespace analysis::L2::liveness {
  result function (const node & function) {
    assert(
      function.is<grammar::L2::function::define>()
      && "liveness: called on non-function!"
    );
    const node & instructions = *function.children.at(3);
    successor::result successor = { instructions };
    analysis::L2::successor::compute(successor);
    liveness::result result = { instructions, successor };
    liveness::compute(result);
    return result;
  }

  void print (
    std::ostream & os,
    const result result,
    bool pretty = false
  ) {
    const node & instructions = result.instructions;
    os << (pretty ? "((in\n" : "(\n(in");
    for (int index = 0; index < instructions.children.size(); index++) {
      const node & instruction_wrapper = *result.instructions.children.at(index);
      const node & instruction = *instruction_wrapper.children.at(0);
      auto in = result.in.at(&instruction);
      os << (pretty ? "  (" : "\n(");
      for (auto var : in)
        os << helper::L2::strip_variable_prefix(var) << " ";
      os << (pretty ? ")\n" : ")");
    }
    //backspace thru: " )\n"
    os << (pretty ? "\b\b\b))\n" : "\n)");
    os << (pretty ? "(out\n"     : "\n\n(out");
    for (int index = 0; index < instructions.children.size(); index++) {
      const node & instruction_wrapper = *result.instructions.children.at(index);
      const node & instruction = *instruction_wrapper.children.at(0);
      auto out = result.out.at(&instruction);
      os << (pretty ? "  (" : "\n(");
      for (auto var : out)
        os << helper::L2::strip_variable_prefix(var) << " ";
      os << (pretty ? ")\n" : ")");
    }
    //backspace thru: " )\n"
    os << (pretty ? "\b\b\b))\n" : "\n)\n\n)\n");
  }
}
// }}}
// }}}

// interference {{{
namespace analysis::L2::interference {
  struct result {
    const node & instructions;
    std::set<std::string> variables;
    liveness::result liveness;
    interference_map graph;
  };
}

// graph connection helpers {{{
namespace analysis::L2::interference::graph {
  // TODO(jordan): uh, not this.
  void biconnect (
    result & result,
    const std::string & a,
    const std::string & b
  ) {
    // NOTE(jordan): don't allow a variable to connect to itself.
    if (a != b) {
      result.graph[a].insert(b);
      result.graph[b].insert(a);
    }
  }
}

// FIXME(jordan): these helpers are gross.
namespace analysis::L2::interference::graph::x86_64_register {
  namespace register_set = grammar::L2::register_set;
  namespace register_helper = helper::L2::x86_64_register;
  using namespace grammar::L2::identifier::x86_64_register;
  void connect_to_all (result & result, const std::string & origin) {
    auto & interferes = result.graph[origin];
    for (const auto & reg : register_helper::analyzable_registers()) {
      biconnect(result, origin, reg);
    }
  }
  void connect_all (result & result) {
    for (const auto & reg : register_helper::analyzable_registers()) {
      connect_to_all(result, reg);
    }
  }
}
// }}}

// analysis (graph building) {{{
namespace analysis::L2::interference {
  void compute (interference::result & result) {
    namespace assign = grammar::L2::instruction::assign;
    namespace update = grammar::L2::instruction::update;
    // 0. Connect all registers to one another. Don't look closely.
    graph::x86_64_register::connect_all(result);
    // Iterate over all of the instructions, and...
    for (int index = 0; index < result.instructions.children.size(); index++) {
      const node & instruction_wrapper = *result.instructions.children.at(index);
      const node & instruction = *instruction_wrapper.children.at(0);
      const auto in   = result.liveness.in[&instruction];
      const auto out  = result.liveness.out[&instruction];
      const auto kill = result.liveness.kill[&instruction];
      // 1. Connect each pair of variables in the same IN set
      for (const auto & variable : in) {
        for (const auto & sibling : in) {
          graph::biconnect(result, variable, sibling);
        }
      }
      // 2. Connect each pair of variables in the same OUT set
      // 3. Connect everything in KILL with OUT; except x <- y (both vars)
      // Check if we should connect the kill set.
      bool connect_kill = true;
      if (instruction.is<assign::assignable::gets_movable>()) {
        assert(instruction.children.size() == 3);
        const node & src = *instruction.children.at(1);
        if (helper::L2::matches<grammar::L2::operand::memory>(src)) {
          // This is a variable 'gets' a variable or register.
          !HACK_ALWAYS_OUT_KILL && (connect_kill = false);
        }
      }
      // Make the connections.
      for (const auto & variable : out) {
        for (const auto & sibling : out) {
          graph::biconnect(result, variable, sibling);
        }
        if (connect_kill) {
          for (const auto & k : kill) {
            graph::biconnect(result, variable, k);
          }
        }
      }
      if (connect_kill) for (const auto & variable : kill) {
        for (const auto & liveout : out) {
          graph::biconnect(result, variable, liveout);
        }
      }
      // 4. Handle special arithmetic constraints
      // Check if we're in the specially-constrained shift-update case.
      if (instruction.is<update::assignable::shift::shift>()) {
        // <a> <sop> <b>
        // <b> cannot be any register except rcx.
        assert(instruction.children.size() == 3);
        // FIXME(jordan): whoops.
        const node & src = *instruction.children.at(2);
        using shift = ::helper::L2::liveness::gen_kill::operand::shift;
        const node & value = shift::unwrap(src);
        bool is_variable = value.is<grammar::L2::operand::variable>();
        const std::string & variable = value.content();
        graph::x86_64_register::connect_to_all(result, variable);
        // FIXME(jordan): this is kinda gross
        using rcx = grammar::L2::identifier::x86_64_register::rcx;
        namespace register_helper = helper::L2::x86_64_register;
        std::string rcx_string = register_helper::convert<rcx>::string;
        result.graph[variable].erase(rcx_string);
        result.graph[rcx_string].erase(variable);
      }
    }
  }
}
// }}}

// wrapper api {{{
namespace analysis::L2::interference {
  result function (const liveness::result & liveness) {
    auto variables = helper::L2::collect_variables(liveness.instructions.children);
    interference::result result = {
      liveness.instructions,
      variables,
      liveness
    };
    // NOTE(jordan): make sure every variable has SOME kind of entry.
    for (auto variable : result.variables) {
      result.graph[variable] = {};
    }
    interference::compute(result);
    return result;
  }

  void print (std::ostream & os, const interference::result result) {
    for (auto & entry : result.graph) {
      const std::string & origin = entry.first;
      auto & interferes  = entry.second;
      os << helper::L2::strip_variable_prefix(origin) << " ";
      for (const std::string & interfering : interferes) {
        os << helper::L2::strip_variable_prefix(interfering) << " ";
      }
      os << "\n";
    }
    return;
  }
}
// }}}
// }}}

// graph coloring {{{
namespace analysis::L2::color {
  /// color definitions {{{
  // NOTE(jordan): We need a lot of colors. Crayola to the rescue!
  enum struct Color {
    plum,
    grape,
    bruise,     // well, okay, this one isn't Crayola.
    cerise,
    orchid,
    purple,     // or this one. (Really. They're all "<adjective> purple")
    fuchsia,
    magenta,
    royalty,    // fine. I cheated. A little. 3 times.
    thistle,
    eggplant,
    lavender,
    mulberry,
    wisteria,
    jazzberry_jam,
    // NOTE(jordan): special color for "must be spilled"
    none,
  };
  namespace { // NOTE(jordan): anonymous namespace is file-private. Weird.
    #define color2s(N) case Color::N : return #N
    std::string color_to_string (Color color) {
      switch (color) {
        color2s(plum);     color2s(grape);    color2s(cerise);
        color2s(purple);   color2s(royalty);  color2s(bruise);
        color2s(orchid);   color2s(fuchsia);  color2s(magenta);
        color2s(thistle);  color2s(eggplant); color2s(lavender);
        color2s(mulberry); color2s(wisteria); color2s(jazzberry_jam);
        // NOTE(jordan): none ==> SPILL
        case Color::none: return "<SPILL>";
      }
      assert(false && "color_to_string: unrecognized color!");
    }
    #undef color2s
    std::set<Color> all_colors = {
      Color::plum,     Color::grape,    Color::bruise,    Color::cerise,
      Color::orchid,   Color::purple,   Color::fuchsia,   Color::magenta,
      Color::royalty,  Color::thistle,  Color::eggplant,  Color::lavender,
      Color::mulberry, Color::wisteria, Color::jazzberry_jam, // shhhh
      // NOTE(jordan): we don't include 'none' because it's special
    };
  }
  /// }}}
  namespace entry {
    // NOTE(jordan): These are just "marker types".
    using uncolored = interference_map::value_type;
    using colored = uncolored;
  }
  struct result {
    const node & instructions;
    std::set<std::string> variables;
    interference::result interference;
    std::vector<entry::uncolored> removed;
    std::map<const std::string, Color> mapping;
    std::map<Color, std::string> color_to_register;
  };
}

// graph manipulation helpers {{{
namespace analysis::L2::color::graph {
  // Assign a color to an uncolored entry.
  const entry::colored & color (
    const entry::uncolored & entry,
    const Color & color,
    result & result
  ) {
    assert(
      result.mapping.find(entry.first) == result.mapping.end()
      && "graph::color: variable is already colored!"
    );
    result.mapping[entry.first] = color;
    return entry;
  }
  // Remove an uncolored entry from the interference graph.
  const entry::uncolored remove (
    const std::string & var,
    result & result
  ) {
    assert(result.mapping.find(var) == result.mapping.end());
    auto entry = std::make_pair(
      var, std::move(result.interference.graph.at(var))
    );
    result.interference.graph.erase(var);
    for (auto & value : result.interference.graph)
      value.second.erase(var);
    return entry;
  }
  // Pick an unused color for a given uncolored entry.
  const Color choose_color (
    const entry::uncolored & entry,
    result & result
  ) {
    const auto & mapping = result.mapping;
    std::set<Color> available_colors = all_colors; // NOTE(jordan): copies
    for (const auto & interferes : entry.second) {
      if (mapping.find(interferes) != mapping.end()) {
        Color adjacent_color = mapping.at(interferes);
        available_colors.erase(adjacent_color);
      }
    }
    if (available_colors.size() != 0) {
      // NOTE(jordan): prefer caller-save colors
      for (auto color : available_colors) {
        namespace caller = grammar::L2::calling_convention::caller_save;
        auto color_register = result.color_to_register[color];
        if (helper::L2::matches<caller::any>(color_register))
          return color;
      }
      return *available_colors.begin();
    } else {
      return Color::none;
    }
  }
  // Reinsert a colored entry into the interference graph.
  void reinsert (const entry::colored & entry, result & result) {
    result.interference.graph[entry.first] = entry.second;
    for (auto & interfered_with : entry.second) {
      result.interference.graph[interfered_with].insert(entry.first);
    }
  }
}
// }}}

namespace analysis::L2::color {
  // REFACTOR(jordan): with the code in driver.h and transform.h
  result function (const interference::result & interference) {
    color::result result = {
      interference.instructions,
      interference.variables,
      interference,
      { /* 'removed' vector    */ },
      { /* 'mapping' to color  */ },
      { /* 'color_to_register' */ },
    };
    // 0. color all register nodes
    namespace register_helper = helper::L2::x86_64_register;
    for (const auto & reg : register_helper::analyzable_registers()) {
      entry::uncolored entry = std::make_pair(
        reg, result.interference.graph.at(reg)
      );
      Color color = graph::choose_color(entry, result);
      graph::color(entry, color, result); // => entry::colored
      result.color_to_register[color] = std::string(reg);
    }
    // 1. sort the variables so those with most edges pop last
    std::vector<std::string> variable_vector;
    for (auto variable : result.variables)
      variable_vector.push_back(variable);
    std::sort(
      variable_vector.begin(),
      variable_vector.end(),
      [&] (std::string left, std::string right) {
        return result.interference.graph[left].size()
             > result.interference.graph[right].size();
      }
    );
    // 2. remove all non-register nodes & add to stack (in some order)
    for (const auto & variable : variable_vector) {
      result.removed.push_back(graph::remove(variable, result));
    }
    // 3. pop the stack, color the node, and reinsert it in the graph
    while (result.removed.size() != 0) {
      const auto uncolored = result.removed.back();
      // NOTE(jordan): don't try to color impossible nodes
      Color color = graph::choose_color(uncolored, result);
      const auto & colored = graph::color(uncolored, color, result);
      graph::reinsert(colored, result);
      result.removed.pop_back();
    }
    return result;
  }

  void print (std::ostream & os, const result & result) { // {{{
    for (const auto & mapping : result.mapping) {
      os
        << helper::L2::strip_variable_prefix(mapping.first)
        << " = "
        << color_to_string(mapping.second)
        << "\n";
    }
  } // }}}

  // recommenders {{{
  const std::string recommend_spill (
    const result & result,
    const std::string & prefix
  ) {
    auto variable = std::find_if(
      result.variables.begin(),
      result.variables.end(),
      [&] (const std::string & variable) {
        return true
          // NOTE(jordan): uncomment this to debug infinite spill loops
          /* && variable.find(prefix) != 0 */
          && result.mapping.at(variable) == Color::none;
      }
    );
    assert(variable != result.variables.end());
    return * variable;
  }

  // NOTE(jordan): unused.
  const std::set<std::string> recommend_spills (const result & result) {
    std::set<std::string> recommended = result.variables; // NOTE: copies
    for (const auto & variable : recommended) {
      if (result.mapping.at(variable) != Color::none)
        recommended.erase(variable);
    }
    return recommended;
  }
  // }}}

  bool is_complete (const result & result) { // {{{
    return std::all_of(
      result.variables.begin(),
      result.variables.end(),
      [&] (const std::string & variable) {
        return result.mapping.at(variable) != Color::none;
      }
    );
  } // }}}
}
// }}}
