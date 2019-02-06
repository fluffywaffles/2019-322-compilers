#include <iostream>

#include "tao/pegtl.hpp"

#include "driver/options.h"

#include "grammar.h"
#include "analysis.h"
#include "ast.h"

namespace driver::L2 {
  namespace peg = tao::pegtl;
  namespace ast = ast::L2;

  using program  = peg::must<grammar::L2::entry>;
  using function = peg::must<grammar::L2::function::define>;

  template <typename Entry, typename Input>
  std::unique_ptr<ast::node> parse (Options & opt, Input & in) {
    if (opt.print_trace) {
      ast::debug::trace<Entry>(in);
      std::cerr << "Parse trace written. Exiting.\n";
      exit(-1);
    }
    auto root = ast::parse<Entry, ast::filter::selector>(in);
    if (opt.print_ast) { ast::print_node(*root); }
    return root;
  }

  template <typename Input>
  std::unique_ptr<ast::node> parse (Options & opt, Input & in) {
    using Mode = Options::Mode;
    switch (opt.mode) {
      case Mode::x86      : return parse<program >(opt, in);
      case Mode::liveness : return parse<function>(opt, in);
    }
    assert(false && "parse: unreachable! Mode unrecognized.");
  }

  template <typename Input>
  int execute (Options & opt, Input & in) {
    auto root = parse(opt, in);

    if (opt.mode == Options::Mode::x86) {
      /* L2::codegen::generate(*root); */
      std::cerr << "Error: cannot generate code for L2 yet.\n";
      return -1;
    }
    if (opt.mode == Options::Mode::liveness) {
      namespace liveness = analysis::L2::liveness;
      liveness::result result = liveness::compute(*root);
      liveness::print(std::cout, result);
      return 0;
    }

    assert(false && "execute: unreachable! Mode unrecognized.");
  }
}
