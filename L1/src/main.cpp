#include <cassert>

#include "tao/pegtl.hpp"
#include "tao/pegtl/contrib/tracer.hpp"

#include "grammar.h"
#include "codegen.h"
#include "parse_tree.h"

namespace peg = tao::pegtl;
namespace ast = L1::parse_tree;
using grammar = peg::must<L1::grammar::entry>;

namespace debug {
  void trace_parse (peg::file_input<> & in) {
    peg::parse<grammar, peg::nothing, peg::tracer>(in);
  }
  void trace_parse_tree (peg::file_input<> & in) {
    ast::parse<
      grammar,
      ast::filter::selector,
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
  auto root = ast::parse<grammar, ast::filter::selector>(in);
  /* ast::print_node(*root); */
  L1::codegen::generate(*root);
  return 0;
}
