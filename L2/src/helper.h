#pragma once

#include <set>
#include <typeindex>
#include <algorithm>

#include "tao/pegtl.hpp"

#include "grammar.h"
#include "ast.h"

// Language-agnostic helpers
namespace helper {
  namespace peg = tao::pegtl;
  template <typename Rule>
  bool matches (const std::string string) {
    // WAFKQUSKWLZAQWAAAA YES I AM THE T<EMPL>ATE RELEASER OF Z<ALGO>
    using namespace peg;
    memory_input<> string_input (string, "<call to matches>");
    return normal<Rule>::template
      match<apply_mode::NOTHING, rewind_mode::DONTCARE, nothing, normal>
      (string_input);
  }
  template <typename Rule, typename Node>
  bool matches (peg::parse_tree::basic_node<Node> const & n) {
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
}

// Language-specific helper-generators
namespace helper::meta {
  template <typename Node, typename Int>
  int integer (Node const & n) {
    assert(n.has_content() && "helper::integer: no content!");
    assert(
      matches<Int>(n)
      && "helper::integer: does not match literal::number::integer!"
    );
    return std::stoi(n.content());
  }

  template <typename Node>
  Node const & unwrap_assert (Node const & parent) {
    assert(
      parent.children.size() == 1
      && "helper::unwrap_assert: not exactly 1 child in parent!"
    );
    return *parent.children.at(0);
  }

  // NOTE(jordan): collect_variables is kinda gross. Idk any better way.
  template <typename Node, typename Variable>
  void collect_variables (
    Node const & start,
    std::set<std::string> & variables
  ) {
    if (start.template is<Variable>()) {
      variables.insert(start.content());
    } else {
      for (auto & child : start.children) {
        collect_variables<Node, Variable>(*child, variables);
      }
    }
  }
  template <typename Node, typename Variable>
  std::set<std::string> collect_variables (
    std::vector<std::unique_ptr<Node>> const & instructions
  ) {
    std::set<std::string> variables;
    for (auto & instruction_wrapper : instructions) {
      auto const & instruction = instruction_wrapper->children.at(0);
      for (auto & child : instruction->children) {
        collect_variables<Node, Variable>(*child, variables);
      }
    }
    return variables;
  }

  /* FIXME(jordan): this is crufty/gross. Shouldn't go backwards from
   * string to variable like this; we should just keep the nodes around.
   */
  template <typename Node, typename Variable, typename Name>
  std::string strip_variable_prefix (std::string const & s) {
    if (matches<Variable>(s)) {
      using variable = peg::must<Variable>;
      peg::string_input<> in(s, "");
      auto const & root
        = ast::L2::parse<variable, ast::L2::filter::selector>(in);
      assert(root->children.size() == 1);
      Node const & var_node = *root->children.at(0);
      assert(var_node.children.size() == 1);
      Node const & name_node = *var_node.children.at(0);
      assert(name_node.template is<Name>());
      return name_node.content();
    } else {
      return s;
    }
  }

  namespace label {
    template<typename Node, typename Label, typename LabelDef>
    Node const & definition_for (
      Node const & label,
      std::vector<std::unique_ptr<Node>> const & instructions
    ) {
      using up_node = std::unique_ptr<Node>;
      assert(
        label.template is<Label>()
        && "definition_for: called on a non-label!"
      );
      for (up_node const & wrapper : instructions) {
        Node const & instruction = unwrap_assert(*wrapper);
        if (instruction.template is<LabelDef>()) {
          Node const & defined_label = *instruction.children.at(0);
          assert(defined_label.template is<Label>());
          if (defined_label.content() == label.content())
            return instruction;
        }
      }
      std::cerr << "Could not find " << label.content() << "\n";
      assert(false && "definition_for: could not find label!");
    }
  }
}

// L2-specific helpers
namespace helper::L2 {
  namespace grammar = grammar::L2;
  using node = ast::L2::node;

  using up_node  = std::unique_ptr<node>;
  using up_nodes = std::vector<up_node>;

  int integer (node const & n) {
    return meta::integer<node, grammar::literal::number::integer::any>(n);
  }

  std::set<std::string> collect_variables (up_nodes const & insts) {
    using variable = grammar::identifier::variable;
    return meta::collect_variables<node, variable>(insts);
  }

  node const & definition_for(
    node     const & label,
    up_nodes const & insts
  ) {
    using Label = grammar::operand::label;
    using Defn  = grammar::instruction::define::label;
    return meta::label::definition_for<node, Label, Defn>(label, insts);
  }

  std::string strip_variable_prefix(std::string const & v) {
    using variable = grammar::operand::variable;
    using name     = grammar::identifier::name;
    return meta::strip_variable_prefix<node, variable, name>(v);
  };

  node const & unwrap_assert (node const & parent) {
    return meta::unwrap_assert<node>(parent);
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
}
