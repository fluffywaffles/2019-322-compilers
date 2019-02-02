// vim: set foldmethod=marker:
#pragma once
#include <map>
#include <set>
#include <cassert>
#include <iostream>
#include <iterator>
#include <algorithm>

#include "L1/codegen.h"
#include "util.h"
#include "grammar.h"
#include "parse_tree.h"

#define GEN_KILL_DEBUG \
        false
#define OP_GEN_KILL_FILTER_DEBUG \
        GEN_KILL_DEBUG

namespace L2::analysis::ast::liveness {
  using namespace L2::grammar;
  using namespace L2::parse_tree;

  using liveness_map  = std::map<const node *, std::set<std::string>>;
  using successor_map = std::map<const node *, std::set<const node *>>;
  using nodes = std::vector<std::shared_ptr<const node>>;
  struct result {
    nodes instructions;
    liveness_map in;
    liveness_map out;
    liveness_map gen;
    liveness_map kill;
    successor_map successor;
  };

  namespace helper { // {{{
    #define L1 L1::codegen::ast::generate::helper::matches
    template <class R> bool matches (const node & n) { return L1<R>(n); }
    template <class R> bool matches (std::string  s) { return L1<R>(s); }
  }

  namespace helper {
    int integer (const node & n) {
      assert(n.has_content() && "helper::integer: no content!");
      return std::stoi(n.content());
    }
  }

  // NOTE(jordan): woof. Template specialization, amirite?
  namespace helper::x86_64_register {
    namespace reg = identifier::x86_64_register;
    using string  = std::string;
    // NOTE(jordan): delete the default converter to prevent use!
    template <typename Register> string to_string () = delete;
    #define handle(R) template<> string to_string<reg::R>(){ return #R; }
    handle(rax) handle(rbx) handle(rcx) handle(rdx) handle(rsi)
    handle(rdi) handle(rbp) handle(rsp) handle(r8 ) handle(r9 )
    handle(r10) handle(r11) handle(r12) handle(r13) handle(r14)
    handle(r15)
  } // }}}

  namespace helper::variable { // {{{
    std::string get_name (const node & variable) {
      assert(variable.children.size() == 1 && "missing child!");
      const node & name = *variable.children.at(0);
      assert(name.is<identifier::name>() && "child is not a 'name'!");
      return name.content();
    }
  } // }}}
}

/**
 *
 * GEN[i]  = { <read variables> }
 * KILL[i] = { <defined variables> }
 *
 */
namespace L2::analysis::ast::liveness::gen_kill { // {{{
  using namespace L2::parse_tree;
  using namespace L2::grammar;

  // NOTE(jordan): import helpers from outer scope.
  namespace helper { using namespace liveness::helper; } // {{{

  namespace helper {
    // NOTE(jordan): macros are a dangerous, beautiful weapon
    #define implement_default_gen_kill(WHICH, DEBUG)                     \
    static_assert(                                                       \
      static_string_eq   (#WHICH, "gen")                                 \
      || static_string_eq(#WHICH, "kill")                                \
    );                                                                   \
    /* standard version */                                               \
    void WHICH (const node & i, const node & g, result & result) {       \
      if (DEBUG) std::cout << #WHICH " name() " << g.name() << "\n";     \
      bool is_variable = g.is<identifier::variable>();                   \
      assert(                                                            \
        is_variable || matches<register_set::any>(g)                     \
        && "helper::" #WHICH ": '" #WHICH "' not register or variable!"  \
      );                                                                 \
      if (matches<identifier::x86_64_register::rsp>(g)) {                \
        if (DEBUG) std::cout << #WHICH ": ignoring rsp.\n";              \
        return;                                                          \
      }                                                                  \
      auto s = is_variable ? helper::variable::get_name(g) : g.content();\
      if (DEBUG) std::cout << #WHICH " content" << s << "\n";            \
      result.WHICH[&i].insert(s);                                        \
      return;                                                            \
    }                                                                    \
    /* register-type-templated version */                                \
    template <typename R>                                                \
    void WHICH (const node & i, liveness::result & result) {             \
      assert(!matches<R>("rsp") && "helper::" #WHICH " cannot gen rsp!");\
      std::string reg_string = helper::x86_64_register::to_string<R>();  \
      if (DEBUG) std::cout << #WHICH"<Register>: " << reg_string << "\n";\
      result.WHICH[&i].insert(reg_string);                               \
      return;                                                            \
    }
    implement_default_gen_kill(gen,  GEN_KILL_DEBUG)
    implement_default_gen_kill(kill, GEN_KILL_DEBUG)
  }
  // }}}

  namespace helper::operand { // {{{
    const node & unwrap_first_child_if_exists (const node & parent) {
      return parent.children.size() > 0 ? *parent.children.at(0) : parent;
    }
    #define implement_default_operand_gen_kill(WHICH)                    \
    static_assert(                                                       \
      static_string_eq   (#WHICH, "gen")                                 \
      || static_string_eq(#WHICH, "kill")                                \
    );                                                                   \
    void WHICH (const node & n, const node & g, result & result) {       \
      const node & value = unwrap_first_child_if_exists(g);              \
      return helper::WHICH(n, value, result);                            \
    }
    implement_default_operand_gen_kill(gen)
    implement_default_operand_gen_kill(kill)
    // NOTE(jordan): the 2 "basic" operand types just alias the defaults.
    namespace assignable { using namespace helper::operand; }
    namespace memory     { using namespace helper::operand; }
    #define operand_gen_kill_filter(predicate)                           \
    bool filter (const node & g) {                                       \
      const node & v = unwrap_first_child_if_exists(g);                  \
      bool accept = predicate;                                           \
      if (OP_GEN_KILL_FILTER_DEBUG) {                                    \
        std::cout << "filter on: '" << v.name() << "'"                   \
                  << " accept? " << accept << "\n";                      \
      }                                                                  \
      return accept;                                                     \
    }
    #define implement_operand_gen_kill(WHICH, ACTION)                    \
    static_assert(                                                       \
      static_string_eq   (#WHICH, "gen")                                 \
      || static_string_eq(#WHICH, "kill")                                \
    );                                                                   \
    void WHICH (const node & n, const node & g, result & result) {       \
      if (filter(g)) {                                                   \
        const node & value = unwrap_first_child_if_exists(g);            \
        return ACTION;                                                   \
      }                                                                  \
    }
  }

  namespace helper::operand::shift {
    using number = grammar::operand::number;
    operand_gen_kill_filter(!helper::matches<number>(v))
    implement_operand_gen_kill(gen, operand::gen(n, value, result))
    implement_operand_gen_kill(kill, operand::kill(n, value, result))
  }

  namespace helper::operand::movable {
    using label  = grammar::operand::label;
    using number = grammar::operand::number;
    operand_gen_kill_filter(!v.is<label>() && !helper::matches<number>(v))
    /* bool filter (const node & g) { */
    /*   const node & value = *g.children.at(0); */
    /*   return true */
    /*     && !value.is<grammar::operand::label>() */
    /*     && !helper::matches<grammar::operand::number>(value); */
    /* } */
    implement_operand_gen_kill(gen, memory::gen(n, value, result))
    implement_operand_gen_kill(kill, memory::kill(n, value, result))
    /* void gen (const node & n, const node & g, liveness::result & result) { */
    /*   if (filter(g)) { */
    /*     const node & memory = *g.children.at(0); */
    /*     return memory::gen(n, memory, result); */
    /*   } */
    /* } */
    /* void kill (const node & n, const node & g, liveness::result & result) { */
    /*   if (filter(g)) { */
    /*     const node & memory = *g.children.at(0); */
    /*     return memory::kill(n, memory, result); */
    /*   } */
    /* } */
  }

  namespace helper::operand::relative {
    operand_gen_kill_filter(true)
    implement_operand_gen_kill(gen, memory::gen(n, value, result))
    implement_operand_gen_kill(kill, memory::kill(n, value, result))
    /* void gen (const node & n, const node & g, liveness::result & result) { */
    /*   assert(g.children.size() == 2); */
    /*   const node & base = *g.children.at(0); */
    /*   return memory::gen(n, base, result); */
    /* } */
    /* void kill (const node & n, const node & g, liveness::result & result) { */
    /*   assert(g.children.size() == 2); */
    /*   const node & base = *g.children.at(0); */
    /*   return memory::kill(n, base, result); */
    /* } */
  }

  namespace helper::operand::comparable {
    using number = grammar::operand::number;
    operand_gen_kill_filter(!helper::matches<number>(v))
    implement_operand_gen_kill(gen, memory::gen(n, value, result))
    implement_operand_gen_kill(kill, memory::kill(n, value, result))
    /* bool filter (const node & g) { */
    /*   return !helper::matches<grammar::operand::number>(g); */
    /* } */
    /* void gen (const node & n, const node & g, liveness::result & result) { */
    /*   if (filter(g)) { */
    /*     const node & memory = *g.children.at(0); */
    /*     return memory::gen(n, memory, result); */
    /*   } */
    /* } */
    /* void kill (const node & n, const node & g, liveness::result & result) { */
    /*   if (filter(g)) { */
    /*     const node & memory = *g.children.at(0); */
    /*     return memory::gen(n, memory, result); */
    /*   } */
    /* } */
  }

  namespace helper::operand::callable {
    operand_gen_kill_filter(!v.is<grammar::operand::label>())
    implement_operand_gen_kill(gen, assignable::gen(n, value, result))
    implement_operand_gen_kill(kill, assignable::kill(n, value, result))
    /* bool filter (const node & g) { */
    /*   const node & value = *g.children.at(0); */
    /*   return !value.is<grammar::operand::label>(); */
    /* } */
    /* void gen (const node & n, const node & g, liveness::result & result) { */
    /*   if (filter(g)) { */
    /*     const node & assignable = *g.children.at(0); */
    /*     return assignable::gen(n, assignable, result); */
    /*   } */
    /* } */
    /* void kill (const node & n, const node & g, liveness::result & result) { */
    /*   if (filter(g)) { */
    /*     const node & assignable = *g.children.at(0); */
    /*     return assignable::gen(n, assignable, result); */
    /*   } */
    /* } */
  }
  // }}}

  void instruction (const node & n, liveness::result & result) {
    using namespace L2::grammar::instruction;

    /* std::cout << "gen/kill on: " << n.name() << "\n"; */

    if (n.is<instruction::any>()) {
      assert(n.children.size() == 1);
      const node & actual_instruction = *n.children.at(0);
      return gen_kill::instruction(actual_instruction, result);
    }

    #define assign_instruction_gen_kill(DEST, SRC)                       \
      assert(n.children.size() == 2);                                    \
      const node & dest = *n.children.at(0);                             \
      const node & src  = *n.children.at(1);                             \
      helper::operand::SRC::gen(n, src, result);                         \
      helper::operand::DEST::kill(n, dest, result);                      \
      return

    #define update_binary_instruction_gen_kill(DEST, SRC)                \
      assert(n.children.size() == 3);                                    \
      const node & dest = *n.children.at(0);                             \
      /* const node & op   = *n.children.at(1); */                       \
      const node & src  = *n.children.at(2);                             \
      helper::operand::SRC::gen(n, src, result);                         \
      helper::operand::DEST::gen(n, dest, result);                       \
      helper::operand::DEST::kill(n, dest, result);                      \
      return

    #define update_unary_instruction_gen_kill(DEST)                      \
      assert(n.children.size() == 2);                                    \
      const node & dest = *n.children.at(0);                             \
      /* const node & op   = *n.children.at(1); */                       \
      helper::operand::DEST::gen(n, dest, result);                       \
      helper::operand::DEST::kill(n, dest, result);                      \
      return

    #define cmp_gen_kill(CMP)                                            \
      assert(CMP.children.size() == 3);                                  \
      const node & lhs = *CMP.children.at(0);                            \
      /* const node & op  = *CMP.children.at(1); */                      \
      const node & rhs = *CMP.children.at(2);                            \
      helper::operand::comparable::gen(n, lhs, result);                  \
      helper::operand::comparable::gen(n, rhs, result);                  \
      return

    if (n.is<assign::assignable::gets_movable>()) {
      assign_instruction_gen_kill(assignable, movable);
    }

    if (n.is<assign::assignable::gets_relative>()) {
      assign_instruction_gen_kill(assignable, relative);
    }

    if (n.is<assign::relative::gets_movable>()) {
      assign_instruction_gen_kill(relative, movable);
    }

    if (n.is<update::assignable::arithmetic::comparable>()) {
      update_binary_instruction_gen_kill(assignable, comparable);
    }

    if (n.is<update::assignable::shift::shift>()) {
      update_binary_instruction_gen_kill(assignable, shift);
    }

    if (n.is<update::assignable::shift::number>()) {
      update_binary_instruction_gen_kill(assignable, shift);
    }

    if (false
      || n.is<update::relative::arithmetic::add_comparable>()
      || n.is<update::relative::arithmetic::subtract_comparable>()
    ) {
      update_binary_instruction_gen_kill(relative, comparable);
    }

    if (false
      || n.is<update::assignable::arithmetic::add_relative>()
      || n.is<update::assignable::arithmetic::subtract_relative>()
    ) {
      update_binary_instruction_gen_kill(assignable, relative);
    }

    if (n.is<update::assignable::arithmetic::increment>()) {
      update_unary_instruction_gen_kill(assignable);
    }

    if (n.is<update::assignable::arithmetic::decrement>()) {
      update_unary_instruction_gen_kill(assignable);
    }

    if (n.is<jump::cjump::if_else>()) {
      assert(n.children.size() == 3);
      const node & cmp  = *n.children.at(0);
      /* const node & then = *n.children.at(1); */
      /* const node & els  = *n.children.at(2); */
      cmp_gen_kill(cmp);
    }

    if (n.is<jump::cjump::when>()) {
      /* namespace predicate = helper::cmp::predicate; */
      assert(n.children.size() == 2);
      const node & cmp  = *n.children.at(0);
      /* const node & then = *n.children.at(1); */
      cmp_gen_kill(cmp);
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
      helper::gen<identifier::x86_64_register::rax>(n, result);
      namespace callee = register_group::callee_save;
      helper::gen<callee::r12>(n, result);
      helper::gen<callee::r13>(n, result);
      helper::gen<callee::r14>(n, result);
      helper::gen<callee::r15>(n, result);
      helper::gen<callee::rbp>(n, result);
      helper::gen<callee::rbx>(n, result);
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
      int args  = helper::integer(integer);
      /* int spill = 8; // return address! */
      /* if (args  > 6) spill += 8 * (args - 6); */
      if (args > 0) {
        namespace arg = register_group::argument;
        if (args >= 1) helper::gen<arg::rdi>(n, result);
        if (args >= 2) helper::gen<arg::rsi>(n, result);
        if (args >= 3) helper::gen<arg::rdx>(n, result);
        if (args >= 4) helper::gen<arg::rcx>(n, result);
        if (args >= 5) helper::gen<arg::r8 >(n, result);
        if (args >= 6) helper::gen<arg::r9 >(n, result);
      }
      helper::kill<identifier::x86_64_register::rax>(n, result);
      namespace caller = register_group::caller_save;
      helper::kill<caller::r8 >(n, result);
      helper::kill<caller::r9 >(n, result);
      helper::kill<caller::r10>(n, result);
      helper::kill<caller::r11>(n, result);
      helper::kill<caller::rax>(n, result);
      helper::kill<caller::rcx>(n, result);
      helper::kill<caller::rdi>(n, result);
      helper::kill<caller::rdx>(n, result);
      helper::kill<caller::rsi>(n, result);
      if (n.is<invoke::call::callable>()) {
        const node & callable = *n.children.at(0);
        helper::operand::callable::gen(n, callable, result);
      }
      return;
    }

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
namespace L2::analysis::ast::successor { // {{{
  using namespace L2::parse_tree;
  using namespace L2::grammar;
  using nodes = std::vector<std::shared_ptr<const node>>;

  // NOTE(jordan): import helpers from outer scope.
  namespace helper { using namespace liveness::helper; }

  namespace helper {
    void set (const node & n, const node & s, liveness::result & result) {
      result.successor[&n].insert(&s);
    }
    const node & definition_for (const node & label, nodes instructions) {
      assert(label.is<operand::label>()
          && "definition_for: called on a non-label!");
      for (auto & instruction_ptr : instructions) {
        const node & instruction = *instruction_ptr;
        if (instruction.is<instruction::define::label>()) {
          const node & defined_label = *instruction.children.at(0);
          assert(defined_label.is<operand::label>());
          if (defined_label.content() == label.content())
            return instruction;
        }
      }
      assert(false && "definition_for: could not find label!");
    }
  }

  void instruction (
    const node & n,
    int index,
    liveness::result & result,
    nodes siblings
  ) {
    using namespace L2::grammar::instruction;

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
      return successor::helper::set(n, next, result);
    }

    if (false
      || n.is<jump::go2>()
      || n.is<jump::cjump::when>()
      || n.is<jump::cjump::if_else>()
    ) {
      // Ugh. Jumps.
      if (n.is<jump::go2>()) {
        const node & label = helper::definition_for(*n.children.at(0), siblings);
        return successor::helper::set(n, label, result);
      }
      if (n.is<jump::cjump::when>()) {
        /* const node & cmp        = *n.children.at(0); */
        const node & then_label = *n.children.at(1);
        const node & then_instruction
          = helper::definition_for(then_label, siblings);
        assert(siblings.size() > index + 1);
        const node & next = *siblings.at(index + 1);
        successor::helper::set(n, next, result);
        successor::helper::set(n, then_instruction, result);
        return;
      }
      if (n.is<jump::cjump::if_else>()) {
        /* const node & cmp        = *n.children.at(0); */
        const node & then_label = *n.children.at(1);
        const node & then_instruction
          = helper::definition_for(then_label, siblings);
        const node & else_label = *n.children.at(2);
        const node & else_instruction
          = helper::definition_for(else_label, siblings);
        successor::helper::set(n, then_instruction, result);
        successor::helper::set(n, else_instruction, result);
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
      return successor::helper::set(n, next, result);
    }

    if (n.is<invoke::ret>()) {
      // Do nothing. A `return` has no successor in an our analysis.
      return;
    }

    std::cout << "something went wrong at\n\t" << n.name() << "\n";
    assert(false && "liveness::successor: unreachable!");
  }
  void instruction (const node & n, int index, liveness::result & res) {
    return instruction(n, index, res, res.instructions);
  }
} // }}}

/**
 *
 * IN[i]  = GEN[i] U (OUT[i] - KILL[i])
 * OUT[i] = U(s : successor of(i)) IN[s]
 *
 */
namespace L2::analysis::ast::liveness {
  using nodes = std::vector<std::shared_ptr<const node>>;

  nodes collect_instructions (const node & function) { // {{{
    nodes result;
    assert(function.is<L2::grammar::function::define>());
    assert(function.children.size() == 4);
    const node & instructions = *function.children.at(3);
    for (auto & instruction : instructions.children) {
      assert(instruction->is<L2::grammar::instruction::any>());
      assert(instruction->children.size() == 1);
      auto shared_instruction = std::shared_ptr<const node>(
        std::move(instruction->children.at(0))
      );
      result.push_back(shared_instruction);
    }
    return result;
  }

  void collect_instructions (const node & function, result & result) {
    result.instructions = collect_instructions(function);
  } // }}}

  /* FIXME(jordan): this code is kinda gross. Refactor? I honestly
   * suspect the code would be cleaner if we didn't use std-library set
   * operations and just iterated over things.
   */
  void in_out (result & result, nodes instructions) {
    // QUESTION: uh... what's our efficiency here? This code is gross.
    int iteration_limit = -1;
    bool fixed_state;
    do {
      fixed_state = true;
      for (int index = 0; index < instructions.size(); index++) {
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
        /* { */
        /*   std::cout << "OUT[" << index << "] - KILL[" << index << "] = "; */
        /*   for (auto x : out_minus_kill) { */
        /*     std::cout << x << " "; */
        /*   } */
        /*   std::cout << "\n"; */
        /* } */
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
        /* { */
        /*   std::cout << "in_original[" << index << "]  = "; */
        /*   for (auto i : in_original) std::cout << i << " "; */
        /*   std::cout << "\n"; */
        /*   std::cout << "out_original[" << index << "] = "; */
        /*   for (auto o : out_original) std::cout << o << " "; */
        /*   std::cout << "\n"; */
        /*   std::cout << "in[" << index << "]  = "; */
        /*   for (auto i : in) std::cout << i << " "; */
        /*   std::cout << "\n"; */
        /*   std::cout << "out[" << index << "] = "; */
        /*   for (auto o : out) std::cout << o << " "; */
        /*   std::cout << "\n"; */
        /* } */
        fixed_state = fixed_state
          && std::equal(in_original.begin(), in_original.end(), in.begin(), in.end())
          && std::equal(out_original.begin(), out_original.end(), out.begin(), out.end());

        result.in [&instruction] = in;
        result.out[&instruction] = out;
      }
      /* { */
      /*   std::cout << "fixed_state?    " << fixed_state << "\n"; */
      /*   std::cout << "hit iter limit? " << ((iteration_limit - 1) > 0) << "\n"; */
      /*   std::cout << "cond: " << (!fixed_state || ((iteration_limit - 1) > 0)) << "\n"; */
      /* } */
    } while (!fixed_state || (--iteration_limit > 0));
  }
  void in_out (result & result) {
    in_out(result, result.instructions);
  }

  void root (const node & root, liveness::result & result) {
    // 1. Compute GEN, KILL
    collect_instructions(*root.children.at(0), result);

    for (int index = 0; index < result.instructions.size(); index++) {
      const node & instruction = *result.instructions.at(index);
      analysis::ast::liveness::gen_kill::instruction(instruction, result);
      /* { */
      /*   std::cout << "gen[" << index << "]  = "; */
      /*   for (auto g : result.gen[&instruction]) std::cout << g << " "; */
      /*   std::cout << "\n"; */
      /*   std::cout << "kill[" << index << "] = "; */
      /*   for (auto r : result.kill[&instruction]) std::cout << r << " "; */
      /*   std::cout << "\n"; */
      /* } */
    }
    /* std::cout << "\n"; */

    for (int index = 0; index < result.instructions.size(); index++) {
      const node & instruction = *result.instructions.at(index);
      analysis::ast::successor::instruction(instruction, index, result);
      /* { */
      /*   std::cout << "succ[" << index << "] = "; */
      /*   for (auto successor : result.successor[&instruction]) { */
      /*     std::cout << successor->name(); */
      /*   } */
      /*   std::cout << "\n"; */
      /* } */
    }
    /* std::cout << "\n"; */

    in_out(result);

    /* std::cout << "\n"; */
    /* for (int index = 0; index < instructions.size(); index++) { */
    /*   const node & instruction = *instructions.at(index); */
    /*   std::cout << "in[" << index << "]  = "; */
    /*   for (auto i : result.in[&instruction]) std::cout << i << " "; */
    /*   std::cout << "\n"; */
    /*   std::cout << "out[" << index << "] = "; */
    /*   for (auto o : result.out[&instruction]) std::cout << o << " "; */
    /*   std::cout << "\n"; */
    /* } */
    /* std::cout << "\n"; */

    return;
  }
}

namespace L2::analysis::liveness {
  using result = ast::liveness::result;
  using nodes = std::vector<std::shared_ptr<const parse_tree::node>>;

  result compute (const parse_tree::node & root) {
    assert(root.is_root() && "generate: got a non-root node!");
    assert(!root.children.empty() && "generate: got an empty AST!");

    result result = {};
    ast::liveness::root(root, result);
    return result;
  }

  void print (
    std::ostream& os,
    result & result,
    nodes instructions,
    bool pretty = false
  ) {
    os << (pretty ? "((in\n" : "(\n(in");
    for (int index = 0; index < instructions.size(); index++) {
      const parse_tree::node & instruction = *instructions.at(index);
      auto in = result.in[&instruction];
      os << (pretty ? "  (" : "\n(");
      for (auto var : in) os << var << " ";
      os << (pretty ? ")\n" : ")");
    }
    //backspace thru: " )\n"
    os << (pretty ? "\b\b\b))\n" : "\n)");
    os << (pretty ? "(out\n"     : "\n\n(out");
    for (int index = 0; index < instructions.size(); index++) {
      const parse_tree::node & instruction = *instructions.at(index);
      auto out = result.out[&instruction];
      os << (pretty ? "  (" : "\n(");
      for (auto var : out) os << var << " ";
      os << (pretty ? ")\n" : ")");
    }
    //backspace thru: " )\n"
    os << (pretty ? "\b\b\b))\n" : "\n)\n\n)\n");
  }
  void print (std::ostream& os, result & result, bool pretty = false) {
    return print(os, result, result.instructions, pretty);
  }
}
