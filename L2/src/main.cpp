// vim: foldmethod=marker
#include <cassert>
#include <unistd.h>

#include "tao/pegtl.hpp"
#include "tao/pegtl/contrib/tracer.hpp"

#include "grammar.h"
//#include "codegen.h"
#include "parse_tree.h"

namespace peg = tao::pegtl;
namespace ast = L2::parse_tree;
using grammar = peg::must<L2::grammar::entry>;

namespace debug {
  void trace_parse (peg::file_input<> & in) {
    peg::parse<grammar, peg::nothing, peg::tracer>(in);
  }
  auto trace_ast (peg::file_input<> & in) {
    return ast::parse<
      grammar,
      ast::filter::selector,
      peg::nothing,
      peg::tracer
    >(in);
  }
}

/* TODO(jordan):
 *
 * Support new command-line arguments that we're required to understand.
 *
 */
struct Options {
  enum Mode {
    x86,
    liveness,
  } mode = Mode::x86;
  bool print_trace = false;
  bool print_ast   = false;
  char * input_name;
};

int main (int argc, char ** argv) {
  assert(argc > 1 && "Wrong number of arguments passed to compiler.");

  // {{{ options parsing
  Options opt;
  int c;

  while ((c = getopt(argc, argv, "lg:O:")) != -1)
    switch (c) {
      case 'g':
        opt.mode = Options::Mode::x86;
        break;
      case 'l':
        opt.mode = Options::Mode::liveness;
        break;
      case 'O':
        // TODO(jordan): maybe we should care about opt level later.
        break;
    }
  opt.input_name = argv[optind];
  // }}}

  peg::file_input<> in(opt.input_name);

  std::unique_ptr<ast::node> root;
  if (opt.print_trace) {
    root = debug::trace_ast(in);
  } else {
    root = ast::parse<grammar, ast::filter::selector>(in);
  }

  if (opt.print_ast) {
    ast::print_node(*root);
  }

  if (opt.mode == Options::Mode::x86) {
    std::cerr << "Error: cannot generate code for L2 yet.\n";
    return -1;
    /* L2::codegen::generate(*root); */
  } else if (opt.mode == Options::Mode::liveness) {
    return 0;
    /* L2::analysis::liveness(*root); */
  }

  return 0;
}
