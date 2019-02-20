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
#include "transform.h"
#include "tile.h"
#include "L2/transform.h"

namespace driver::L3 {
  namespace peg = tao::pegtl;

  using node     = ast::L3::node;
  using up_node  = helper::L3::up_node;
  using up_nodes = helper::L3::up_nodes;
  using program  = peg::must<grammar::L3::program>;

  template <typename Entry, typename Input>
  std::unique_ptr<node> parse (Options & opt, Input & in) {
    if (opt.print_trace) {
      ast::L3::debug::trace<Entry>(in);
      std::cerr << "Parse trace written. Exiting.\n";
      exit(-1);
    }
    auto root = ast::L3::parse<Entry>(in);
    if (opt.print_ast) { ast::L3::debug::print_node(*root); }
    return root;
  }

  template <typename Input>
  std::unique_ptr<node> parse (Options & opt, Input & in) {
    using Mode = Options::Mode;
    switch (opt.mode) {
      case Mode::x86       : return parse<program>(opt, in);
      case Mode::liveness  : return parse<program>(opt, in);
      case Mode::test_node : return parse<program>(opt, in);
      case Mode::run_arbitrary_tests : return parse<program>(opt, in);
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
      node const & program  = *root->children.at(0);
      node const & function = *program.children.at(0);
      auto const summary = analysis::function::summarize(function);
      std::cout << "function " << summary.name << "\n";

      helper::view::vec<node> last_instruction;
      last_instruction.push_back(summary.instructions.back());
      // NOTE(jordan): print matcher debug log (2nd argument = true)
      namespace tile = tile::L3;
      auto retnul = tile::ret::nothing::accept(last_instruction, true);
      auto retval = tile::ret::value::accept(last_instruction, true);
      std::cout
        << "ret::nothing?"
        << " " << retnul
        << "\n";
      std::cout
        << "ret::value?"
        << " " << retval
        << "\n";

      if (retnul) {
        up_node l2 = tile::ret::nothing::generate(last_instruction, true);
        transform::L2::spit::instructions(*l2, std::cout);
      } else if (retval) {
        up_node l2 = tile::ret::value::generate(last_instruction, true);
        transform::L2::spit::instructions(*l2, std::cout);
      }

      analysis::liveness::print(summary.instructions, summary.liveness);
      // NOTE(jordan): watch out! sharp! This mutates the labels.
      transform::L3::globalize::apply(summary.name, summary.labels_summary);
      analysis::variables::print(summary.variables_summary);
      analysis::labels::print(summary.labels_summary);
      return 0;
    }
    if (Options::Mode::test_node == opt.mode) {
      assert(std::string(opt.input_name) == "tests/test2.L3");
      node const & program  = *root->children.at(0);
      node const & original_function = *program.children.at(0);
      up_node const up_function = original_function.clone();
      node const & function = *up_function;
      node const & contexts = *function.children.at(2);
      node const & context  = *contexts.children.at(0);
      node const & instruction = *context.children.at(0);
      node const & original_operand = *instruction.children.at(0);
      up_node const up_operand = original_operand.clone(false);
      node & operand = *up_operand;
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

      std::unique_ptr<node> label
        = ast::L3::construct::from_string<grammar::L2::operand::label>(
          ":test"
        );
      std::cout
        << "is from: " << label->source
        << ", is: " << label->name()
        << " " << label->realized_content
        << "\n";
      label->transform<
        grammar::L2::operand::label,
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
    if (Options::Mode::run_arbitrary_tests == opt.mode) {
      namespace tile = tile::L3;
      up_node const print_node
        = ast::L3::construct::from_string<grammar::L3::instruction::call>(
          "call print(5)" // prints 2
        );
      helper::view::vec<node> print_window;
      print_window.push_back(&*print_node);
      std::cout
        << "call::any?"
        << "\t" << tile::call::any::accept(print_window, true)
        << "\n";
      std::cout
        << "call::defined?"
        << "\t" << tile::call::defined::accept(print_window, true)
        << "\n";
      std::cout
        << "call::intrinsic::print?"
        << "\t" << tile::call::intrinsic::print::accept(print_window, true)
        << "\n";
      return 0;
    }
    assert(false && "execute: unreachable! Mode unrecognized.");
  }
}
