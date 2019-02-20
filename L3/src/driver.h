#pragma once
#include <cassert>
#include <iostream>
#include <fstream>

#include "tao/pegtl.hpp"

#include "driver/options.h"
#include "helper.h"
#include "analysis.h"
#include "grammar.h"
#include "ast.h"

namespace driver::L3 {
  namespace peg = tao::pegtl;
  namespace ast = ast::L3;

  using program  = peg::must<grammar::L3::program>;

  template <typename Entry, typename Input>
  std::unique_ptr<ast::node> parse (Options & opt, Input & in) {
    if (opt.print_trace) {
      ast::debug::trace<Entry>(in);
      std::cerr << "Parse trace written. Exiting.\n";
      exit(-1);
    }
    auto root = ast::parse<Entry>(in);
    if (opt.print_ast) { ast::debug::print_node(*root); }
    return root;
  }

  template <typename Input>
  std::unique_ptr<ast::node> parse (Options & opt, Input & in) {
    using Mode = Options::Mode;
    switch (opt.mode) {
      case Mode::x86       : return parse<program>(opt, in);
      case Mode::liveness  : return parse<program>(opt, in);
      case Mode::test_node : return parse<program>(opt, in);
    }
    assert(false && "parse: unreachable! Mode unrecognized.");
  }

  template <typename Input>
  int execute (Options & opt, Input & in) {
    auto const root = parse(opt, in);
    if (Options::Mode::x86 == opt.mode) {
      std::cerr << "Error: Cannot generate L3 yet!\n";
      return -1;
    }
    if (Options::Mode::liveness == opt.mode) {
      namespace analysis = analysis::L3;
      ast::node const & program = *root->children.at(0);
      ast::node const & function = *program.children.at(0);
      auto const summary = analysis::function::summarize(function);
      std::cout << "function " << summary.name << "\n";
      analysis::liveness::print(
        std::cout,
        summary.instructions,
        summary.liveness
      );
      std::cout << "variables\n\t";
      for (auto const & variable : summary.variables_summary.variables) {
        std::cout << variable << " ";
      }
      std::cout << "\n";
      std::cout << "labels\n\t";
      for (auto const & label : summary.labels_summary.labels) {
        std::cout << label << " ";
      }
      std::cout << "\n";
      std::cout << "label definitions\n";
      for (auto const & def_entry : summary.labels_summary.definitions) {
        auto const & defs = def_entry.second;
        assert(defs.size() == 1 && "function: label defined >1 time!");
        ast::node const * def  = *defs.begin();
        ast::node const & defined_label = helper::L3::unwrap_assert(*def);
        std::cout
          << "\t" << *def_entry.first
          << " (as " << defined_label.content() << ") defined at"
          << " " << (*defs.begin())->begin()
          << "\n";
      }
      std::cout << "\n";
      std::cout << "label uses\n";
      for (auto const & use : summary.labels_summary.uses) {
        auto const & uses = use.second;
        std::cout
          << "\t" << *use.first << " used...";
        for (auto const & use_site : uses) {
          ast::node const & label = *use_site;
          std::cout
            << "\n\t\tas " << label.content()
            << " at "
            << (use_site->realized
                  ? use_site->original_begin()
                  : use_site->begin());
        }
        std::cout << "\n";
      }
      std::cout << "\n";
      return 0;
    }
    if (Options::Mode::test_node == opt.mode) {
      assert(std::string(opt.input_name) == "tests/test2.L3");
      ast::node const & program  = *root->children.at(0);
      ast::node const & original_function = *program.children.at(0);
      helper::L3::up_node const up_function = original_function.clone();
      ast::node const & function = *up_function;
      ast::node const & contexts = *function.children.at(2);
      ast::node const & context  = *contexts.children.at(0);
      ast::node const & instruction = *context.children.at(0);
      ast::node const & original_operand = *instruction.children.at(0);
      helper::L3::up_node const up_operand = original_operand.clone(false);
      ast::node & operand = *up_operand;
      /* operand.realize(); */
      operand.transform<
        grammar::L3::operand::value,
        grammar::L2::operand::variable
      >("%one_two");
      std::cout
        << "is from: " << operand.source
        << ", is: " << operand.name()
        << " " << operand.content()
        << "\n";
      std::cout
        << "was from: " << operand.original_source
        << ", was: " << operand.original_name()
        << " " << operand.original_content()
        << "\n";
      std::cout
        << "which itself was from: " << original_operand.original_source
        << ", and was: " << original_operand.original_name()
        << " " << original_operand.original_content()
        << "\n";

      std::unique_ptr<ast::node> label
        = ast::construct::free_node<grammar::L3::operand::label>(":test");
      std::cout
        << "is from: " << label->source
        << ", is: " << label->name()
        << " " << label->realized_content
        << "\n";
      label->transform<
        grammar::L3::operand::label,
        grammar::L3::operand::variable
      >("%some_var");
      std::cout
        << "now: "
        << label->name()
        << " " << label->content()
        << "\n";
      std::cout << "was from: "
        << label->original_source
        << ", was: "
        << label->original_name()
        << " "
        << label->original_content()
        << "\n";
      return 0;
    }
    assert(false && "execute: unreachable! Mode unrecognized.");
  }
}
