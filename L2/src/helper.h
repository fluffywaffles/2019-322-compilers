#pragma once
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
    std::vector<std::string> & variables
  ) {
    if (start.is<grammar::identifier::variable>()) {
      variables.push_back(variable::get_name(start));
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
