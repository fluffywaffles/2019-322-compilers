#include "options.h"

void Options::parse (Options & opt, int argc, char ** argv) {
  int c;
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
}
