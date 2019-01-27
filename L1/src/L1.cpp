#include <cassert>

#include "grammar.h"
#include "tao/pegtl.hpp"
#include "tao/pegtl/contrib/tracer.hpp"

namespace peg = tao::pegtl;
using grammar = peg::must<L1::grammar::program>;

namespace debug {
  void trace_parse (peg::file_input<> &in) {
    peg::parse<grammar, peg::nothing, peg::tracer>(in);
  }
}

int main (int argc, char ** argv) {
  assert(argc > 1 && "Wrong number of arguments passed to compiler.");
  peg::file_input<> in(argv[1]);
  debug::trace_parse(in);
  return 0;
}
