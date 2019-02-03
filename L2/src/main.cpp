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
using program  = peg::must<L2::grammar::entry>;
using function = peg::must<L2::grammar::function::define>;

template <typename Entry, typename Options, typename Input>
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