#pragma once
#include <cassert>
#include <iostream>
#include <fstream>

#include "tao/pegtl.hpp"

#include "driver/options.h"
#include "ast.h"
#include "grammar.h"
#include "analysis.h"
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

  // variable generators
  std::string gen_pointer_variable (
    node const & array_variable,
    std::string index_string,
    std::string pointer_type = "index",
    std::string object_type  = "array"
  ) {
    return helper::string::from_strings({
      "%", object_type,
      "_", helper::L3::strip_variable_prefix(array_variable.content()),
      "_", pointer_type,
      "_", index_string,
    });
  }
  std::string gen_pointer_variable (
    node const & array_variable,
    node const & index_variable,
    std::string pointer_type = "index",
    std::string object_type  = "array"
  ) {
    return gen_pointer_variable(
      array_variable,
      helper::L3::strip_variable_prefix(index_variable.content()),
      pointer_type,
      object_type
    );
  }
  std::string gen_pointer_variable (
    node const & array_variable,
    int byte_offset,
    std::string pointer_type = "index",
    std::string object_type  = "array"
  ) {
    return gen_pointer_variable(
      array_variable,
      std::to_string(byte_offset),
      pointer_type,
      object_type
    );
  }
  std::string gen_dimension_variable (
    node const & array_variable,
    node const & dimension,
    int dimension_index
  ) {
    return helper::string::from_strings({
      "%", helper::L3::strip_variable_prefix(array_variable.content()),
      "_dimension_",
      helper::L3::strip_variable_prefix(dimension.content()),
      "_dimidx_", std::to_string(dimension_index),
    });
  }
  struct translation_result {
    analysis::IR::variables::result variables_summary;
    std::vector<std::string> instructions;
  };
  struct translate_instructions {
    static bool compute (node const & i, translation_result & result) {
      namespace view = helper::view;
      using namespace grammar::IR::instruction;
      auto const & variables_summary = result.variables_summary;
      auto const & declarations = variables_summary.declaration;
      auto & instructions       = result.instructions;
      // skip type declarations
      if (i.is<declare::variable>()) {
        return false;
      }
      // transform array instructions
      if (i.is<assign::variable::gets_new>()) {
        node const & variable  = *i.children.at(0);
        /* node const & gets      = *i.children.at(1); */
        node const & any_make  = *i.children.at(2);
        node const & make      = helper::unwrap_assert(any_make);
        node const & object    = *make.children.at(0);
        node const & arguments = *make.children.at(1);
        /* ast::debug::print_node(make); */
        if (object.is<grammar::IR::literal::object::array>()) {
          view::vec<node> dimensions = {};
          for (up_node const & argument : arguments.children)
            dimensions.push_back(&helper::unwrap_assert(*argument));
          int num_dimensions = dimensions.size();
          int num_dimensions_encoded = (num_dimensions << 1) + 1;
          /*
           * | total size | # dimensions | dimension size | ... | data ...
           */
          // prepare total size; initialize it to 1
          std::string size_variable = helper::string::from_strings({
            "%", helper::L3::strip_variable_prefix(variable.content()),
            "_total_size",
          });
          helper::collection::append(instructions, {
            size_variable, " <- 1\n",
          });
          // decode each dimension and multiply it into the total size
          for (int index = 0; index < dimensions.size(); index++) {
            node const & dimension = *dimensions.at(index);
            // %<variable:name>_<dimension>_dimension_<index>
            std::string dim_variable = gen_dimension_variable(
              variable,
              dimension,
              index
            );
            helper::collection::append(instructions, {
              dim_variable,
                " <- ", dimension.content(), " >> 1\n",
              size_variable,
                " <- ", size_variable, " * ", dim_variable, "\n",
            });
          }
          // add extra header data to size
          int header_size = num_dimensions + 1; // #dimensions + each size
          helper::collection::append(instructions, {
            size_variable,
              " <- ", size_variable,
              " + ", std::to_string(header_size),
              "\n",
          });
          // encode total size
          std::string size_encoded_variable = size_variable + "_encoded";
          helper::collection::append(instructions, {
            size_encoded_variable,
              " <- ", size_variable, " << 1",
              "\n",
            size_encoded_variable,
              " <- ", size_encoded_variable, " + 1",
              "\n",
          });
          // allocate the array
          helper::collection::append(instructions, {
            variable.content(),
              " <- call allocate(", size_encoded_variable, ", 1)",
              "\n",
          });
          // store the number of dimensions in the header
          std::string num_dimensions_variable
            = gen_pointer_variable(variable, 1, "header");
          helper::collection::append(instructions, {
            num_dimensions_variable, " <- ", variable.content(), " + 8",
            "\n",
            "store ", num_dimensions_variable,
              " <- ", std::to_string(num_dimensions_encoded),
              "\n",
          });
          // store each dimension size in the header (starts from 2)
          for (int index = 2; (index - 2) < dimensions.size(); index++) {
            int bytes = 8 * index;
            node const & dimension = *dimensions.at(index - 2);
            std::string header_dim_variable
              = gen_pointer_variable(variable, index, "header");
            helper::collection::append(instructions, {
              header_dim_variable,
                " <- ", variable.content(),
                " + ", std::to_string(bytes),
                "\n",
              "store ", header_dim_variable,
                " <- ", dimension.content(),
                "\n",
            });
          }
          return false;
        }
        if (object.is<grammar::IR::literal::object::tuple>()) {
          node const & argument = helper::unwrap_assert(arguments);
          node const & size     = helper::unwrap_assert(argument);
          helper::collection::append(instructions, {
            variable.content(),
              " <- call allocate (", size.content(), ", 1)",
              "\n",
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
        /* ast::debug::print_node(access); */
        assert(helper::collection::has(
          array.content(),
          variables_summary.variables
        ));
        auto const * array_variable = &*helper::collection::find(
          array.content(),
          variables_summary.variables
        );
        auto const & declaration = declarations.at(array_variable);
        node const & typed = declaration->typed_operand;
        // must unwrap the actual type from its  'any type' wrapper
        node const & any_type = *typed.children.at(0);
        node const & type     = helper::unwrap_assert(any_type);
        if (type.is<grammar::IR::literal::type::multiarray::any>()) {
          node const & array_type = helper::unwrap_assert(type);
          node const & type_dimensions = *array_type.children.at(1);
          assert(
            type_dimensions.is<
              grammar::IR::meta::literal::type::multiarray::dimensions
            >()
          );
          int num_dimensions = type_dimensions.children.size();
          std::vector<std::string> index_strings = {};
          for (up_node const & up_accessor : accessors.children) {
            node const & index_node = helper::unwrap_assert(*up_accessor);
            index_strings.push_back(
              helper::L3::strip_variable_prefix(index_node.content())
            );
          }
          std::string index_variable = gen_pointer_variable(
            array,
            helper::string::from_strings(index_strings, "_")
          );
          // Prepare the index!
          helper::collection::append(instructions, {
            index_variable, " <- 0", "\n",
          });
          for (
            int index_index = 0;
            index_index < accessors.children.size();
            index_index++
          ) {
            node const & accessor   = *accessors.children.at(index_index);
            node const & index_node = helper::unwrap_assert(accessor);
            // if this is the 1st index, skip multiplying 0 by a dimension
            if (index_index > 0) {
              // multiply index-so-far by the size of its dimension
              std::string dim_size_variable
                = gen_pointer_variable(array, index_index, "dimension");
              helper::collection::append(instructions, {
                // ptr: array + offset
                dim_size_variable,
                  " <- ", std::to_string((index_index + 2) * 8),
                  " + ", array.content(),
                  "\n",
                // load that address
                dim_size_variable,
                  " <- load ", dim_size_variable,
                  "\n",
                // decode the size
                dim_size_variable,
                  " <- ", dim_size_variable, " >> 1",
                  "\n",
                // and then multiply by it
                index_variable,
                  " <- ", index_variable,
                  " * ", dim_size_variable,
                  "\n",
              });
            }
            // now add the next index
            helper::collection::append(instructions, {
              index_variable,
                " <- ", index_variable,
                " + ", index_node.content(),
                "\n",
            });
          }
          // Now, finally, offset the index from the array and do a store.
          helper::collection::append(instructions, {
            // offset *= dimension_size
            index_variable,
              " <- ", index_variable, " * 8",
              "\n",
            // skip over the header cells (including size + # dimensions)
            index_variable,
              " <- ", index_variable,
              " + ", std::to_string((num_dimensions + 2) * 8),
              "\n",
            // add the array base pointer
            index_variable,
              " <- ", index_variable, " + ", array.content(),
              "\n",
            // load that address into <variable>
            variable.content(),
              " <- load ", index_variable,
              "\n",
          });
          return false;
        }
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
        node const & array     = *length.children.at(0);
        node const & dim_index = *length.children.at(1);
        /* ast::debug::print_node(length); */
        assert(helper::collection::has(
          array.content(),
          variables_summary.variables
        ));
        auto const * array_variable = &*helper::collection::find(
          array.content(),
          variables_summary.variables
        );
        auto const & declaration = declarations.at(array_variable);
        node const & typed    = declaration->typed_operand;
        node const & any_type = *typed.children.at(0);
        node const & type     = helper::unwrap_assert(any_type);
        if (type.is<grammar::IR::literal::type::multiarray::any>()) {
          // get a pointer address to: %<array> + 8 * (%<dim_index> + 2)
          std::string index_variable
            = gen_pointer_variable(array, dim_index, "dimension");
          helper::collection::append(instructions, {
            // header cell: 2 + dim_index (2, 3, 4, ...)
            index_variable,
              " <- ", dim_index.content(), " + 2",
              "\n",
            // offset: header cell index * 8
            index_variable,
              " <- ", index_variable, " * 8",
              "\n",
            // ptr: array + offset
            index_variable,
              " <- ", index_variable, " + ", array.content(),
              "\n",
            // load that address into <variable>
            variable.content(),
              " <- load ", index_variable,
              "\n",
          });
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
        node const & array     = *access.children.at(0);
        node const & accessors = *access.children.at(1);
        /* ast::debug::print_node(access); */
        assert(helper::collection::has(
          array.content(),
          variables_summary.variables
        ));
        auto const * array_variable = &*helper::collection::find(
          array.content(),
          variables_summary.variables
        );
        auto const & declaration = declarations.at(array_variable);
        node const & typed    = declaration->typed_operand;
        node const & any_type = *typed.children.at(0);
        node const & type     = helper::unwrap_assert(any_type);
        if (type.is<grammar::IR::literal::type::tuple_>()) {
          node const & accessor = helper::unwrap_assert(accessors);
          node const & index_node = helper::unwrap_assert(accessor);
          auto tuple_index_variable = gen_pointer_variable(
            array,
            index_node,
            "index",
            "tuple"
          );
          helper::collection::append(instructions, {
            // Get the index variable and add 1
            tuple_index_variable,
              " <- ", index_node.content(), " + 1",
              "\n",
            // Multiply by 8 to obtain the pointer offset
            tuple_index_variable,
              " <- ", tuple_index_variable, " * 8",
              "\n",
            // Add the array base pointer
            tuple_index_variable,
              " <- ", tuple_index_variable,
              " + ", array.content(),
              "\n",
            // Store the value
            "store ", tuple_index_variable,
              " <- ", variable.content(),
              "\n",
          });
          return false;
        }
        if (type.is<grammar::IR::literal::type::multiarray::any>()) {
          node const & array_type = helper::unwrap_assert(type);
          node const & type_dimensions = *array_type.children.at(1);
          assert(
            type_dimensions.is<
              grammar::IR::meta::literal::type::multiarray::dimensions
            >()
          );
          int num_dimensions = type_dimensions.children.size();
          std::vector<std::string> index_strings = {};
          for (up_node const & up_accessor : accessors.children) {
            node const & index_node = helper::unwrap_assert(*up_accessor);
            index_strings.push_back(
              helper::L3::strip_variable_prefix(index_node.content())
            );
          }
          std::string index_variable = gen_pointer_variable(
            array,
            helper::string::from_strings(index_strings, "_")
          );
          // Prepare the index!
          helper::collection::append(instructions, {
            index_variable, " <- 0", "\n",
          });
          for (
            int index_index = 0;
            index_index < accessors.children.size();
            index_index++
          ) {
            node const & accessor   = *accessors.children.at(index_index);
            node const & index_node = helper::unwrap_assert(accessor);
            // if this is the 1st index, skip multiplying 0 by a dimension
            if (index_index > 0) {
              // multiply index-so-far by the size of its dimension
              std::string dim_size_variable
                = gen_pointer_variable(array, index_index, "dimension");
              helper::collection::append(instructions, {
                // ptr: array + offset
                dim_size_variable,
                  " <- ", std::to_string((index_index + 2) * 8),
                  " + ", array.content(),
                  "\n",
                // load that address
                dim_size_variable,
                  " <- load ", dim_size_variable,
                  "\n",
                // decode the size
                dim_size_variable,
                  " <- ", dim_size_variable, " >> 1",
                  "\n",
                // and then multiply by it
                index_variable,
                  " <- ", index_variable,
                  " * ", dim_size_variable,
                  "\n",
              });
            }
            // now add the next index
            helper::collection::append(instructions, {
              index_variable,
                " <- ", index_variable,
                " + ", index_node.content(),
                "\n",
            });
          }
          // Now, finally, offset the index from the array and do a store.
          helper::collection::append(instructions, {
            // offset *= dimension_size
            index_variable,
              " <- ", index_variable, " * 8",
              "\n",
            // skip over the header cells (including size + # dimensions)
            index_variable,
              " <- ", index_variable,
              " + ", std::to_string((num_dimensions + 2) * 8),
              "\n",
            // add the array base pointer
            index_variable,
              " <- ", index_variable, " + ", array.content(),
              "\n",
            // store <variable> into that address
            "store ", index_variable,
              " <- ", variable.content(),
              "\n",
          });
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
        || i.is<ret::value>()
        || i.is<ret::nothing>()
        || i.is<define::label>()
        || i.is<branch::unconditional>()
        || i.is<assign::variable::gets_shift>()
        || i.is<assign::variable::gets_call>()
        || i.is<assign::variable::gets_movable>()
        || i.is<assign::variable::gets_comparison>()
        || i.is<assign::variable::gets_arithmetic>()
      ) {
        instructions.push_back(helper::string::trim(i.content()));
        instructions.push_back("\n");
        return false;
      } else {
        return true;
      }
    }
  };

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
        translation_result result = { std::move(variables_summary) };
        ast::walk< translate_instructions >(blocks.children, result);
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
