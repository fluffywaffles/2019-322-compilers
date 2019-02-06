#pragma once
#include <unistd.h>

namespace driver::L2 {
  struct Options {
    enum Mode {
      x86,
      liveness,
    } mode = Mode::x86;
    bool print_trace = false;
    bool print_ast   = false;
    char * input_name;
    static Options argv (int argc, char ** argv);
  };

  Options Options::argv (int argc, char ** argv) {
    int c;
    Options opt;
    while ((c = getopt(argc, argv, "tpl:g:O:")) != -1)
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
        case 't':
          opt.print_trace = true;
          break;
        case 'p':
          opt.print_ast = true;
          break;
      }
    opt.input_name = argv[optind];
    return opt;
  }
}
