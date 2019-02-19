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
    if (Options::Mode::x86 == opt.mode) {
      auto const root = parse(opt, in);
      std::cerr << "Error: Cannot generate L3 yet!\n";
      return -1;
    }
    if (Options::Mode::liveness == opt.mode) {
      namespace analysis = analysis::L3;
      auto const root = parse(opt, in);
      ast::node const & program = *root->children.at(0);
      ast::node const & function = *program.children.at(0);
      auto const summary = analysis::function::summarize(function);
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
      return 0;
    }
    if (Options::Mode::test_node == opt.mode) {
      assert(std::string(opt.input_name) == "tests/test3.L3");
      peg::file_input<> in("tests/test2.L3");
      auto const root = parse(opt, in);
      ast::node const & program  = *root->children.at(0);
      ast::node const & function = *program.children.at(0);
      ast::node const & contexts = *function.children.at(2);
      ast::node const & context  = *contexts.children.at(0);
      ast::node const & instruction = *context.children.at(0);
      ast::node & operand = *instruction.children.at(0);
      operand.realize();
      operand.transform<
        grammar::L3::operand::value,
        grammar::L3::operand::variable
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
