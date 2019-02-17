#pragma once
#include <set>
#include <typeindex>
#include <algorithm>

#include "grammar.h"
#include "ast.h"

namespace helper::L2 { // {{{
  namespace grammar = grammar::L2;
  using node = ast::L2::node;

  using up_node  = std::unique_ptr<node>;
  using up_nodes = std::vector<up_node>;

  /* NOTE(jordan): nothing about this helper is L1-specific due to
   * templating; should move it into a common utils?
   */
  template <typename Rule>
  bool matches (const std::string string) {
    // WAFKQUSKWLZAQWAAAA YES I AM THE T<EMPL>ATE RELEASER OF Z<ALGO>
    using namespace tao::pegtl;
    memory_input<> string_input (string, "");
    return normal<Rule>::template
      match<apply_mode::NOTHING, rewind_mode::DONTCARE, nothing, normal>
      (string_input);
  }
  template <typename Rule>
  bool matches (const node & n) {
    assert(n.has_content() && "matches: must have content!");
    const std::string & content = n.content();
    return matches<Rule>(content);
  }

  template <typename Colln>
  bool set_equal (Colln const & a, Colln const & b) {
    return std::equal(a.begin(), a.end(), b.begin(), b.end());
  }

  template <typename Colln>
  void set_union (Colln const & a, Colln const & b, Colln & dest) {
    // NOTE(jordan): return dest.end(). Don't care; discard.
    std::set_union(
      a.begin(), a.end(),
      b.begin(), b.end(),
      std::inserter(dest, dest.begin())
    );
    return;
  }

  template <typename Colln>
  void set_difference (Colln const & a, Colln const & b, Colln & dest) {
    // NOTE(jordan): return dest.end(). Don't care; discard.
    std::set_difference(
      a.begin(), a.end(),
      b.begin(), b.end(),
      std::inserter(dest, dest.begin())
    );
    return;
  }

  int integer (node const & n) {
    assert(n.has_content() && "helper::integer: no content!");
    assert(
      matches<grammar::literal::number::integer::any>(n)
      && "helper::integer: does not match literal::number::integer!"
    );
    return std::stoi(n.content());
  }

  node const & unwrap_assert (node const & parent) {
    assert(
      parent.children.size() == 1
      && "helper::unwrap_assert: not exactly 1 child in parent!"
    );
    return *parent.children.at(0);
  }

  // NOTE(jordan): woof. Template specialization, amirite?
  namespace x86_64_register {
    namespace reg = grammar::identifier::x86_64_register;
    template <typename Register> struct convert {
      static const std::type_index index;
      static const std::string string;
    };
    // REFACTOR(jordan): look at that register call pattern...
    #define mkreg2s(R) \
      template<> std::string const convert<reg::R>::string = #R
    mkreg2s(rax); mkreg2s(rbx); mkreg2s(rcx); mkreg2s(rdx); mkreg2s(rsi);
    mkreg2s(rdi); mkreg2s(rbp); mkreg2s(rsp); mkreg2s(r8 ); mkreg2s(r9 );
    mkreg2s(r10); mkreg2s(r11); mkreg2s(r12); mkreg2s(r13); mkreg2s(r14);
    mkreg2s(r15);
    #undef mkreg2s
    #define mkreg2i(R)                                                   \
      template<> std::type_index const                                   \
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
    std::set<std::string> analyzable_registers () {
      std::set<std::string> result;
      for (auto & index : all_register_indices) {
        std::string const reg = index_to_string(index);
        if (!matches<grammar::register_set::unanalyzable>(reg))
          result.insert(reg);
      }
      return result;
    }
  }

  // NOTE(jordan): collect_variables is kinda gross. Idk any better way.
  void collect_variables (
    node const & start,
    std::set<std::string> & variables
  ) {
    if (start.is<grammar::identifier::variable>()) {
      variables.insert(start.content());
    } else {
      for (auto & child : start.children) {
        collect_variables(*child, variables);
      }
    }
  }

  std::set<std::string> collect_variables (up_nodes const & instructions) {
    std::set<std::string> variables;
    for (auto & instruction_wrapper : instructions) {
      auto const & instruction = instruction_wrapper->children.at(0);
      for (auto & child : instruction->children) {
        collect_variables(*child, variables);
      }
    }
    return variables;
  }

  node const & definition_for (
    node const & label,
    up_nodes const & instructions
  ) {
    assert(
      label.is<grammar::operand::label>()
      && "definition_for: called on a non-label!"
    );
    for (up_node const & wrapper : instructions) {
      node const & instruction = unwrap_assert(*wrapper);
      if (instruction.is<grammar::instruction::define::label>()) {
        node const & defined_label = *instruction.children.at(0);
        assert(defined_label.is<grammar::operand::label>());
        if (defined_label.content() == label.content())
          return instruction;
      }
    }
    std::cerr << "Could not find " << label.content() << "\n";
    assert(false && "definition_for: could not find label!");
  }

  /* FIXME(jordan): this is crufty/gross. Shouldn't go backwards from
   * string to variable like this; we should just keep the nodes around.
   */
  std::string strip_variable_prefix (std::string const & s) {
    if (matches<grammar::identifier::variable>(s)) {
      using variable = peg::must<grammar::identifier::variable>;
      peg::string_input<> in(s, "");
      auto const & root
        = ast::L2::parse<variable, ast::L2::filter::selector>(in);
      assert(root->children.size() == 1);
      node const & var_node = *root->children.at(0);
      assert(var_node.children.size() == 1);
      node const & name_node = *var_node.children.at(0);
      assert(name_node.is<grammar::identifier::name>());
      return name_node.content();
    } else {
      return s;
    }
  }
}
