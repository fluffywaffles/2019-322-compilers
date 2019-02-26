#pragma once
#include <cassert>
#include <iostream>
#include <fstream>

#include "tao/pegtl.hpp"

#include "driver/options.h"
#include "grammar.h"
#include "ast.h"
#include "analysis.h"

namespace driver::IR {
  namespace peg = tao::pegtl;

  using node     = ast::node;
  using up_node  = ast::up_node;
  using up_nodes = ast::up_nodes;
  using program  = peg::must<grammar::IR::program>;

  template <typename Entry, typename Input>
  up_node parse (Options & opt, Input & in) {
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
  up_node parse (Options & opt, Input & in) {
    using Mode = Options::Mode;
    switch (opt.mode) {
      case Mode::x86       : return parse<program>(opt, in);
      case Mode::run_tests : return parse<program>(opt, in);
    }
    assert(false && "parse: unreachable! Mode unrecognized.");
  }

  template <typename Input>
  int execute (Options & opt, Input & in) {
    up_node const root = parse(opt, in);
    if (Options::Mode::x86 == opt.mode) {
      return 0;
    }
    if (Options::Mode::run_tests == opt.mode) {
      node const & program = *root->children.at(0);
      for (up_node const & function : program.children) {
        std::cerr
          << "\nfunction " << function->children.at(1)->content()
          << "\n----------------------------------------\n\n";
        auto vars = analysis::IR::variables::summarize({ &*function });
        analysis::IR::variables::print(vars);
      }
      return 0;
    }
    assert(false && "execute: unreachable! Mode unrecognized.");
  }
}
