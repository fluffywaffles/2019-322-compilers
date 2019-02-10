// vim: set foldmethod=marker:
#pragma once
#include <map>
#include <set>
#include <cassert>
#include <iostream>
#include <iterator>
#include <algorithm>

#include "L1/codegen.h"
#include "grammar.h"
#include "ast.h"
#include "helper.h"

namespace analysis::L2 {
  namespace ast     = ast::L2;
  namespace grammar = grammar::L2;
  using namespace ast;
  using namespace grammar;

  using nodes = helper::L2::nodes;
  using string = std::string;
  using liveness_map     = std::map<const node *, std::set<string>>;
  using successor_map    = std::map<const node *, std::set<const node *>>;
  using interference_map = std::map<const string, std::set<string>>;
}

// liveness {{{
namespace analysis::L2::liveness {
  // debug constants {{{
  const int DBG_SUCC                       = 0b000001;
  const int DBG_PRINT                      = 0b000010;
  const int DBG_GEN_KILL                   = 0b000100;
  const int DBG_IN_OUT_LOOP_CND            = 0b001000;
  const int DBG_IN_OUT_LOOP_ORIG           = 0b010000;
  const int DBG_IN_OUT_LOOP_OUT_MINUS_KILL = 0b100000;
  // }}}

  struct result {
    nodes instructions;
    liveness_map in;
    liveness_map out;
    liveness_map gen;
    liveness_map kill;
    successor_map successor;
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
namespace helper::L2::liveness::gen_kill {
  enum struct GenKill { gen, kill };
  struct variable {
    using result = analysis::L2::liveness::result;
    static const bool DBG = false;
    static void generic (
      GenKill choice,
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
      std::string name = is_variable
        ? helper::L2::variable::get_name(v)
        : v.content();
      if (DBG) std::cout << "gen/kill: of var: " << name << "\n";
      switch (choice) {
        case GenKill::gen  : result.gen [&i].insert(name); return;
        case GenKill::kill : result.kill[&i].insert(name); return;
        default: assert(false && "gen/kill: unreachable!");
      }
    }
    template <typename Reg>
    static void generic (GenKill choice, const node & i, result & result) {
      assert(!matches<Reg>("rsp") && "gen/kill: cannot gen rsp!");
      namespace register_helper = helper::L2::x86_64_register;
      const std::string & reg = register_helper::as_string<Reg>::value;
      if (DBG) std::cout << "gen/kill<Register>: " << reg << "\n";
      switch (choice) {
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
      GenKill choice,
      const node & n,
      const node & v,
      result & result
    ) {
      using variable = gen_kill::variable;
      const node & value = Operand::unwrap(v);
      if (accept(value)) {
        if (GenKill::gen  == choice) variable::gen(n, value, result);
        if (GenKill::kill == choice) variable::kill(n, value, result);
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

namespace analysis::L2::liveness::gen_kill { // {{{
  void instruction (const node & n, liveness::result & result) {
    namespace helper = helper::L2::liveness::gen_kill;
    using namespace grammar::instruction;

    if (n.is<instruction::any>()) {
      assert(n.children.size() == 1);
      const node & actual_instruction = *n.children.at(0);
      return gen_kill::instruction(actual_instruction, result);
    }

    // macros {{{
    // TODO(jordan): These would be better as templates and/or functions.
    #define assign_instruction_gen_kill(DEST, SRC)                       \
      assert(n.children.size() == 2);                                    \
      const node & dest = *n.children.at(0);                             \
      const node & src  = *n.children.at(1);                             \
      helper::operand::SRC::gen(n, src, result);                         \
      helper::operand::DEST::kill(n, dest, result)

    #define update_binary_instruction_gen_kill(DEST, SRC)                \
      assert(n.children.size() == 3);                                    \
      const node & dest = *n.children.at(0);                             \
      /* const node & op   = *n.children.at(1); */                       \
      const node & src  = *n.children.at(2);                             \
      helper::operand::SRC::gen(n, src, result);                         \
      helper::operand::DEST::gen(n, dest, result);                       \
      helper::operand::DEST::kill(n, dest, result)

    #define update_unary_instruction_gen_kill(DEST)                      \
      assert(n.children.size() == 2);                                    \
      const node & dest = *n.children.at(0);                             \
      /* const node & op   = *n.children.at(1); */                       \
      helper::operand::DEST::gen(n, dest, result);                       \
      helper::operand::DEST::kill(n, dest, result)

    #define cmp_gen_kill(CMP)                                            \
      assert(CMP.children.size() == 3);                                  \
      const node & lhs = *CMP.children.at(0);                            \
      /* const node & op  = *CMP.children.at(1); */                      \
      const node & rhs = *CMP.children.at(2);                            \
      helper::operand::comparable::gen(n, lhs, result);                  \
      helper::operand::comparable::gen(n, rhs, result)
    // }}}

    using namespace grammar::operand;
    if (n.is<assign::assignable::gets_movable>()) {
      assign_instruction_gen_kill(assignable, movable);
      return;
    }

    if (n.is<assign::assignable::gets_relative>()) {
      assign_instruction_gen_kill(assignable, relative);
      return;
    }

    if (n.is<assign::relative::gets_movable>()) {
      assign_instruction_gen_kill(relative, movable);
      return;
    }

    if (n.is<update::assignable::arithmetic::comparable>()) {
      update_binary_instruction_gen_kill(assignable, comparable);
      return;
    }

    if (n.is<update::assignable::shift::shift>()) {
      update_binary_instruction_gen_kill(assignable, shift);
      return;
    }

    if (n.is<update::assignable::shift::number>()) {
      update_binary_instruction_gen_kill(assignable, shift);
      return;
    }

    if (false
      || n.is<update::relative::arithmetic::add_comparable>()
      || n.is<update::relative::arithmetic::subtract_comparable>()
    ) {
      update_binary_instruction_gen_kill(relative, comparable);
      return;
    }

    if (false
      || n.is<update::assignable::arithmetic::add_relative>()
      || n.is<update::assignable::arithmetic::subtract_relative>()
    ) {
      update_binary_instruction_gen_kill(assignable, relative);
      return;
    }

    if (n.is<update::assignable::arithmetic::increment>()) {
      update_unary_instruction_gen_kill(assignable);
      return;
    }

    if (n.is<update::assignable::arithmetic::decrement>()) {
      update_unary_instruction_gen_kill(assignable);
      return;
    }

    if (n.is<jump::cjump::if_else>()) {
      assert(n.children.size() == 3);
      const node & cmp  = *n.children.at(0);
      /* const node & then = *n.children.at(1); */
      /* const node & els  = *n.children.at(2); */
      cmp_gen_kill(cmp);
      return;
    }

    if (n.is<jump::cjump::when>()) {
      /* namespace predicate = helper::cmp::predicate; */
      assert(n.children.size() == 2);
      const node & cmp  = *n.children.at(0);
      /* const node & then = *n.children.at(1); */
      cmp_gen_kill(cmp);
      return;
    }

    if (n.is<define::label>()) {
      assert(n.children.size() == 1);
      /* const node & label = *n.children.at(0); */
      return;
    }

    if (n.is<jump::go2>()) {
      assert(n.children.size() == 1);
      /* const node & label = *n.children.at(0); */
      return;
    }

    if (n.is<assign::assignable::gets_stack_arg>()) {
      assert(n.children.size() == 2);
      const node & dest = *n.children.at(0);
      /* const node & stack_arg = *n.children.at(1); */
      helper::operand::assignable::kill(n, dest, result);
      return;
    }

    if (n.is<assign::assignable::gets_comparison>()) {
      /* namespace predicate = helper::cmp::predicate; */
      assert(n.children.size() == 2);
      const node & dest = *n.children.at(0);
      helper::operand::assignable::kill(n, dest, result);
      const node & cmp  = *n.children.at(1);
      cmp_gen_kill(cmp);
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
      assert(n.children.size() == 2);
      const node & integer  = *n.children.at(1);
      int args  = ::helper::L2::integer(integer);
      if (args > 0) {
        namespace arg = calling_convention::call::argument;
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

    #undef assign_instruction_gen_kill
    #undef update_unary_instruction_gen_kill
    #undef update_binary_instruction_gen_kill
    #undef cmp_gen_kill

    std::cout << "something went wrong at\n\t" << n.name() << "\n";
    assert(false && "liveness::instruction: unreachable!");
  }
} // }}}

/**
 *
 * succ[cjump_when    ] = then_label && i + 1
 * succ[cjump_if_else ] = then_label && else_label
 * succ[call_u_N      ] = i + 1 // intraprocedural
 * succ[call_intrinsic] = i + 1 // intraprocedural
 * succ[<else>...     ] = i + i
 *
 */
namespace analysis::L2::liveness::successor { // {{{
  void set (const node & n, const node & s, liveness::result & result) {
    result.successor[&n].insert(&s);
  }

  void instruction (const node & n, int index, result & result) {
    using namespace grammar::instruction;

    nodes siblings = result.instructions;

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
      const node & next = *siblings.at(index + 1);
      return successor::set(n, next, result);
    }

    if (false
      || n.is<jump::go2>()
      || n.is<jump::cjump::when>()
      || n.is<jump::cjump::if_else>()
    ) {
      // Ugh. Jumps.
      if (n.is<jump::go2>()) {
        const node & label = helper::L2::definition_for(*n.children.at(0), siblings);
        return successor::set(n, label, result);
      }
      if (n.is<jump::cjump::when>()) {
        /* const node & cmp        = *n.children.at(0); */
        const node & then_label = *n.children.at(1);
        const node & then_instruction
          = helper::L2::definition_for(then_label, siblings);
        assert(siblings.size() > index + 1);
        const node & next = *siblings.at(index + 1);
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
      const node & next = *siblings.at(index + 1);
      return successor::set(n, next, result);
    }

    if (n.is<invoke::ret>()) {
      // Do nothing. A `return` has no successor in an our analysis.
      return;
    }

    std::cout << "something went wrong at\n\t" << n.name() << "\n";
    assert(false && "liveness::successor: unreachable!");
  }
} // }}}

/**
 *
 * IN[i]  = GEN[i] U (OUT[i] - KILL[i])
 * OUT[i] = U(s : successor of(i)) IN[s]
 *
 */
// liveness::in_out {{{
namespace analysis::L2::liveness {
  void in_out (result & result, unsigned debug = 0) {
    nodes instructions = result.instructions;
    // QUESTION: uh... what's our efficiency here? This code is gross.
    int iteration_limit = -1; // NOTE(jordan): used for debugging.
    bool fixed_state;
    do {
      fixed_state = true;
      for (int index = instructions.size() - 1; index >= 0; index--) {
        const node & instruction = *instructions.at(index);
        // load gen, kill sets and successors
        auto & gen        = result.gen [&instruction];
        auto & kill       = result.kill[&instruction];
        auto & successors = result.successor[&instruction];
        // copy in and out sets
        auto in  = result.in [&instruction];
        auto out = result.out[&instruction];
        // out_minus_kill = OUT[i] - KILL[i]
        std::set<std::string> out_minus_kill;
        std::set_difference(
          out.begin(), out.end(),
          kill.begin(), kill.end(),
          std::inserter(out_minus_kill, out_minus_kill.begin()));
        // debug {{{
        if (debug & DBG_IN_OUT_LOOP_OUT_MINUS_KILL) {
          std::cout << "OUT[" << index << "] - KILL[" << index << "] = ";
          for (auto x : out_minus_kill) {
            std::cout << x << " ";
          }
          std::cout << "\n";
        } // }}}
        // IN[i] = GEN[i] U (out_minus_kill)
        std::set_union(
          gen.begin(), gen.end(),
          out_minus_kill.begin(), out_minus_kill.end(),
          std::inserter(in, in.begin()));
        // OUT[i] = U(s : successor of(i)) IN[s]
        for (const node * successor : successors) {
          auto & in_s = result.in[successor];
          std::set_union(
            in_s.begin(), in_s.end(),
            out.begin(), out.end(),
            std::inserter(out, out.begin()));
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
          && std::equal(in_original.begin(), in_original.end(), in.begin(), in.end())
          && std::equal(out_original.begin(), out_original.end(), out.begin(), out.end());

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

// compute liveness {{{
namespace analysis::L2::liveness {
  void compute (liveness::result & result, unsigned debug = 0) {
    // 1. Compute GEN, KILL
    for (int index = 0; index < result.instructions.size(); index++) {
      const node & instruction = *result.instructions.at(index);
      analysis::L2::liveness::gen_kill::instruction(instruction, result);
      // debug {{{
      if (debug & DBG_GEN_KILL) {
        std::cout << "gen[" << index << "]  = ";
        for (auto g : result.gen[&instruction]) std::cout << g << " ";
        std::cout << "\n";
        std::cout << "kill[" << index << "] = ";
        for (auto r : result.kill[&instruction]) std::cout << r << " ";
        std::cout << "\n";
      }
      // }}}
    }
    // debug {{{
    if (debug & DBG_GEN_KILL) std::cout << "\n";
    // }}}

    // 2. Compute successors
    for (int index = 0; index < result.instructions.size(); index++) {
      namespace successor = analysis::L2::liveness::successor;
      const node & instruction = *result.instructions.at(index);
      successor::instruction(instruction, index, result);
      // debug {{{
      if (debug & DBG_SUCC) {
        std::cout << "succ[" << index << "] = ";
        for (auto successor : result.successor[&instruction]) {
          std::cout << successor->name();
        }
        std::cout << "\n";
      }
      // }}}
    }
    // debug {{{
    if (debug & DBG_SUCC) std::cout << "\n";
    // }}}

    // 3. Iteratively compute IN/OUT sets
    in_out(result, debug);

    // debug {{{
    if (debug & DBG_PRINT) {
      std::cout << "\n";
      for (int index = 0; index < result.instructions.size(); index++) {
        const node & instruction = *result.instructions.at(index);
        std::cout << "in[" << index << "]  = ";
        for (auto i : result.in[&instruction]) std::cout << i << " ";
        std::cout << "\n";
        std::cout << "out[" << index << "] = ";
        for (auto o : result.out[&instruction]) std::cout << o << " ";
        std::cout << "\n";
      }
      std::cout << "\n";
    }
    // }}}

    return;
  }
}
// }}}

namespace analysis::L2::liveness {
  result compute (const ast::node & root) {
    assert(root.is_root() && "liveness: got a non-root node!");
    assert(!root.children.empty() && "liveness: got an empty AST!");
    liveness::result result = {};
    const node & function = *root.children.at(0);
    result.instructions = helper::L2::collect_instructions(function);
    liveness::compute(result);
    return result;
  }

  void print (
    std::ostream & os,
    const result result,
    bool pretty = false
  ) {
    nodes instructions = result.instructions;
    os << (pretty ? "((in\n" : "(\n(in");
    for (int index = 0; index < instructions.size(); index++) {
      const ast::node & instruction = *instructions.at(index);
      auto in = result.in.at(&instruction);
      os << (pretty ? "  (" : "\n(");
      for (auto var : in) os << var << " ";
      os << (pretty ? ")\n" : ")");
    }
    //backspace thru: " )\n"
    os << (pretty ? "\b\b\b))\n" : "\n)");
    os << (pretty ? "(out\n"     : "\n\n(out");
    for (int index = 0; index < instructions.size(); index++) {
      const ast::node & instruction = *instructions.at(index);
      auto out = result.out.at(&instruction);
      os << (pretty ? "  (" : "\n(");
      for (auto var : out) os << var << " ";
      os << (pretty ? ")\n" : ")");
    }
    //backspace thru: " )\n"
    os << (pretty ? "\b\b\b))\n" : "\n)\n\n)\n");
  }
}
// }}}

// interference {{{
namespace analysis::L2::interference {
  struct result {
    nodes instructions;
    std::vector<std::string> variables;
    liveness::result liveness;
    interference_map graph;
  };

  namespace graph {
    // TODO(jordan): uh, not this.
    void biconnect (
      result & result,
      const std::string & a,
      const std::string & b
    ) {
      result.graph[a].insert(b);
      result.graph[b].insert(a);
    }
  }

  // FIXME(jordan): these helpers are gross.
  namespace graph::x86_64_register { // {{{
    namespace register_helper = helper::L2::x86_64_register;
    using namespace grammar::identifier::x86_64_register;

    void connect_to (result & result, const std::string & origin) {
      auto & interferes = result.graph[origin];
      biconnect(result, origin, register_helper::as_string<rax>::value);
      biconnect(result, origin, register_helper::as_string<rbx>::value);
      biconnect(result, origin, register_helper::as_string<rcx>::value);
      biconnect(result, origin, register_helper::as_string<rdx>::value);
      biconnect(result, origin, register_helper::as_string<rsi>::value);
      biconnect(result, origin, register_helper::as_string<rdi>::value);
      biconnect(result, origin, register_helper::as_string<rbp>::value);
      biconnect(result, origin, register_helper::as_string<r8 >::value);
      biconnect(result, origin, register_helper::as_string<r9 >::value);
      biconnect(result, origin, register_helper::as_string<r10>::value);
      biconnect(result, origin, register_helper::as_string<r11>::value);
      biconnect(result, origin, register_helper::as_string<r12>::value);
      biconnect(result, origin, register_helper::as_string<r13>::value);
      biconnect(result, origin, register_helper::as_string<r14>::value);
      biconnect(result, origin, register_helper::as_string<r15>::value);
      interferes.erase(origin);
    }

    template <typename Register>
    void connect_to (result & result) {
      connect_to(result, register_helper::as_string<Register>::value);
    }

    // FIXME(jordan): never a sin without its just reward...
    void connect_all (result & result) {
      connect_to<rax>(result);
      connect_to<rbx>(result);
      connect_to<rcx>(result);
      connect_to<rdx>(result);
      connect_to<rsi>(result);
      connect_to<rdi>(result);
      connect_to<rbp>(result);
      connect_to<r8 >(result);
      connect_to<r9 >(result);
      connect_to<r10>(result);
      connect_to<r11>(result);
      connect_to<r12>(result);
      connect_to<r13>(result);
      connect_to<r14>(result);
      connect_to<r15>(result);
    }
  } // }}}
}

namespace analysis::L2::interference { // {{{
  void compute (interference::result & result) {
    namespace assign = grammar::instruction::assign;
    namespace update = grammar::instruction::update;
    // 0. Connect all registers to one another. Don't look closely.
    graph::x86_64_register::connect_all(result);
    // Iterate over all of the instructions, and...
    for (int index = 0; index < result.instructions.size(); index++) {
      const auto & instruction = *result.instructions.at(index);
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
        assert(instruction.children.size() == 2);
        const node & src = *instruction.children.at(1);
        if (helper::L2::matches<operand::memory>(src)) {
          // This is a variable 'gets' a variable or register.
          connect_kill = false;
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
        bool is_variable = value.is<operand::variable>();
        const std::string & variable
          = is_variable
          ? helper::L2::variable::get_name(value)
          : value.content();
        // FIXME(jordan): yeeuuup. This is horrible, I know. But... ugh.
        namespace register_help = helper::L2::x86_64_register;
        using namespace grammar::identifier::x86_64_register;
        using namespace graph;
        biconnect(result, variable, register_help::as_string<rax>::value);
        biconnect(result, variable, register_help::as_string<rbx>::value);
        biconnect(result, variable, register_help::as_string<rdx>::value);
        biconnect(result, variable, register_help::as_string<rsi>::value);
        biconnect(result, variable, register_help::as_string<rdi>::value);
        biconnect(result, variable, register_help::as_string<rbp>::value);
        biconnect(result, variable, register_help::as_string<r8 >::value);
        biconnect(result, variable, register_help::as_string<r9 >::value);
        biconnect(result, variable, register_help::as_string<r10>::value);
        biconnect(result, variable, register_help::as_string<r11>::value);
        biconnect(result, variable, register_help::as_string<r12>::value);
        biconnect(result, variable, register_help::as_string<r13>::value);
        biconnect(result, variable, register_help::as_string<r14>::value);
        biconnect(result, variable, register_help::as_string<r15>::value);
        result.graph[variable].erase(variable);
      }
    }
  }
} // }}}

namespace analysis::L2::interference {
  result compute (const ast::node & root) {
    assert(root.is_root() && "interference: got a non-root node!");
    assert(!root.children.empty() && "interference: got an empty AST!");
    liveness::result liveness_result = liveness::compute(root);
    std::vector<std::string> variables
      = helper::L2::collect_variables(liveness_result.instructions);
    interference::result result = {
      liveness_result.instructions,
      variables,
      liveness_result
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
      os << origin << " ";
      for (const std::string & interfering : interferes) {
        if (interfering != origin) os << interfering << " ";
      }
      os << "\n";
    }
    return;
  }
}
// }}}
