// vim: foldmethod=marker
#include <cassert>
#include <iostream>

#include "tao/pegtl.hpp"

#include "driver.h"

int main (int argc, char ** argv) {
  namespace driver = driver::L2;
  using Options = driver::Options;
  assert(argc > 1 && "Wrong number of arguments passed to compiler.");
  Options opt = Options::argv(argc, argv);
  peg::file_input<> in(opt.input_name);
  return driver::execute(opt, in);
}
