// vim: foldmethod=marker
#include <cassert>
#include <iostream>

#include "tao/pegtl.hpp"

#include "options.h"
#include "grammar.h"
#include "analysis.h"
//#include "codegen.h"
#include "parse_tree.h"

namespace peg = tao::pegtl;
namespace ast = L2::parse_tree;
using grammar  = peg::must<L2::grammar::entry>;
using function = peg::must<L2::grammar::function::define>;

template <Options::Mode mode, typename Entry, typename Input>
std::unique_ptr<ast::node> parse (Options & opt, Input & in) {
  if (opt.print_trace) {
    ast::debug::trace<Entry>(in);
    std::cerr << "Trace attempted. Exiting.\n";
    exit(-1);
  }
  auto root = ast::parse<Entry, ast::filter::selector>(in);
  if (opt.print_ast) { ast::print_node(*root); }
  return root;
}

template <typename Input>
std::unique_ptr<ast::node> parse (Options & opt, Input & in) {
  if (Options::Mode::x86 == opt.mode)
    return parse<Options::Mode::x86, grammar, Input>(opt, in);
  if (Options::Mode::liveness == opt.mode)
    return parse<Options::Mode::liveness, function, Input>(opt, in);
  assert(false && "parse: unreachable! Mode unrecognized.");
}

int main (int argc, char ** argv) {
  assert(argc > 1 && "Wrong number of arguments passed to compiler.");

  Options opt(argc, argv);
  peg::file_input<> in(opt.input_name);

  auto root = parse(opt, in);

  if (opt.mode == Options::Mode::x86) {
    /* L2::codegen::generate(*root); */
    std::cerr << "Error: cannot generate code for L2 yet.\n";
    return -1;
  }
  if (opt.mode == Options::Mode::liveness) {
    namespace liveness = L2::analysis::liveness;
    liveness::result result = liveness::compute(*root);
    liveness::print(std::cout, result);
    return 0;
  }

  assert(false && "main: unreachable! Unrecognized mode.");
}
