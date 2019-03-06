#pragma once

#include "L3/helper.h"

#include "ast.h"

namespace helper::IR::variable {
  using node = ast::node;
  // variable generators
  std::string gen_pointer (
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
  std::string gen_pointer (
    node const & array_variable,
    node const & index_variable,
    std::string pointer_type = "index",
    std::string object_type  = "array"
  ) {
    return gen_pointer(
      array_variable,
      helper::L3::strip_variable_prefix(index_variable.content()),
      pointer_type,
      object_type
    );
  }
  std::string gen_pointer (
    node const & array_variable,
    int byte_offset,
    std::string pointer_type = "index",
    std::string object_type  = "array"
  ) {
    return gen_pointer(
      array_variable,
      std::to_string(byte_offset),
      pointer_type,
      object_type
    );
  }
  std::string gen_dimension (
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
}
