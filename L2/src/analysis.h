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
#include "ast.h"

namespace analysis::L2 {
  namespace ast     = ast::L2;
  namespace grammar = grammar::L2;
  using namespace ast;
  using namespace grammar;

  // debug constants {{{
  const int DBG_SUCC                       = 0b000001;
  const int DBG_PRINT                      = 0b000010;
  const int DBG_GEN_KILL                   = 0b000100;
  const int DBG_IN_OUT_LOOP_CND            = 0b001000;
  const int DBG_IN_OUT_LOOP_ORIG           = 0b010000;
  const int DBG_IN_OUT_LOOP_OUT_MINUS_KILL = 0b100000;
  // }}}

  using string = std::string;
  // TODO(jordan): make this a set of Register instead of std::string
  using liveness_map     = std::map<const node *, std::set<string>>;
  using successor_map    = std::map<const node *, std::set<const node *>>;
  // TODO(jordan): make this use a Register type instead of std::string
  using interference_map = std::map<const string, std::set<string>>;
  using nodes = std::vector<std::shared_ptr<const node>>;

  namespace helper { // {{{
    namespace L1_helper = codegen::L1::generate::helper;
    template <class R>
    bool matches (const node & n)  { return L1_helper::matches<R>(n); }
    template <class R>
    bool matches (const string & s) { return L1_helper::matches<R>(s); }
  }

  namespace helper {
    int integer (const node & n) {
      assert(n.has_content() && "helper::integer: no content!");
      assert(
        matches<literal::number::integer::any>(n)
        && "helper::integer: does not match literal::number::integer!"
      );
      return std::stoi(n.content());
    }
  }

  namespace helper {
    const node & unwrap_assert (const node & parent) {
      assert(
        parent.children.size() == 1
        && "helper::unwrap_assert: not exactly 1 child in parent!"
      );
      return *parent.children.at(0);
    }
  }

  namespace helper {
    nodes collect_instructions (const node & function) {
      nodes result;
      assert(function.is<function::define>());
      assert(function.children.size() == 4);
      const node & instructions = *function.children.at(3);
      for (auto & instruction_wrapper : instructions.children) {
        assert(instruction_wrapper->is<instruction::any>());
        assert(instruction_wrapper->children.size() == 1);
        /* FIXME(jordan): this is only ok because we stop trying to look
         * at the children of our functions (our instructions) after we've
         * collected them. If we did try to look at the children of our
         * functions, they'd be gone: we `std::move`d them.
         */
        const auto shared_instruction = std::shared_ptr<const node>(
          std::move(instruction_wrapper->children.at(0))
        );
        result.push_back(shared_instruction);
      }
      return result;
    }
  }

  // NOTE(jordan): woof. Template specialization, amirite?
  namespace helper::x86_64_register {
    namespace reg = identifier::x86_64_register;
    template <typename Register> struct as_string {
      const static std::string value;
    };
    #define mkreg2s(R) \
      template<> const std::string as_string<reg::R>::value = #R;
    mkreg2s(rax) mkreg2s(rbx) mkreg2s(rcx) mkreg2s(rdx) mkreg2s(rsi)
    mkreg2s(rdi) mkreg2s(rbp) mkreg2s(rsp) mkreg2s(r8 ) mkreg2s(r9 )
    mkreg2s(r10) mkreg2s(r11) mkreg2s(r12) mkreg2s(r13) mkreg2s(r14)
    mkreg2s(r15)
    #undef mkreg2s
  }

  namespace helper::variable {
    std::string get_name (const node & variable) {
      assert(variable.children.size() == 1 && "missing child!");
      const node & name = *variable.children.at(0);
      assert(name.is<identifier::name>() && "child is not a 'name'!");
      return name.content();
    }

    void collect_variables (
      const node & start,
      std::vector<std::string> & variables
    ) {
      if (start.is<identifier::variable>()) {
        variables.push_back(helper::variable::get_name(start));
      } else {
        for (auto & child : start.children) {
          collect_variables(*child, variables);
        }
      }
    }

    std::vector<std::string> collect_variables (nodes instructions) {
      std::vector<std::string> variables;
      for (auto & instruction_ptr : instructions) {
        for (auto & child : instruction_ptr->children) {
          collect_variables(*child, variables);
        }
      }
      return variables;
    }
  }
  // }}}
}

// liveness {{{
namespace analysis::L2::liveness {
  struct result {
    nodes instructions;
    liveness_map in;
    liveness_map out;
    liveness_map gen;
    liveness_map kill;
    successor_map successor;
  };

  namespace helper { using namespace L2::helper; }
}

/**
 *
 * GEN[i]  = { <read variables> }
 * KILL[i] = { <defined variables> }
 *
 */
namespace analysis::L2::liveness::gen_kill { // {{{
  // NOTE(jordan): import helpers from outer scope.
  namespace helper { using namespace liveness::helper; }

  namespace helper { // {{{
    enum struct GenKill { gen, kill };
    struct gen_kill {
      static const bool DBG = false;
      static void generic (
        GenKill choice,
        const node & i,
        const node & v,
        result & result
      ) {
        if (DBG) {
          std::cout << "gen/kill: inst : " << i.name() << "\n";
          std::cout << "gen/kill: var  : " << v.name()
            << " (" << (v.has_content() ? v.content() : "") << ")\n";
        }
        bool is_variable = v.is<identifier::variable>();
        assert(
          is_variable || matches<register_set::any>(v)
          && "gen/kill: node is not register or variable!"
        );
        if (matches<identifier::x86_64_register::rsp>(v)) {
          if (DBG) std::cout << "gen/kill: ignoring rsp.\n";
          return;
        }
        std::string name = is_variable
          ? helper::variable::get_name(v)
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
        namespace register_helper = helper::x86_64_register;
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
      static void gen  (const node & i, liveness::result & result) {
        return generic<Reg>(GenKill::gen, i, result);
      }
      template <typename Reg>
      static void kill (const node & i, liveness::result & result) {
        return generic<Reg>(GenKill::kill, i, result);
      }
    };
  }
  // }}}

  // helper::operand {{{
  namespace helper::operand {
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
        using gen_kill = helper::gen_kill;
        const node & value = Operand::unwrap(v);
        if (accept(value)) {
          if (GenKill::gen  == choice) gen_kill::gen(n, value, result);
          if (GenKill::kill == choice) gen_kill::kill(n, value, result);
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
        const node & child = helper::unwrap_assert(parent);
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
        return helper::unwrap_assert(parent);
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
        return !helper::matches<number>(v);
      }
    };
    // NOTE(jordan): "Variable-or-Value" operands
    struct movable : base<movable>, may_wrap<movable, memory> {
      using rule = grammar::operand::movable;
      static bool accept (const node & v) {
        using namespace grammar::operand;
        return !v.is<label>() && !helper::matches<number>(v);
      }
    };
    struct comparable : base<comparable>, may_wrap<comparable, memory> {
      using rule = grammar::operand::comparable;
      static bool accept (const node & v) {
        using namespace grammar::operand;
        return !helper::matches<number>(v);
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

  void instruction (const node & n, liveness::result & result) {
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
      using gen_kill = helper::gen_kill;
      gen_kill::gen<identifier::x86_64_register::rax>(n, result);
      namespace callee = register_group::callee_save;
      gen_kill::gen<callee::r12>(n, result);
      gen_kill::gen<callee::r13>(n, result);
      gen_kill::gen<callee::r14>(n, result);
      gen_kill::gen<callee::r15>(n, result);
      gen_kill::gen<callee::rbp>(n, result);
      gen_kill::gen<callee::rbx>(n, result);
      return;
    }

    if (false
      || n.is<invoke::call::callable>()
      || n.is<invoke::call::intrinsic::print>()
      || n.is<invoke::call::intrinsic::allocate>()
      || n.is<invoke::call::intrinsic::array_error>()
    ) {
      using gen_kill = helper::gen_kill;
      assert(n.children.size() == 2);
      const node & integer  = *n.children.at(1);
      int args  = helper::integer(integer);
      if (args > 0) {
        namespace arg = register_group::argument;
        if (args >= 1) gen_kill::gen<arg::rdi>(n, result);
        if (args >= 2) gen_kill::gen<arg::rsi>(n, result);
        if (args >= 3) gen_kill::gen<arg::rdx>(n, result);
        if (args >= 4) gen_kill::gen<arg::rcx>(n, result);
        if (args >= 5) gen_kill::gen<arg::r8 >(n, result);
        if (args >= 6) gen_kill::gen<arg::r9 >(n, result);
      }
      gen_kill::kill<identifier::x86_64_register::rax>(n, result);
      namespace caller = register_group::caller_save;
      gen_kill::kill<caller::r8 >(n, result);
      gen_kill::kill<caller::r9 >(n, result);
      gen_kill::kill<caller::r10>(n, result);
      gen_kill::kill<caller::r11>(n, result);
      gen_kill::kill<caller::rax>(n, result);
      gen_kill::kill<caller::rcx>(n, result);
      gen_kill::kill<caller::rdi>(n, result);
      gen_kill::kill<caller::rdx>(n, result);
      gen_kill::kill<caller::rsi>(n, result);
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
  // NOTE(jordan): import helpers from outer scope.
  namespace helper { using namespace liveness::helper; }

  namespace helper { // {{{
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
  } // }}}

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
} // }}}

/**
 *
 * IN[i]  = GEN[i] U (OUT[i] - KILL[i])
 * OUT[i] = U(s : successor of(i)) IN[s]
 *
 */
// liveness::in_out {{{
namespace analysis::L2::liveness {
  void in_out (result & result, unsigned debug) {
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
  void compute (liveness::result & result, unsigned debug) {
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
  result compute (const ast::node & root, unsigned debug = 0) {
    assert(root.is_root() && "liveness: got a non-root node!");
    assert(!root.children.empty() && "liveness: got an empty AST!");
    liveness::result result = {};
    const node & function = *root.children.at(0);
    result.instructions = helper::collect_instructions(function);
    liveness::compute(result, debug);
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

  namespace helper { using namespace L2::helper; }

  namespace helper {
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
  namespace helper::x86_64_register { // {{{
    namespace register_helper = ::analysis::L2::helper::x86_64_register;
    using namespace grammar::identifier::x86_64_register;

    void connect_to_all (result & result, const std::string & origin) {
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
    void connect_to_all (result & result) {
      connect_to_all(result, register_helper::as_string<Register>::value);
    }

    // FIXME(jordan): never a sin without its just reward...
    void connect_all_registers (result & result) {
      connect_to_all<rax>(result);
      connect_to_all<rbx>(result);
      connect_to_all<rcx>(result);
      connect_to_all<rdx>(result);
      connect_to_all<rsi>(result);
      connect_to_all<rdi>(result);
      connect_to_all<rbp>(result);
      connect_to_all<r8 >(result);
      connect_to_all<r9 >(result);
      connect_to_all<r10>(result);
      connect_to_all<r11>(result);
      connect_to_all<r12>(result);
      connect_to_all<r13>(result);
      connect_to_all<r14>(result);
      connect_to_all<r15>(result);
    }
  } // }}}
}

namespace analysis::L2::interference { // {{{
  void compute (
    interference::result & result,
    unsigned debug = 0 // TODO(jordan): add debug flags for interference
  ) {
    namespace assign = grammar::instruction::assign;
    namespace update = grammar::instruction::update;
    // 0. Connect all registers to one another. Don't look closely.
    helper::x86_64_register::connect_all_registers(result);
    // Iterate over all of the instructions, and...
    for (int index = 0; index < result.instructions.size(); index++) {
      const auto & instruction = *result.instructions.at(index);
      const auto in   = result.liveness.in[&instruction];
      const auto out  = result.liveness.out[&instruction];
      const auto kill = result.liveness.kill[&instruction];
      // 1. Connect each pair of variables in the same IN set
      for (const auto & variable : in) {
        for (const auto & sibling : in) {
          helper::biconnect(result, variable, sibling);
        }
      }
      // 2. Connect each pair of variables in the same OUT set
      // 3. Connect everything in KILL with OUT; except x <- y (both vars)
      // Check if we should connect the kill set.
      bool connect_kill = true;
      if (instruction.is<assign::assignable::gets_movable>()) {
        assert(instruction.children.size() == 2);
        const node & src = *instruction.children.at(1);
        if (helper::matches<operand::memory>(src)) {
          // This is a variable 'gets' a variable or register.
          connect_kill = false;
        }
      }
      // Make the connections.
      for (const auto & variable : out) {
        for (const auto & sibling : out) {
          helper::biconnect(result, variable, sibling);
        }
        if (connect_kill) {
          for (const auto & k : kill) {
            helper::biconnect(result, variable, k);
          }
        }
      }
      if (connect_kill) for (const auto & variable : kill) {
        for (const auto & liveout : out) {
          helper::biconnect(result, variable, liveout);
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
        using shift = liveness::gen_kill::helper::operand::shift;
        const node & value = shift::unwrap(src);
        bool is_variable = value.is<identifier::variable>();
        const std::string & variable
          = is_variable
          ? helper::variable::get_name(value)
          : value.content();
        // FIXME(jordan): yeeuuup. This is horrible, I know. But... ugh.
        namespace register_help = L2::helper::x86_64_register;
        using namespace grammar::identifier::x86_64_register;
        using namespace helper;
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
  result compute (const ast::node & root, unsigned debug = 0) {
    assert(root.is_root() && "interference: got a non-root node!");
    assert(!root.children.empty() && "interference: got an empty AST!");
    liveness::result liveness_result = liveness::compute(root, debug);
    std::vector<std::string> variables
      = helper::variable::collect_variables(liveness_result.instructions);
    interference::result result = {
      liveness_result.instructions,
      variables,
      liveness_result
    };
    // NOTE(jordan): make sure every variable has SOME kind of entry.
    for (auto variable : result.variables) {
      result.graph[variable] = {};
    }
    interference::compute(result, debug);
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
