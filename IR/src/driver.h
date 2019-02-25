#pragma once
#include <cassert>
#include <iostream>
#include <fstream>

#include "tao/pegtl.hpp"

#include "driver/options.h"
#include "grammar.h"
#include "ast.h"

namespace driver::IR {
  namespace peg = tao::pegtl;

  using node     = ast::node;
  /* using up_node  = helper::IR::up_node; */
  /* using up_nodes = helper::IR::up_nodes; */
  using program  = peg::must<grammar::IR::program>;

  template <typename Entry, typename Input>
  std::unique_ptr<node> parse (Options & opt, Input & in) {
    if (opt.print_trace) {
      ast::IR::debug::trace<Entry>(in);
      std::cerr << "Parse trace written. Exiting.\n";
      exit(-1);
    }
    auto root = ast::IR::parse<Entry>(in);
    if (opt.print_ast) { ast::IR::debug::print_node(*root); }
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
      return 0;
    }
    if (Options::Mode::liveness == opt.mode) {
      return 0;
    }
    if (Options::Mode::test_node == opt.mode) {

      return 0;
    }
    if (Options::Mode::run_arbitrary_tests == opt.mode) {
      return 0;
    }
    assert(false && "execute: unreachable! Mode unrecognized.");
  }
}
