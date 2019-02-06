#pragma once
#include <unistd.h>
#include <cassert>
#include <cstring>

namespace driver::L2 {
  struct Options {
    enum Mode {
      x86,
      spill,
      liveness,
      interference,
    } mode = Mode::x86;
    bool parsed_mode = false;
    bool print_trace = false;
    bool print_ast   = false;
    char * input_name;
    static Options argv (int argc, char ** argv);
  };

  /* NOTE(jordan): just... gross. Not great. Ugh.
   * You know what? If we can just keep all the gross getopt stuff in this
   * one place, that's good enough. That's fine. I'll be ok.
   */
  void assert_single_mode (Options & opt) {
    assert(!opt.parsed_mode && "Error: already parsed a mode!");
    opt.parsed_mode = true;
  }

  bool check_set () {
    return strncmp(optarg, "1", 1) == 0;
  }

  /*
   * ====================================================================
   *  Handling CLI Input
   * ====================================================================
   *
   * If the API can't be clean, then the comments will do their damnedest
   * to polish that turd. While I cannot abolish the turd, I will not
   * abide its existing unpolished!
   *
   * +======================================================+
   * |                                                      |
   * |    ()                                                |
   * |   (  )                                               |
   * |  (    )    <~~~ Turd. Very polished. No blemishes.   |
   * | (______)                                             |
   * |                                                      |
   * +======================================================+
   *
   */
  Options Options::argv (int argc, char ** argv) {
    int c;
    Options opt;
    while ((c = getopt(argc, argv, "tpisl:g:O:")) != -1)
      switch (c) {
      /*
       * ----------------------------------------------------------------
       *  Mode selection
       * ----------------------------------------------------------------
       */
        case 'g':
          if (check_set()) {
            assert_single_mode(opt);
            opt.mode = Options::Mode::x86;
          }
          break;
        case 'l':
          if (check_set()) {
            assert_single_mode(opt);
            opt.mode = Options::Mode::liveness;
          }
          break;
        case 'i':
          assert_single_mode(opt);
          opt.mode = Options::Mode::interference;
          break;
        case 's':
          assert_single_mode(opt);
          opt.mode = Options::Mode::spill;
          break;
      /*
       * ----------------------------------------------------------------
       *  Optimization level (TODO)
       * ----------------------------------------------------------------
       */
        case 'O':
          // TODO(jordan): maybe we should care about opt level later.
          break;
      /*
       * ----------------------------------------------------------------
       *  Debug flags
       * ----------------------------------------------------------------
       */
        case 't':
          opt.print_trace = true;
          break;
        case 'p':
          opt.print_ast = true;
          break;
      }
    /*
     * ------------------------------------------------------------------
     *  Input file name (NOTE: assumes exactly 1 input)
     * ------------------------------------------------------------------
     */
    opt.input_name = argv[optind];
    return opt;
  }
}
