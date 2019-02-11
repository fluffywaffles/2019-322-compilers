#pragma once
#include <set>
#include <typeindex>

#include "L1/codegen.h"
#include "grammar.h"
#include "ast.h"

namespace helper::L2 { // {{{
  namespace L1_helper = codegen::L1::generate::helper;
  namespace grammar = grammar::L2;
  using node = ast::L2::node;

  using nodes = std::vector<std::shared_ptr<const node>>;

  template <class R>
  bool matches (const node & n)  { return L1_helper::matches<R>(n); }
  template <class R>
  bool matches (const std::string & s) { return L1_helper::matches<R>(s); }

  int integer (const node & n) {
    assert(n.has_content() && "helper::integer: no content!");
    assert(
      matches<grammar::literal::number::integer::any>(n)
      && "helper::integer: does not match literal::number::integer!"
    );
    return std::stoi(n.content());
  }

  const node & unwrap_assert (const node & parent) {
    assert(
      parent.children.size() == 1
      && "helper::unwrap_assert: not exactly 1 child in parent!"
    );
    return *parent.children.at(0);
  }

  nodes collect_instructions (const node & function) {
    nodes result;
    assert(function.is<grammar::function::define>());
    assert(function.children.size() == 4);
    const node & instructions = *function.children.at(3);
    for (auto & instruction_wrapper : instructions.children) {
      assert(instruction_wrapper->is<grammar::instruction::any>());
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

  // NOTE(jordan): woof. Template specialization, amirite?
  namespace x86_64_register {
    namespace reg = grammar::identifier::x86_64_register;
    template <typename Register> struct convert {
      const static std::type_index index;
      const static std::string string;
    };
    #define mkreg2s(R) \
      template<> const std::string convert<reg::R>::string = #R
    mkreg2s(rax); mkreg2s(rbx); mkreg2s(rcx); mkreg2s(rdx); mkreg2s(rsi);
    mkreg2s(rdi); mkreg2s(rbp); mkreg2s(rsp); mkreg2s(r8 ); mkreg2s(r9 );
    mkreg2s(r10); mkreg2s(r11); mkreg2s(r12); mkreg2s(r13); mkreg2s(r14);
    mkreg2s(r15);
    #undef mkreg2s
    #define mkreg2i(R)                                                   \
      template<> const std::type_index                                   \
      convert<reg::R>::index = std::type_index(typeid(reg::R))
    mkreg2i(rax); mkreg2i(rbx); mkreg2i(rcx); mkreg2i(rdx); mkreg2i(rsi);
    mkreg2i(rdi); mkreg2i(rbp); mkreg2i(rsp); mkreg2i(r8 ); mkreg2i(r9 );
    mkreg2i(r10); mkreg2i(r11); mkreg2i(r12); mkreg2i(r13); mkreg2i(r14);
    mkreg2i(r15);
    #undef mkreg2i
    #define r2i(R) convert<reg::R>::index
    std::set<std::type_index> all_register_indices = {
      r2i(rax), r2i(rbx), r2i(rcx), r2i(rdx), r2i(rsi), r2i(rdi),
      r2i(rbp), r2i(rsp), r2i(r8 ), r2i(r9 ), r2i(r10), r2i(r11),
      r2i(r12), r2i(r13), r2i(r14), r2i(r15),
    };
    #undef r2i
    #define ri2s(R) \
      if (convert<reg::R>::index == reg) return convert<reg::R>::string
    std::string index_to_string (std::type_index reg) {
      ri2s(rax); ri2s(rbx); ri2s(rcx); ri2s(rdx); ri2s(rsi); ri2s(rdi);
      ri2s(rbp); ri2s(rsp); ri2s(r8 ); ri2s(r9 ); ri2s(r10); ri2s(r11);
      ri2s(r12); ri2s(r13); ri2s(r14); ri2s(r15);
      assert(false && "register_index_to_string: unrecognized index!");
    }
    #undef ri2s
  }

  namespace variable {
    std::string get_name (const node & variable) {
      assert(variable.children.size() == 1 && "missing child!");
      const node & name = *variable.children.at(0);
      assert(
        name.is<grammar::identifier::name>()
        && "child is not a 'name'!"
      );
      return name.content();
    }
  }

  // NOTE(jordan): collect_variables is kinda gross. Idk any better way.
  void collect_variables (
    const node & start,
    std::set<std::string> & variables
  ) {
    if (start.is<grammar::identifier::variable>()) {
      variables.insert(variable::get_name(start));
    } else {
      for (auto & child : start.children) {
        collect_variables(*child, variables);
      }
    }
  }

  std::set<std::string> collect_variables (nodes instructions) {
    std::set<std::string> variables;
    for (auto & instruction_ptr : instructions) {
      for (auto & child : instruction_ptr->children) {
        collect_variables(*child, variables);
      }
    }
    return variables;
  }

  const node & definition_for (
    const node & label,
    nodes instructions
  ) {
    assert(
      label.is<grammar::operand::label>()
      && "definition_for: called on a non-label!"
    );
    for (auto & instruction_ptr : instructions) {
      const node & instruction = *instruction_ptr;
      if (instruction.is<grammar::instruction::define::label>()) {
        const node & defined_label = *instruction.children.at(0);
        assert(defined_label.is<grammar::operand::label>());
        if (defined_label.content() == label.content())
          return instruction;
      }
    }
    assert(false && "definition_for: could not find label!");
  }
}
