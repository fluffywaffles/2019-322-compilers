#pragma once

#include "ast.h"
#include "grammar.h"
#include "analysis.h"
#include "helper.h"

namespace transform::LA {
  namespace collection = helper::collection;
  using node = ast::node;
  struct identify_names {
    static bool act (node &, analysis::LA::functions::result const &);
  };
}

bool transform::LA::identify_names::act (
  ast::node & n,
  analysis::LA::functions::result const & functions_summary
) {
  std::cout << n.name() << "\n";
  /*
   * EXPLANATION(jordan): skip over operands that have already been
   * identified as either a label or a variable. Also skip over labels
   * that existed in the LA program originally.
   */
  if (false
    || n.is<grammar::LA::operand::label>()
    || n.is<grammar::LA::operand::variable>()
  ) {
    return false;
  }
  /* EXPLANATION(jordan): in a function definition, always identify the
   * name of the function as a label.
   */
  if (n.is<grammar::LA::function::define>()) {
    // [0] type [1] name [2] parameters [3] instructions
    node & name_node = *n.children.at(1);
    name_node.realize();
    name_node.transform<
      grammar::LA::operand::name,
      grammar::LA::operand::label
    >(helper::string::from_strings({ ":", name_node.content() }));
    return true;
    /* NOTE(jordan): we can go ahead and recur into the children of the
     * function definition; the label will be caught by the above label
     * case and we won't accidentally revisit its name later.
     */
  }
  /*
   * EXPLANATION(jordan): any call instruction might be calling a function
   * by name, or by its value stored in a variable. Disambiguate using
   * outside context about existing function names, and turn function
   * names into labels instead of variables.
   */
  if (n.is<grammar::LA::expression::call::any>()) {
    // callee might be a function label, or might be a variable.
    node const & call = helper::unwrap_assert(n);
    node & callee = *call.children.at(0);
    if (collection::has(callee.content(), functions_summary.names)) {
      // the callee is a function name; make it into a label
      callee.realize();
      callee.transform<
        grammar::LA::operand::name,
        grammar::LA::operand::label
      >(helper::string::from_strings({ ":", callee.content() }));
    }
    return true; // keep going; we can find more operands in our arguments
    /* NOTE(jordan): we won't reach our new label's name because the label
     * case is handled separately above; when we recur into our new label
     * (the callee), it'll stop recurring.
     */
  }
  /*
   * EXPLANATION(jordan): a movable might be a function name; it can be
   * either a name, a number, or a label (i.e. a non-function label). If
   * it's a function name, then we should identify it as a label.
   * Otherwise, we can just keep recurring and handle it later.
   */
  if (n.is<grammar::LA::operand::movable>()) {
    // a movable might be a function label
    if (collection::has(n.content(), functions_summary.names)) {
      // the movable is a function name; make it into a label
      n.realize();
      n.transform<
        grammar::LA::operand::name,
        grammar::LA::operand::label
      >(helper::string::from_strings({ ":", n.content() }));
      return false; // don't recur into our new label's name
    }
    return true; // otherwise, recur into the original movable's child
  }
  /* EXPLANATION(jordan): assume a name is a variable if we haven't
   * already handled it specially above. We won't reach this case for the
   * names of labels, or previously-identified variables.
   */
  if (n.is<grammar::LA::operand::name>()) {
    n.realize();
    n.transform<
      grammar::LA::operand::name,
      grammar::LA::operand::variable
    >(helper::string::from_strings({ "%", n.content() }));
    return false; // don't recur into our new variable's name
  }
  return true;
}
