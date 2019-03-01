#pragma once
#include <cassert>
#include <iostream>
#include <fstream>

#include "tao/pegtl.hpp"

#include "driver/options.h"
#include "grammar.h"
#include "ast.h"
#include "analysis.h"

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
          /* parameter.realize(); */
          /* parameter.transform< */
          /*   grammar::IR::operand::list::parameter, */
          /*   grammar::L3::operand::list::parameter */
          /* >(variable.content()); */
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
        struct translation_result {
          analysis::IR::variables::result variables_summary;
          std::vector<std::string> instructions;
        };
        struct translate_instructions {
          static bool compute (node & i, translation_result & result) {
            using namespace grammar::IR::instruction;
            auto & variables_summary = result.variables_summary;
            auto & instructions      = result.instructions;
            // skip type declarations
            if (i.is<declare::variable>()) {
              return false;
            }
            // transform array instructions
            if (i.is<assign::variable::gets_new>()) {
              node const & variable  = *i.children.at(0);
              /* node const & gets      = *i.children.at(1); */
              node const & make      = helper::unwrap_assert(*i.children.at(2));
              node const & object    = *make.children.at(0);
              node const & arguments = *make.children.at(1);
              if (object.is<grammar::IR::literal::object::array>()) {
                // TODO
                return false;
              }
              if (object.is<grammar::IR::literal::object::tuple>()) {
                node const & argument = helper::unwrap_assert(arguments);
                node const & size     = helper::unwrap_assert(argument);
                helper::collection::append(instructions, {
                  variable.content(),
                  " <- call allocate (", size.content(), ", 1)\n",
                });
                return false;
              }
              std::cerr
                << "'new' invoked for unrecognized object"
                << " " << object.content() << "\n";
              assert(false && "tried to construct unrecognized object");
            }
            if (i.is<assign::variable::gets_index>()) {
              node const & variable  = *i.children.at(0);
              /* node const & gets      = *i.children.at(1); */
              node const & access    = *i.children.at(2);
              node const & array     = *access.children.at(0);
              node const & accessors = *access.children.at(1);
              assert(helper::collection::has(array.content(), variables_summary.variables));
              auto const * array_variable = &*helper::collection::find(
                array.content(),
                variables_summary.variables
              );
              node const * declaration
                = variables_summary.declarations.at(array_variable);
              assert(declaration->is<declare::variable>());
              ast::L3::debug::print_node(*declaration);
              // the 'any type' wrapper
              node const & any_type = *declaration->children.at(0);
              // the actual fucking type
              node const & type     = helper::unwrap_assert(any_type);
              if (type.is<grammar::IR::literal::type::tuple_>()) {
                node const & accessor = helper::unwrap_assert(accessors);
                node const & index_node = helper::unwrap_assert(accessor);
                helper::collection::append(instructions, {
                  // Get the index variable and add 1
                  variable.content(),
                    " <- ", index_node.content(), " + 1",
                    "\n",
                  // Multiply by 8 to obtain the pointer offset
                  variable.content(),
                    " <- ", variable.content(), " * 8",
                    "\n",
                  // Add the array base pointer
                  variable.content(),
                    " <- ", variable.content(),
                    " + ", array.content(),
                    "\n",
                  // Load the offset pointer
                  variable.content(),
                    " <- load ", variable.content(),
                    "\n",
                });
                return false;
              }
              if (type.is<grammar::IR::literal::type::multiarray::any>()) {
                // TODO
                return false;
              }
              std::cerr
                << "indexed into invalid (or unhandled) type"
                << " " << type.content()
                << " (" << *array_variable << ")\n";
              assert(false && "tried to access an index of an invalid type");
            }
            if (i.is<assign::variable::gets_length>()) {
              /* NOTE(jordan): only applicable for multiarrays (which have
               * dimensions, hence the 2nd argument to the length
               * expression).
               */
              node const & variable  = *i.children.at(0);
              /* node const & gets      = *i.children.at(1); */
              node const & length    = *i.children.at(2);
              ast::L3::debug::print_node(length);
              node const & array     = *length.children.at(0);
              node const & dimension = *length.children.at(1);
              assert(helper::collection::has(array.content(), variables_summary.variables));
              auto const * array_variable = &*helper::collection::find(
                array.content(),
                variables_summary.variables
              );
              node const * declaration
                = variables_summary.declarations.at(array_variable);
              assert(declaration->is<declare::variable>());
              // the 'any type' wrapper
              node const & any_type = *declaration->children.at(0);
              // the actual fucking type
              node const & type     = helper::unwrap_assert(any_type);
              if (type.is<grammar::IR::literal::type::multiarray::any>()) {
                // TODO
                return false;
              }
              std::cerr
                << "tried to get length of invalid (or unhandled) type"
                << " " << type.content()
                << " (" << *array_variable << ")\n";
              assert(false && "tried to get length of non-multiarray");
            }
            if (i.is<assign::array::gets_movable>()) {
              node const & access    = *i.children.at(0);
              /* node const & gets      = *i.children.at(1); */
              node const & variable  = *i.children.at(2);
              ast::L3::debug::print_node(access);
              node const & array     = *access.children.at(0);
              node const & accessors = *access.children.at(1);
              assert(helper::collection::has(array.content(), variables_summary.variables));
              auto const * array_variable = &*helper::collection::find(
                array.content(),
                variables_summary.variables
              );
              node const * declaration
                = variables_summary.declarations.at(array_variable);
              assert(declaration->is<declare::variable>());
              ast::L3::debug::print_node(*declaration);
              // the 'any type' wrapper
              node const & any_type = *declaration->children.at(0);
              // the actual fucking type
              node const & type     = helper::unwrap_assert(any_type);
              if (type.is<grammar::IR::literal::type::tuple_>()) {
                node const & accessor = helper::unwrap_assert(accessors);
                node const & index_node = helper::unwrap_assert(accessor);
                auto temporary_variable = helper::string::from_strings({
                  "%tuple_index_", index_node.content(), "_ptr",
                });
                helper::collection::append(instructions, {
                  // Get the index variable and add 1
                  temporary_variable,
                    " <- ", index_node.content(), " + 1",
                    "\n",
                  // Multiply by 8 to obtain the pointer offset
                  temporary_variable,
                    " <- ", temporary_variable, " * 8",
                    "\n",
                  // Add the array base pointer
                  temporary_variable,
                    " <- ", temporary_variable,
                    " + ", array.content(),
                    "\n",
                  // Store the value
                  "store ", temporary_variable,
                    " <- ", variable.content(),
                    "\n",
                });
                return false;
              }
              if (type.is<grammar::IR::literal::type::multiarray::any>()) {
                // TODO
                return false;
              }
              std::cerr
                << "indexed into invalid (or unhandled) type"
                << " " << type.content()
                << " (" << *array_variable << ")\n";
              assert(false && "tried to access an index of an invalid type");
            }
            // transform if/else to when + unconditional
            if (i.is<branch::if_else>()) {
              node const & variable = *i.children.at(0);
              node const & then_lbl = *i.children.at(1);
              node const & else_lbl = *i.children.at(2);
              helper::collection::append(instructions, {
                "br ", variable.content(), " ", then_lbl.content(), "\n",
                "br ", else_lbl.content(), "\n",
              });
              return false;
            }
            // do nothing for all other instructions
            if (false
              || i.is<call>()
              || i.is<define::label>()
              || i.is<branch::unconditional>()
              || i.is<assign::variable::gets_shift>()
              || i.is<assign::variable::gets_call>()
              || i.is<assign::variable::gets_movable>()
              || i.is<assign::variable::gets_comparison>()
              || i.is<assign::variable::gets_arithmetic>()
            ) {
              instructions.push_back(i.content());
              instructions.push_back("\n");
              return false;
            } else {
              return true;
            }
          }
        };
        translation_result result = { std::move(variables_summary) };
        ast::walk<translate_instructions>(blocks.children, result);
        auto fun_root = helper::make_L3<grammar::L3::function::define>({
          "define ", label.content(), " (", untyped_parameters, ") {\n",
            helper::string::from_strings(result.instructions),
          "}\n",
        });
        ast::L3::debug::print_node(*fun_root);
        /* function.realize(); */
        /* function.transform< */
        /*   grammar::IR::function::define, */
        /*   grammar::L3::function::define */
        /* >(helper::string::from_strings({ */
        /*   "define ", label.content(), " (", untyped_parameters, ") {\n", */
        /*     helper::string::from_strings(instructions), */
        /*   "}\n", */
        /* })); */
      }
      return 0;
    }
    if (Options::Mode::run_tests == opt.mode) {
      node const & program = *root->children.at(0);
      for (up_node const & function : program.children) {
        std::cerr
          << "function " << function->children.at(1)->content()
          << "\n----------------------------------------\n";
        auto vars = analysis::IR::variables::summarize({ &*function });
        analysis::IR::variables::print(vars);
      }
      return 0;
    }
    assert(false && "execute: unreachable! Mode unrecognized.");
  }
}
