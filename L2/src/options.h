#pragma once
#include <unistd.h>

struct Options {
  enum Mode {
    x86,
    liveness,
  } mode = Mode::x86;
  bool print_trace = false;
  bool print_ast   = false;
  char * input_name;
  Options (int argc, char ** argv) { Options::parse(*this, argc, argv); }
  static void parse (Options & opt, int argc, char ** argv);
};
