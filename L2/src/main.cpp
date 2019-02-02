// vim: foldmethod=marker
#include <cassert>
#include <unistd.h>

#include "tao/pegtl.hpp"
#include "tao/pegtl/contrib/tracer.hpp"

#include "grammar.h"
#include "analysis.h"
//#include "codegen.h"
#include "parse_tree.h"

namespace peg = tao::pegtl;
namespace ast = L2::parse_tree;
using grammar  = peg::must<L2::grammar::entry>;
using function = peg::must<L2::grammar::function::define>;

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

  while ((c = getopt(argc, argv, "pl:g:O:")) != -1)
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
      case 'p':
        opt.print_ast = true;
        break;
    }
  opt.input_name = argv[optind];
  // }}}

  peg::file_input<> in(opt.input_name);

  if (opt.mode == Options::Mode::x86) {
    auto root = ast::parse<grammar, ast::filter::selector>(in);
    if (opt.print_ast) { ast::print_node(*root); }
    /* L2::codegen::generate(*root); */
    std::cerr << "Error: cannot generate code for L2 yet.\n";
    return -1;
  } else if (opt.mode == Options::Mode::liveness) {
    auto root = ast::parse<function, ast::filter::selector>(in);
    if (opt.print_ast) { ast::print_node(*root); }
    namespace analysis = L2::analysis;
    analysis::liveness_result result = analysis::liveness(*root, true);
  }

  return 0;
}
