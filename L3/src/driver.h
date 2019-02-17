#pragma once
#include <cassert>
#include <iostream>
#include <fstream>

#include "tao/pegtl.hpp"

#include "driver/options.h"

#include "grammar.h"
#include "ast.h"

namespace driver::L3 {
  namespace peg = tao::pegtl;
  namespace ast = ast::L3;
  //namespace transform = transform::L2;

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
      case Mode::x86      : return parse<program>(opt, in);
      case Mode::liveness : return parse<program>(opt, in);
    }
    assert(false && "parse: unreachable! Mode unrecognized.");
  }

  template <typename Input>
  int execute (Options & opt, Input & in) {
    auto const root = parse(opt, in);
    if (Options::Mode::x86 == opt.mode) { // {{{
      /* ast::node const & program  = *root->children.at(0); */
      /* ast::node const & function = *program.children.at(0); */
      /* ast::node const & contexts = *function.children.at(2); */
      /* ast::node const & context  = *contexts.children.at(0); */
      /* ast::node const & instruction = *context.children.at(0); */
      /* ast::node & operand = *instruction.children.at(0); */
      /* operand.realize(); */
      /* std::cout << operand.name() << " has '" << operand.content() << "'\n"; */

      /* std::unique_ptr<ast::node> label */
      /*   = ast::construct::free_node<grammar::L3::operand::label>(":test"); */
      /* std::cout << label->realized_content << "\n"; */
      /* label->reset<grammar::L3::operand::label>(":not_test"); */
      /* std::cout << label->name() << " has '" << label->content() << "'\n"; */

      std::cerr << "Error: Cannot generate L3 yet!\n";
      return -1;
    } // }}}
    if (Options::Mode::liveness == opt.mode) { // {{{
      /* namespace analysis = analysis::L3; */
      /* auto const & function = root->children.at(0); */
      /* auto liveness = analysis::liveness::function(*function); */
      /* analysis::liveness::print(std::cout, liveness); */
      std::cerr << "Error: Cannot analyze liveness for L3 yet!\n";
      return -1;
    } // }}}
    assert(false && "execute: unreachable! Mode unrecognized.");
  }
}
