#include <cassert>

#include "tao/pegtl.hpp"
#include "tao/pegtl/contrib/tracer.hpp"

#include "grammar.h"
#include "codegen.h"
#include "ast.h"

namespace peg = tao::pegtl;
using entry = peg::must<grammar::L1::entry>;

namespace debug {
  void trace_parse (peg::file_input<> & in) {
    peg::parse<entry, peg::nothing, peg::tracer>(in);
  }
  void trace_parse_tree (peg::file_input<> & in) {
    ast::L1::parse<
      entry,
      ast::L1::filter::selector,
      peg::nothing,
      peg::tracer
    >(in);
  }
}

int main (int argc, char ** argv) {
  assert(argc > 1 && "Wrong number of arguments passed to compiler.");
  peg::file_input<> in(argv[1]);
  /* debug::trace_parse(in); */
  /* debug::trace_parse_tree(in); */
  auto root = ast::L1::parse<entry, ast::L1::filter::selector>(in);
  /* ast::print_node(*root); */
  codegen::L1::generate::to_file("prog.S", *root);
  return 0;
}
