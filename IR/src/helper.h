#pragma once

#include "tao/pegtl.hpp"

#include "ast.h"
#include "L3/helper.h"

namespace helper {
  template <typename Entry>
  auto make_L3 = ast::L3::construct::from_strings<peg::must<Entry>>;
}
