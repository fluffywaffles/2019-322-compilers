#pragma once

#include <cassert>
#include <iostream>
#include <fstream>

#include "tao/pegtl.hpp"

#include "driver/options.h"
#include "ast.h"
#include "grammar.h"
#include "transform.h"
#include "analysis.h"
//#include "helper.h"

namespace driver::LA {
  namespace peg = tao::pegtl;

  using node     = ast::node;
  using up_node  = ast::up_node;
  using up_nodes = ast::up_nodes;
  using program  = peg::must<grammar::LA::program>;

  template <typename Entry, typename Input>
  up_node parse (Options & opt, Input & in) {
    if (opt.print_trace) {
      ast::LA::debug::trace<Entry>(in);
      std::cerr << "Parse trace written. Exiting.\n";
      exit(-1);
    }
    auto root = ast::LA::parse<Entry>(in);
    if (opt.print_ast) { ast::debug::print_node(*root); }
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
      node const & program = *root->children.at(0);
      return 0;
    }
    if (Options::Mode::run_tests == opt.mode) {
      node const & program  = *root->children.at(0);
      up_nodes const & up_functions = program.children;
      auto functions_summary
        = analysis::LA::functions::summarize(up_functions);
      ast::walk< ast::mutator::trim_content >(up_functions);
      ast::walk< transform::LA::identify_names >(
        up_functions,
        functions_summary
      );
      ast::debug::print_node(*root);
      return 0;
    }
    assert(false && "execute: unreachable! Mode unrecognized.");
  }
}
