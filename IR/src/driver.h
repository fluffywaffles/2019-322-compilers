#pragma once
#include <cassert>
#include <iostream>
#include <fstream>

#include "tao/pegtl.hpp"

#include "driver/options.h"
#include "ast.h"
#include "grammar.h"
#include "analysis.h"
#include "codegen.h"
#include "helper.h"

namespace driver::IR {
  namespace peg = tao::pegtl;

  using node     = ast::node;
  using up_node  = ast::up_node;
  using up_nodes = ast::up_nodes;
  using program  = peg::must<grammar::IR::program>;

  template <typename Entry, typename Input>
  up_node parse (Options & opt, Input & in) {
    if (opt.print_trace) {
      ast::IR::debug::trace<Entry>(in);
      std::cerr << "Parse trace written. Exiting.\n";
      exit(-1);
    }
    auto root = ast::IR::parse<Entry>(in);
    if (opt.print_ast) { ast::IR::debug::print_node(*root); }
    return root;
  }

  template <typename Input>
  up_node parse (Options & opt, Input & in) {
    using Mode = Options::Mode;
    switch (opt.mode) {
      case Mode::x86       : return parse<program>(opt, in);
      case Mode::run_tests : return parse<program>(opt, in);
    }
    assert(false && "parse: unreachable! Mode unrecognized.");
  }

  template <typename Input>
  int execute (Options & opt, Input & in) {
    up_node const root = parse(opt, in);
    if (Options::Mode::x86 == opt.mode) {
      node const & program = *root->children.at(0);
      std::vector<std::string> function_strings = {};
      for (up_node const & up_function : program.children) {
        node const & function = *up_function;
        auto variables_summary
          = analysis::IR::variables::summarize({ &function });
        node const & label      = *function.children.at(1);
        node const & parameters = *function.children.at(2);
        std::vector<std::string> untyped_parameter_strings;
        // drop types from parameters
        for (up_node const & up_parameter : parameters.children) {
          node & parameter = *up_parameter;
          node const & typed     = *parameter.children.at(0);
          node const & variable  = *typed.children.at(1);
          untyped_parameter_strings.push_back(variable.content());
          untyped_parameter_strings.push_back(",");
        }
        std::string untyped_parameters = helper::string::from_strings(
          untyped_parameter_strings
        );
        // drop type declarations,
        // transform length accesses,
        // transform new constructions,
        // transform indexing accesses,
        // transform if/else branches
        node const & blocks = *function.children.at(3);
        codegen::IR::result result = { std::move(variables_summary) };
        // NOTE(jordan): to trim instructions for better printing:
        ast::walk< ast::mutator::trim_content >(blocks.children);
        ast::walk< codegen::IR::translate >(blocks.children, result);
        std::string function_string = helper::string::from_strings({
          "define ", label.content(), " (", untyped_parameters, ") {\n",
            helper::string::from_strings(result.instructions),
          "}\n",
        });
        function_strings.push_back(function_string);
        up_node const fun_root = ast::L3::construct::from_string<
          grammar::L3::function::define
        >(function_string);
        /* ast::debug::print_node(*fun_root); */
      }
      std::ofstream out;
      out.open("prog.L3");
      for (auto function_string : function_strings)
        out << function_string;
      return 0;
    }
    if (Options::Mode::run_tests == opt.mode) {
      node const & program = *root->children.at(0);
      for (up_node const & function : program.children) {
        std::cerr
          << "function " << function->children.at(1)->content()
          << "\n----------------------------------------\n";
        auto vars = analysis::IR::variables::summarize({ &*function });
        // analysis::IR::variables::print(vars);
        auto arrays = analysis::IR::variables::arrays::summarize(vars);
        analysis::IR::variables::arrays::print(arrays);
      }
      return 0;
    }
    assert(false && "execute: unreachable! Mode unrecognized.");
  }
}
