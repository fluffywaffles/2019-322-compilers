#include <cassert>
#include <iostream>

#include "tao/pegtl.hpp"

#include "driver.h"

int main (int argc, char ** argv) {
  namespace driver = driver::IR;
  using Options = driver::Options;
  // TODO(jordan): Print some kind of help output.
  assert(argc > 1 && "Wrong number of arguments passed to compiler.");
  Options opt = Options::argv(argc, argv);
  peg::file_input<> in(opt.input_name);
  return driver::execute(opt, in);
}
