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
#include "parse_tree.h"

namespace L2::analysis::ast::liveness {
  using namespace L2::parse_tree;

  using liveness_map  = std::map<const node *, std::set<std::string>>;
  using successor_map = std::map<const node *, std::set<const node *>>;
  struct result {
    liveness_map in;
    liveness_map out;
    liveness_map gen;
    liveness_map kill;
    successor_map successor;
  };

  namespace helper { // {{{
    template <typename Rule>
      bool matches (const node & n) {
        return L1::codegen::ast::generate::helper::matches<Rule>(n);
      }
    template <typename Rule>
      bool matches (std::string s) {
        return L1::codegen::ast::generate::helper::matches<Rule>(s);
      }
  }

  namespace helper {
    int integer (const node & n) {
      assert(n.has_content() && "helper::integer: no content!");
      return std::stoi(n.content());
    }
  }

  // NOTE(jordan): woof. Template specialization, amirite?
  namespace helper {
    namespace reg = L2::grammar::literal::identifier::x86_64_register;
    using string  = std::string;
    // NOTE(jordan): delete the default converter to prevent use!
    template <typename Register> string register_to_string () = delete;
    template <> string register_to_string <reg::rax> () { return "rax"; }
    template <> string register_to_string <reg::rbx> () { return "rbx"; }
    template <> string register_to_string <reg::rcx> () { return "rcx"; }
    template <> string register_to_string <reg::rdx> () { return "rdx"; }
    template <> string register_to_string <reg::rsi> () { return "rsi"; }
    template <> string register_to_string <reg::rdi> () { return "rdi"; }
    template <> string register_to_string <reg::rbp> () { return "rbp"; }
    template <> string register_to_string <reg::rsp> () { return "rsp"; }
    template <> string register_to_string <reg::r8 > () { return  "r8"; }
    template <> string register_to_string <reg::r9 > () { return  "r9"; }
    template <> string register_to_string <reg::r10> () { return "r10"; }
    template <> string register_to_string <reg::r11> () { return "r11"; }
    template <> string register_to_string <reg::r12> () { return "r12"; }
    template <> string register_to_string <reg::r13> () { return "r13"; }
    template <> string register_to_string <reg::r14> () { return "r14"; }
    template <> string register_to_string <reg::r15> () { return "r15"; }
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

  namespace helper::variable { // {{{
    std::string get_name (const node & variable) {
      assert(variable.children.size() == 1
          && "helper::variable: missing child!");
      const node & name = *variable.children.at(0);
      assert(name.is<identifier::name>()
          && "helper::variable: child is not a 'name'!");
      return name.content();
    }
  } // }}}

  // NOTE(jordan): import helpers from outer scope.
  namespace helper { using namespace liveness::helper; } // {{{

  namespace helper {
    void gen (const node & i, const node & g, liveness::result & result) {
      /* std::cout << "hi you " << g.name() << "\n"; */
      assert(
        g.is<identifier::variable>() || matches<register_set::any>(g)
        && "helper::gen: 'gen' is neither register nor variable!"
      );
      std::string name
        = g.is<identifier::variable>()
        ? helper::variable::get_name(g)
        : g.content();
      /* std::cout << "hi you " << name << "\n"; */
      // FIXME(jordan): include x86_64_register in AST & check type
      if (name != "rsp") result.gen[&i].insert(name);
      return;
    }
    template <typename Register>
    void gen (const node & i, liveness::result & result) {
      assert(!matches<Register>("rsp") && "helper::gen: cannot gen 'rsp'!");
      result.gen[&i].insert(helper::register_to_string<Register>());
      return;
    }
  }

  namespace helper {
    void kill (const node & i, const node & g, liveness::result & result) {
      /* std::cout << "fuck you " << g.name() << "\n"; */
      assert(
        g.is<identifier::variable>() || matches<register_set::any>(g)
        && "helper::kill: 'kill' is neither register nor variable!"
      );
      std::string name
        = g.is<identifier::variable>()
        ? helper::variable::get_name(g)
        : g.content();
      /* std::cout << "fuck you " << name << "\n"; */
      result.kill[&i].insert(name);
      return;
    }
    template <typename Register>
    void kill (const node & i, liveness::result & result) {
      result.kill[&i].insert(helper::register_to_string<Register>());
      return;
    }
  }
  // }}}

  namespace helper::operand { // {{{
    void gen (const node & n, const node & g, liveness::result & result) {
      const node & value = g.children.size() == 1 ? *g.children.at(0) : g;
      return helper::gen(n, value, result);
    }
    void kill (const node & n, const node & g, liveness::result & result) {
      const node & value = g.children.size() == 1 ? *g.children.at(0) : g;
      return helper::kill(n, value, result);
    }
    namespace shift {
      void gen (const node & n, const node & g, liveness::result & result) {
        return helper::operand::gen(n, g, result);
      }
      void kill (const node & n, const node & g, liveness::result & result) {
        return helper::operand::kill(n, g, result);
      }
    }
    namespace assignable {
      void gen (const node & n, const node & g, liveness::result & result) {
        return helper::operand::gen(n, g, result);
      }
      void kill (const node & n, const node & g, liveness::result & result) {
        return helper::operand::kill(n, g, result);
      }
    }
    namespace memory {
      void gen (const node & n, const node & g, liveness::result & result) {
        return helper::operand::gen(n, g, result);
      }
      void kill (const node & n, const node & g, liveness::result & result) {
        return helper::operand::kill(n, g, result);
      }
    }
    namespace movable {
      bool filter (const node & g) {
        const node & value = *g.children.at(0);
        return true
          && !value.is<grammar::operand::label>()
          && !helper::matches<grammar::operand::number>(value);
      }
      void gen (const node & n, const node & g, liveness::result & result) {
        if (filter(g)) {
          const node & memory = *g.children.at(0);
          return memory::gen(n, memory, result);
        }
      }
      void kill (const node & n, const node & g, liveness::result & result) {
        if (filter(g)) {
          const node & memory = *g.children.at(0);
          return memory::kill(n, memory, result);
        }
      }
    }
    namespace relative {
      void gen (const node & n, const node & g, liveness::result & result) {
        assert(g.children.size() == 2);
        const node & base = *g.children.at(0);
        return memory::gen(n, base, result);
      }
      void kill (const node & n, const node & g, liveness::result & result) {
        assert(g.children.size() == 2);
        const node & base = *g.children.at(0);
        return memory::kill(n, base, result);
      }
    }
    namespace comparable {
      bool filter (const node & g) {
        return !helper::matches<grammar::operand::number>(g);
      }
      void gen (const node & n, const node & g, liveness::result & result) {
        if (filter(g)) {
          const node & memory = *g.children.at(0);
          return memory::gen(n, memory, result);
        }
      }
      void kill (const node & n, const node & g, liveness::result & result) {
        if (filter(g)) {
          const node & memory = *g.children.at(0);
          return memory::gen(n, memory, result);
        }
      }
    }
    namespace callable {
      bool filter (const node & g) {
        const node & value = *g.children.at(0);
        return !value.is<grammar::operand::label>();
      }
      void gen (const node & n, const node & g, liveness::result & result) {
        if (filter(g)) {
          const node & assignable = *g.children.at(0);
          return assignable::gen(n, assignable, result);
        }
      }
      void kill (const node & n, const node & g, liveness::result & result) {
        if (filter(g)) {
          const node & assignable = *g.children.at(0);
          return assignable::gen(n, assignable, result);
        }
      }
    }
  } // }}}

  void instruction (const node & n, liveness::result & result) {
    using namespace L2::grammar::instruction;

    /* std::cout << "gen/kill on: " << n.name() << "\n"; */

    if (n.is<instruction::any>()) {
      assert(n.children.size() == 1);
      const node & actual_instruction = *n.children.at(0);
      return gen_kill::instruction(actual_instruction, result);
    }

    if (n.is<assign::assignable::gets_movable>()) {
      assert(n.children.size() == 2);
      const node & dest = *n.children.at(0);
      const node & src  = *n.children.at(1);
      helper::operand::movable::gen(n, src, result);
      helper::operand::assignable::kill(n, dest, result);
      return;
    }

    if (n.is<assign::assignable::gets_relative>()) {
      assert(n.children.size() == 2);
      const node & dest = *n.children.at(0);
      const node & src  = *n.children.at(1);
      helper::operand::relative::gen(n, src, result);
      helper::operand::kill(n, dest, result);
      return;
    }

    if (n.is<assign::relative::gets_movable>()) {
      assert(n.children.size() == 2);
      const node & dest = *n.children.at(0);
      const node & src  = *n.children.at(1);
      helper::operand::movable::gen(n, src, result);
      helper::operand::relative::kill(n, dest, result);
      return;
    }

    if (n.is<update::assignable::arithmetic::comparable>()) {
      assert(n.children.size() == 3);
      const node & dest = *n.children.at(0);
      /* const node & op   = *n.children.at(1); */
      const node & src  = *n.children.at(2);
      helper::operand::comparable::gen(n, src, result);
      helper::operand::assignable::gen(n, dest, result);
      helper::operand::assignable::kill(n, dest, result);
      return;
    }

    if (n.is<update::assignable::shift::shift>()) {
      assert(n.children.size() == 3);
      const node & dest = *n.children.at(0);
      /* const node & op   = *n.children.at(1); */
      const node & src  = *n.children.at(2);
      /* auto src8 = helper::register_to_lower8(src); */
      helper::operand::shift::gen(n, src, result);
      helper::operand::assignable::gen(n, dest, result);
      helper::operand::assignable::kill(n, dest, result);
      return;
    }

    if (n.is<update::assignable::shift::number>()) {
      assert(n.children.size() == 3);
      const node & dest = *n.children.at(0);
      /* const node & op   = *n.children.at(1); */
      /* const node & num  = *n.children.at(2); */
      helper::operand::assignable::gen(n, dest, result);
      helper::operand::assignable::kill(n, dest, result);
      return;
    }

    if (false
      || n.is<update::relative::arithmetic::add_comparable>()
      || n.is<update::relative::arithmetic::subtract_comparable>()
    ) {
      assert(n.children.size() == 3);
      const node & dest = *n.children.at(0);
      /* const node & op   = *n.children.at(1); */
      const node & src  = *n.children.at(2);
      helper::operand::comparable::gen(n, src, result);
      helper::operand::relative::kill(n, dest, result);
      return;
    }

    if (false
      || n.is<update::assignable::arithmetic::add_relative>()
      || n.is<update::assignable::arithmetic::subtract_relative>() 
    ) {
      assert(n.children.size() == 3);
      const node & dest = *n.children.at(0);
      /* const node & op   = *n.children.at(1); */
      const node & src  = *n.children.at(2);
      helper::operand::relative::gen(n, src, result);
      helper::operand::assignable::gen(n, dest, result);
      helper::operand::assignable::kill(n, dest, result);
      return;
    }

    if (n.is<assign::assignable::gets_comparison>()) {
      /* namespace predicate = helper::cmp::predicate; */
      assert(n.children.size() == 2);
      const node & dest = *n.children.at(0);
      const node & cmp  = *n.children.at(1);
      assert(cmp.children.size() == 3);
      const node & lhs  = *cmp.children.at(0);
      /* const node & op   = *cmp.children.at(1); */
      const node & rhs  = *cmp.children.at(2);
      helper::operand::comparable::gen(n, lhs, result);
      helper::operand::comparable::gen(n, rhs, result);
      helper::operand::assignable::kill(n, dest, result);
      return;
    }

    if (n.is<jump::cjump::if_else>()) {
      /* namespace predicate = helper::cmp::predicate; */
      assert(n.children.size() == 3);
      const node & cmp  = *n.children.at(0);
      assert(cmp.children.size() == 3);
      const node & lhs  = *cmp.children.at(0);
      /* const node & op   = *cmp.children.at(1); */
      const node & rhs  = *cmp.children.at(2);
      /* const node & then = *n.children.at(1); */
      /* const node & els  = *n.children.at(2); */
      helper::operand::comparable::gen(n, lhs, result);
      helper::operand::comparable::gen(n, rhs, result);
      return;
    }

    if (n.is<jump::cjump::when>()) {
      /* namespace predicate = helper::cmp::predicate; */
      assert(n.children.size() == 2);
      const node & cmp  = *n.children.at(0);
      assert(cmp.children.size() == 3);
      const node & lhs  = *cmp.children.at(0);
      /* const node & op   = *cmp.children.at(1); */
      const node & rhs  = *cmp.children.at(2);
      /* const node & then = *n.children.at(1); */
      helper::operand::comparable::gen(n, lhs, result);
      helper::operand::comparable::gen(n, rhs, result);
      return;
    }

    if (n.is<define::label>()) {
      assert(n.children.size() == 1);
      /* const node & label = *n.children.at(0); */
      /* liveness::label(label, os); */
      return;
    }

    if (n.is<jump::go2>()) {
      assert(n.children.size() == 1);
      /* const node & label = *n.children.at(0); */
      /* liveness::label(label, os); */
      return;
    }

    if (n.is<invoke::ret>()) {
      /* int stack = 8 * locals; */
      /* if (args  > 6) stack += 8 * (args - 6); */
      /* if (stack > 0) { */
      /* } */
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
      // NOTE(jordan: already added rax above.
      /* helper::kill<caller::rax>(n, result); */
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

    if (n.is<update::assignable::arithmetic::increment>()) {
      assert(n.children.size() == 2); // ignore '++'
      const node & dest = *n.children.at(0);
      helper::operand::assignable::gen(n, dest, result);
      helper::operand::assignable::kill(n, dest, result);
      return;
    }

    if (n.is<update::assignable::arithmetic::decrement>()) {
      assert(n.children.size() == 2); // ignore '--'
      const node & dest = *n.children.at(0);
      helper::operand::assignable::gen(n, dest, result);
      helper::operand::assignable::kill(n, dest, result);
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

    if (n.is<assign::assignable::gets_stack_arg>()) {
      assert(n.children.size() == 2);
      const node & dest = *n.children.at(0);
      /* const node & stack_arg = *n.children.at(1); */
      helper::operand::assignable::kill(n, dest, result);
      return;
    }

    std::cout << "something went wrong at\n\t" << n.name() << "\n";
    assert(false && "liveness::instruction: unreachable!");
  }

  /* void instructions (const node & n, liveness::result & result) { */
  /*   for (auto & child : n.children) { */
  /*     assert(child->is<grammar::instruction::any>() */
  /*         && "instructions: got non-instruction!"); */
  /*     gen_kill::instruction(*child, result); */
  /*   } */
  /*   return; */
  /* } */

  /* void function (const node & n, liveness::result & result) { */
  /*   assert(n.children.size() == 4); */
  /*   // NOTE(jordan): ignore name, arg_count, and local_count */
  /*   /1* const node & name         = *n.children.at(0); *1/ */
  /*   /1* const node & arg_count    = *n.children.at(1); *1/ */
  /*   /1* const node & local_count  = *n.children.at(2); *1/ */
  /*   const node & instructions = *n.children.at(3); */
  /*   return gen_kill::instructions(instructions, result); */
  /* } */

  /* void functions (const node & n, liveness::result & result) { */
  /*   for (auto & child : n.children) { */
  /*     assert(child->is<grammar::function::define>() */
  /*         && "functions: got non-function!"); */
  /*     gen_kill::function(*child, result); */
  /*   } */
  /*   return; */
  /* } */

  /* void program (const node & n, liveness::result & result) { */
  /*   assert(n.children.size() == 2); */
  /*   assert(n.is<grammar::program::define>() && "top is not a program!"); */
  /*   // NOTE(jordan): ignore entry */
  /*   /1* const node & entry     = *n.children.at(0); *1/ */
  /*   const node & functions = *n.children.at(1); */
  /*   return gen_kill::functions(functions, result); */
  /* } */

  /* void root (const node & n, liveness::result & result) { */
  /*   assert(n.children.size() == 1); */
  /*   return gen_kill::program(*n.children.at(0), result); */
  /* } */
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

  // NOTE(jordan): import helpers from outer scope.
  namespace helper { using namespace liveness::helper; }

  namespace helper {
    void set (const node & n, const node & s, liveness::result & result) {
      result.successor[&n].insert(&s);
    }
    const node & definition_for (
      const node & label,
      std::vector<std::shared_ptr<const node>> instructions
    ) {
      assert(label.is<identifier::label>()
          && "definition_for: called on a non-label!");
      for (auto & instruction_ptr : instructions) {
        const node & instruction = *instruction_ptr;
        if (instruction.is<instruction::define::label>()) {
          const node & defined_label = *instruction.children.at(0);
          assert(defined_label.is<identifier::label>());
          if (defined_label.content() == label.content())
            return instruction;
        }
      }
      assert(false && "definition_for: could not find label!");
    }
  }

  void instruction (
    const node & n,
    std::vector<std::shared_ptr<const node>> siblings,
    int index,
    liveness::result & result
  ) {
    using namespace L2::grammar::instruction;

    // {{{ assertions
    if (false
      || n.is<define::label>()
      || n.is<jump::go2>()
    ) {
      assert(n.children.size() == 1);
    }
    if (false
      || n.is<assign::assignable::gets_movable>()
      || n.is<assign::assignable::gets_relative>()
      || n.is<assign::relative::gets_movable>()
      || n.is<assign::assignable::gets_comparison>()
      || n.is<jump::cjump::when>()
      || n.is<update::assignable::arithmetic::increment>()
      || n.is<update::assignable::arithmetic::decrement>()
    ) {
      assert(n.children.size() == 2);
    }
    if (false
      || n.is<update::assignable::arithmetic::comparable>()
      || n.is<update::assignable::shift::shift>()
      || n.is<update::assignable::shift::number>()
      || n.is<update::relative::arithmetic::add_comparable>()
      || n.is<update::relative::arithmetic::subtract_comparable>()
      || n.is<update::assignable::arithmetic::add_relative>()
      || n.is<update::assignable::arithmetic::subtract_relative>()
      || n.is<jump::cjump::if_else>()
    ) {
      assert(n.children.size() == 3);
    }
    if (false
      || n.is<assign::assignable::gets_address>()
    ) {
      assert(n.children.size() == 5);
    }

    if (false
      || n.is<assign::assignable::gets_comparison>()
    ) {
      const node & cmp  = *n.children.at(1);
      assert(cmp.children.size() == 3);
    }

    if (false
      || n.is<jump::cjump::if_else>()
      || n.is<jump::cjump::when>()
    ) {
      const node & cmp  = *n.children.at(0);
      assert(cmp.children.size() == 3);
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
    }

    if (n.is<invoke::call::callable>()) {
      const node & callable = *n.children.at(0);
      assert(callable.children.size() == 1);
      const node & value = *callable.children.at(0);
      assert(false
          || value.is<grammar::operand::assignable>()
          || value.is<identifier::label>()
          && "value must be either assignable or label");
    }
    // }}}

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
    assert(false && "liveness::instruction: unreachable!");
  }

/*   void instructions (const node & n, liveness::result & result) { */
/*     // NOTE(jordan): use an iterator so we can look ahead */
/*     for (int index = 0; index < n.children.size(); index++) { */
/*       const node & child = *n.children.at(index); */
/*       assert(child.is<grammar::instruction::any>() */
/*           && "instructions: got non-instruction!"); */
/*       successor::instruction(child, n.children, index, result); */
/*     } */
/*     return; */
/*   } */

/*   void function (const node & n, liveness::result & result) { */
/*     assert(n.children.size() == 4); */
/*     // NOTE(jordan): ignore name, arg_count, and local_count */
/*     /1* const node & name         = *n.children.at(0); *1/ */
/*     /1* const node & arg_count    = *n.children.at(1); *1/ */
/*     /1* const node & local_count  = *n.children.at(2); *1/ */
/*     const node & instructions = *n.children.at(3); */
/*     return successor::instructions(instructions, result); */
/*   } */

/*   void functions (const node & n, liveness::result & result) { */
/*     for (auto & child : n.children) { */
/*       assert(child->is<grammar::function::define>() */
/*           && "functions: got non-function!"); */
/*       successor::function(*child, result); */
/*     } */
/*     return; */
/*   } */

/*   void program (const node & n, liveness::result & result) { */
/*     assert(n.children.size() == 2); */
/*     assert(n.is<grammar::program::define>() && "top is not a program!"); */
/*     // NOTE(jordan): ignore entry */
/*     /1* const node & entry     = *n.children.at(0); *1/ */
/*     const node & functions = *n.children.at(1); */
/*     return successor::functions(functions, result); */
/*   } */

/*   void root (const node & n, liveness::result & result) { */
/*     assert(n.children.size() == 1); */
/*     return successor::program(*n.children.at(0), result); */
/*   } */
} // }}}

/**
 *
 * IN[i]  = GEN[i] U (OUT[i] - KILL[i])
 * OUT[i] = U(s : successor of(i)) IN[s]
 *
 */
namespace L2::analysis::ast::liveness {
  /* std::vector<std::shared_ptr<const node>> collect_instructions (const node & n) { // {{{ */
  /*   assert(n.is_root()); */

  /*   const node & program = *n.children.at(0); */
  /*   assert(program.is<L2::grammar::program::define>()); */
  /*   assert(program.children.size() == 2); */

  /*   const node & functions = *program.children.at(1); */
  /*   assert(functions.is<L2::grammar::program::functions>()); */
  /*   assert(functions.children.size() > 1); */

  /*   std::vector<std::shared_ptr<const node>> result; */
  /*   for (auto & function : functions.children) { */
  /*     assert(function->is<L2::grammar::function::define>()); */
  /*     assert(function->children.size() == 4); */
  /*     const node & instructions = *function->children.at(3); */
  /*     for (auto & instruction : instructions.children) { */
  /*       assert(instruction->is<L2::grammar::instruction::any>()); */
  /*       assert(instruction->children.size() == 1); */
  /*       auto shared_instruction = std::shared_ptr<const node>( */
  /*         std::move(instruction->children.at(0)) */
  /*       ); */
  /*       result.push_back(shared_instruction); */
  /*     } */
  /*   } */

  /*   return result; */
  /* } // }}} */

  std::vector<std::shared_ptr<const node>> collect_instructions (const node & function) { // {{{
    std::vector<std::shared_ptr<const node>> result;
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
  } // }}}

  /* FIXME(jordan): this code is kinda gross. Refactor? I honestly
   * suspect the code would be cleaner if we didn't use std-library set
   * operations and just iterated over things.
   */
  void in_out (std::vector<std::shared_ptr<const node>> instructions, result & result) {
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

  void root (const node & root, liveness::result & result, bool print = false) {
    // 1. Compute GEN, KILL
    std::vector<std::shared_ptr<const node>> instructions
      = collect_instructions(*root.children.at(0));

    for (int index = 0; index < instructions.size(); index++) {
      const node & instruction = *instructions.at(index);
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

    for (int index = 0; index < instructions.size(); index++) {
      const node & instruction = *instructions.at(index);
      analysis::ast::successor::instruction(
        instruction,
        instructions,
        index,
        result);
      /* { */
      /*   std::cout << "succ[" << index << "] = "; */
      /*   for (auto successor : result.successor[&instruction]) { */
      /*     std::cout << successor->name(); */
      /*   } */
      /*   std::cout << "\n"; */
      /* } */
    }
    /* std::cout << "\n"; */

    in_out(instructions, result);

    #define pretty false
    if (print) {
      std::cout << (pretty ? "((in\n" : "(\n(in");
      for (int index = 0; index < instructions.size(); index++) {
        const node & instruction = *instructions.at(index);
        auto in = result.in[&instruction];
        std::cout << (pretty ? "  (" : "\n(");
        for (auto var : in) std::cout << var << " ";
        std::cout << (pretty ? ")\n" : ")");
      }
      //backspace thru: " )\n"
      std::cout << (pretty ? "\b\b\b))\n" : "\n)");
      std::cout << (pretty ? "(out\n"     : "\n\n(out");
      for (int index = 0; index < instructions.size(); index++) {
        const node & instruction = *instructions.at(index);
        auto out = result.out[&instruction];
        std::cout << (pretty ? "  (" : "\n(");
        for (auto var : out) std::cout << var << " ";
        std::cout << (pretty ? ")\n" : ")");
      }
      //backspace thru: " )\n"
      std::cout << (pretty ? "\b\b\b))\n" : "\n)\n\n)\n");
    }

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

namespace L2::analysis {
  using liveness_result = ast::liveness::result;
  liveness_result liveness (const parse_tree::node & root, bool print = false) {
    assert(root.is_root() && "generate: got a non-root node!");
    assert(!root.children.empty() && "generate: got an empty AST!");

    liveness_result result = {};
    ast::liveness::root(root, result, print);
    return result;
  }
}
