// vim: set foldmethod=marker:
#include <cassert>
#include <iostream>
#include <fstream>

#include "tao/pegtl.hpp"

#include "driver/options.h"

#include "grammar.h"
#include "analysis.h"
#include "transform.h"
#include "ast.h"

namespace driver::L2 {
  namespace peg = tao::pegtl;
  namespace ast = ast::L2;
  namespace transform = transform::L2;

  using program  = peg::must<grammar::L2::entry>;
  using function = peg::must<grammar::L2::function::define>;
  using spiller  = peg::must<grammar::L2::spiller>;

  template <typename Entry, typename Input>
  std::unique_ptr<ast::node> parse (Options & opt, Input & in) {
    if (opt.print_trace) {
      ast::debug::trace<Entry>(in);
      std::cerr << "Parse trace written. Exiting.\n";
      exit(-1);
    }
    auto root = ast::parse<Entry, ast::filter::selector>(in);
    if (opt.print_ast) { ast::print_node(*root); }
    return root;
  }

  template <typename Input>
  std::unique_ptr<ast::node> parse (Options & opt, Input & in) {
    using Mode = Options::Mode;
    switch (opt.mode) {
      // FIXME(jordan): Mode::x86 should parse a whole program
      case Mode::x86          : return parse<program >(opt, in);
      case Mode::spill        : return parse<spiller >(opt, in);
      case Mode::liveness     : return parse<function>(opt, in);
      case Mode::interference : return parse<function>(opt, in);
    }
    assert(false && "parse: unreachable! Mode unrecognized.");
  }

  template <typename Input>
  int execute (Options & opt, Input & in) {
    /**
     * Anyone want to take a guess as to why this causes a segfault? ;)
     *
     *    auto & root = *parse(opt, in);
     *
     * HINT {{{
     * (note that parse returns a std::unique_ptr<node>)
     * }}}
     **/
    const auto root = parse(opt, in);
    if (Options::Mode::x86 == opt.mode) {
      namespace analysis = analysis::L2;
      const ast::node & program = *root->children.at(0);
      /* transform::to_L1::program(program, std::cout); */
      // for function in program:
      // 1. liveness
      // 2. interference
      // 3. color
      // 4. spill? -> if yes, goto 1
      // 5. done.
      /**
       * FIXME(jordan): so... this is a mess.
       *
       * An AST node only stays in scope for as long as its corresponding
       * peg::Input type -- which is non-copyable and non-movable, and so
       * bound very tightly to the current lexical scope.
       *
       * What this means is: we have to materialize the AST for any
       * transformed code on-demand every time. We can't store its input;
       * we can't trust its contents.
       *
       * What it meant for the last 6 hours of my life was:
       *
       *    (:go
       *      0 0
       *      return)
       *
       * Turned into stuff like:
       *
       *    (UFweqUu
       *          ∀
       *        urn)
       *
       * So that was fun. Praise Zalgo.
       *
       * Had I known then what I know now, this all could be much cleaner.
       */
      const ast::node & functions = *program.children.at(1);
      std::map<int, std::string> replacement_functions;
      std::map<int, analysis::color::result> colorings;
      /* for (const auto & function : functions.children) { */
      /*   auto liveness     = analysis::liveness::function(*function); */
      /*   auto interference = analysis::interference::function(liveness); */
      /*   analysis::interference::print(std::cout, interference); */
      /* } */
      // Color the graph
      // NOTE(jordan): One good thing: this clean condition expression.
      while (colorings.size() < functions.children.size()) {
        for (int index = 0; index < functions.children.size(); index++) {
          auto found_position = replacement_functions.find(index);
          if (found_position != replacement_functions.end()) {
            // materialize the replacement AST...
            peg::memory_input<> in(replacement_functions.at(index), "");
            const auto root = parse<driver::L2::function>(opt, in);
            const ast::node & function = *root->children.at(0);
            transform::color::try_color_function(
              function,
              index,
              replacement_functions,
              colorings
            );
          } else {
            const ast::node & function = *functions.children.at(index);
            const std::string & name = function.children.at(0)->content();
            transform::color::try_color_function(
              function,
              index,
              replacement_functions,
              colorings
            );
          }
        }
      }
      // Rewrite variables
      for (int index = 0; index < functions.children.size(); index++) {
        auto found_position = replacement_functions.find(index);
        if (found_position != replacement_functions.end()) {
          /* std::cout << replacement_functions.at(index); */
          auto coloring = colorings.at(index);
          /* analysis::color::print(std::cout, coloring); */
          /* analysis::interference::print(std::cout, coloring.interference); */
          // materialize the replacement AST...
          peg::memory_input<> in(replacement_functions.at(index), "");
          const auto root = parse<driver::L2::function>(opt, in);
          const ast::node & function = *root->children.at(0);
          std::ostringstream os;
          transform::color::apply::function(function, coloring, os);
          /* std::cout << os.str() << "\n\n"; */
          replacement_functions[index] = os.str();
        } else {
          const ast::node & function = *functions.children.at(index);
          /* transform::spill::function(function, "", "", std::cout); */
          auto coloring = colorings.at(index);
          /* analysis::color::print(std::cout, coloring); */
          /* analysis::interference::print(std::cout, coloring.interference); */
          std::ostringstream os;
          transform::color::apply::function(function, coloring, os);
          /* std::cout << os.str() << "\n\n"; */
          replacement_functions[index] = os.str();
        }
      }
      // Transform stack-arg
      for (auto entry : replacement_functions) {
        const std::string & function_string = entry.second;
        peg::memory_input<> in(function_string, "");
        const auto root = parse<driver::L2::function>(opt, in);
        const ast::node & function = *root->children.at(0);
        std::ostringstream os;
        transform::to_L1::function(function, os);
        /* std::cout << os.str() << "\n\n"; */
        replacement_functions[entry.first] = os.str();
      }
      // Output the program
      std::ofstream out;
      out.open("prog.L1");
      const ast::node & entry = *program.children.at(0);
      out
        << "(" << entry.content()
        << "\n";
      for (auto entry : replacement_functions)
        out << entry.second << "\n";
      out << "\n)";
      return 0;
      /* L2::codegen::generate(*root); */
      /* std::cerr << "Error: cannot generate code for L2 yet.\n"; */
      /* return -1; */
    }
    if (Options::Mode::spill == opt.mode) {
      using node = ast::node;
      assert(root->children.size() == 3 && "spill: parsed incorrectly!");
      const node & function = *root->children.at(0);
      const node & target   = *root->children.at(1);
      const node & prefix   = *root->children.at(2);
      transform::spill::function(function, target, prefix, std::cout);
      return 0;
    }
    if (Options::Mode::liveness == opt.mode) {
      namespace analysis = analysis::L2;
      const auto & function = root->children.at(0);
      auto liveness = analysis::liveness::function(*function);
      analysis::liveness::print(std::cout, liveness);
      return 0;
    }
    if (Options::Mode::interference == opt.mode) {
      namespace analysis = analysis::L2;
      const auto & function = root->children.at(0);
      auto liveness     = analysis::liveness::function(*function);
      auto interference = analysis::interference::function(liveness);
      analysis::interference::print(std::cout, interference);
      return 0;
    }
    assert(false && "execute: unreachable! Mode unrecognized.");
  }
}
