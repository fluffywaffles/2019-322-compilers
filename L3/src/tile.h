#pragma once

#include <tuple>
#include <typeinfo>
#include <algorithm>

#include "L2/transform.h"

#include "helper.h"
#include "grammar.h"
#include "ast.h"

namespace grammar::L2 {
  struct instruction_sequence : peg::plus<instruction::any> {};
  struct instructions : peg::plus<instruction_sequence> {};
}

namespace tile {
  namespace view = helper::view;
  using node     = ast::node;
  using up_node  = helper::L3::up_node;
  using up_nodes = helper::L3::up_nodes;
  // TODO(jordan): calm.
  namespace { // FIXME(jordan): shhhhhhhhh
    template <size_t I, typename Tuple>
      using get = std::tuple_element<I, Tuple>;
    template <size_t I, typename Tuple>
      struct index_ok {
        static const bool value = I < std::tuple_size<Tuple>::value;
      };
    template <size_t I, typename Tuple>
      struct safe_index {
        static const size_t value = index_ok<I, Tuple>::value ? I : 0;
      };
    template <size_t I, typename Tuple> // TODO(jordan): tut-tut; hush now
      using rule_at = std::conditional<
        index_ok<I, Tuple>::value,
        typename get<safe_index<I, Tuple>::value, Tuple>::type,
        peg::failure
      >;
  }
  template <typename... Rules>
  struct match {
    static const bool accept (
      view::vec<node> const & window,
      bool debug = false
    ) {
      return match<std::tuple<Rules...>>::accept(window, debug);
    }
  };
  template <typename... Rules>
  struct match <std::tuple<Rules...>> {
    using RulesTuple = std::tuple<Rules...>;
    static const int match_size = std::tuple_size<RulesTuple>::value;
    static const bool accept (
      view::vec<node> const & window,
      bool debug = false
    ) {
      assert(true
        && match_size == window.size()
        && "you done fucked up now, this should've failed at compile-time"
      );
      bool result = true;
      if (debug) {
        std::cerr << "» tile::match<Tile>::accept...\n";
      }
      #define match_rule(I)                                              \
        do {                                                             \
          if (I < match_size) {                                          \
            using rule_i = typename rule_at<I,RulesTuple>::type;         \
            bool const matches_i = window.at(I)->is<rule_i>();           \
            if (debug) {                                                 \
              auto const * info = &typeid(rule_i);                       \
              std::cerr                                                  \
                << "» " << (matches_i ? "SXS " : "FAIL")                 \
                << " tile::match::accept while matching node"            \
                << " " << I                                              \
                << " (" << window.at(I)->name() << ")"                   \
                << " against rule"                                       \
                << " " << peg::internal::demangle(info->name())          \
                << "\n";                                                 \
            }                                                            \
            result &= matches_i;                                         \
          }                                                              \
        } while (false)
      /* NOTE(jordan): we could trivially extend this past 15 rules, but
       * that doesn't seem necessary. 15-instruction windows are big
       * enough for everything I think. Probably way more than big enough.
       */
      match_rule( 0); match_rule( 1); match_rule( 2); match_rule( 3);
      match_rule( 4); match_rule( 5); match_rule( 6); match_rule( 7);
      match_rule( 8); match_rule( 9); match_rule(10); match_rule(11);
      match_rule(12); match_rule(13); match_rule(14); match_rule(15);
      #undef match_rule
      if (debug) {
        std::cerr
          << "» " << (result ? "SXS " : "FAIL")
          << " tile::match<Tile>::accept\n";
      }
      return result;
    }
  };
  template <typename Base, typename Specializer>
  struct specialize {
    static const bool accept (
      view::vec<node> const & window,
      bool debug = false
    ) {
      bool base_accepted = Base::accept(window, debug);
      if (!base_accepted) return false;
      bool specializer_accepted = Specializer::accept(window);
      if (debug) {
        auto const * info = &typeid(Specializer);
        std::cerr
          << "» " << (specializer_accepted ? "SXS " : "FAIL")
          << " tile::spclz::accept against"
          << " " << peg::internal::demangle(info->name())
          << "\n";
      }
      return base_accepted && specializer_accepted;
    }
  };
}

namespace tile::registry {
  template <typename T> struct depends : std::false_type {};
  /* EXPLANATION(jordan): We specialize registry::generator for every tile
   * that, when matched, is expected to generate some code. The generator
   * registry should only be explored, and its generators invoked, by the
   * general-purpose generator::check_generate wrapper method.
   */
  template <typename MatchedTile>
  struct generator {
    static up_node generate (view::vec<node> const & matched) {
      /* NOTE(jordan): This static_assert generates compiler errors for
       * missing generators. This way, you cannot compile if a tile claims
       * to be a 'generates' tile and you call 'generate', but a generator
       * is not registered.
       */
      static_assert(
        depends<MatchedTile>::value,
        "registry::generator no generator is registered for this tile!"
      );
      return {};
    };
  };
}

namespace tile {
  template <typename MatchedTile, typename Expect, bool can_expect = true>
  struct generates {
    /* NOTE(jordan): only allow generate to be invoked if a generator is
     * registered for the matched tile.
     */
    static const int generated_size = std::tuple_size<Expect>::value;
    static up_node generate (
      view::vec<node> const & matched,
      bool debug = false
    ) {
      using generator = tile::registry::generator<MatchedTile>;
      up_node root    = std::move(generator::generate(matched));
      view::vec<node> result_view;
      for (up_node const & instruction_wrapper : root->children) {
        assert(instruction_wrapper->is<grammar::L2::instruction::any>());
        node const & actual_instruction
          = helper::L3::unwrap_assert(*instruction_wrapper);
        result_view.push_back(&actual_instruction);
      }
      if (debug) std::cerr << "» tile::generator::check_generate...\n";
      if (can_expect) {
        assert(tile::match<Expect>::accept(result_view, debug));
        if (debug) std::cerr << "» SXS  tile::generator::check_generate\n";
      } else {
        if (debug)
          std::cerr
            << "» SKIP tile::generator::check_generate:"
            << " variable-size output\n";
      }
      return root;
    }
  };
}

namespace tile::L3 {
  namespace L3 = grammar::L3;
  namespace L2 = grammar::L2;
}

namespace tile::L3::ret {
  /* void return
   * ?
   *  return
   * →
   *  return
   */
  struct nothing
    : tile::match<L3::instruction::ret::nothing>
    , tile::generates<nothing, std::tuple<
        L2::instruction::invoke::ret
      >>
    {};
  /* value return
   * ?
   *  return <value>
   * →
   *  rax <- <value>
   *  return
   */
  struct value
    : tile::match<L3::instruction::ret::value>
    , tile::generates<value, std::tuple<
        L2::instruction::assign::assignable::gets_movable,
        L2::instruction::invoke::ret
      >>
    {};
}

namespace tile::L3::assign::address {
  /* store
   * ?
   *  store <variable> <- <movable>
   * →
   *  mem <variable> 0 <- <movable>
   */
  struct gets_movable
    : tile::match<L3::instruction::assign::address::gets_movable>
    , tile::generates<gets_movable, std::tuple<
        L2::instruction::assign::relative::gets_movable
      >>
    {};
}

namespace tile::L3::call {
  /* call
   * ?
   *  call <callee> <arguments>
   * →
   *  ! no generators for call instruction
   *  ! must match one of its specializations
   */
  struct any : tile::match<L3::instruction::call> {};
}

namespace tile::L3::specializers::call {
  namespace literal_intrinsic = grammar::L3::literal::intrinsic;
  template <typename IntrinsicLiteral>
  struct intrinsic {
    static const bool accept (view::vec<node> const & window) {
      node const & call   = helper::L3::unwrap_assert(*window.at(0));
      node const & callee = *call.children.at(0);
      return call.is<grammar::L3::expression::call::intrinsic>()
          && helper::matches<IntrinsicLiteral>(callee);
    }
  };
  struct defined {
    static const bool accept (view::vec<node> const & window) {
      node const & call = helper::L3::unwrap_assert(*window.at(0));
      return call.is<grammar::L3::expression::call::defined>();
    }
  };
}

namespace tile::L3::call {
  /* call defined
   * ?
   *  call <callable> <arguments>
   * →
   *  mem rsp -8 <- <return_label>
   *  (... implement calling convention ...)
   *  call <callable> <arity>
   *  <return_label>
   */
  struct defined
    : tile::specialize<call::any, specializers::call::defined>
    , tile::generates<defined, std::tuple<
      L2::instruction::assign::relative::gets_movable, // mem rsp -8 <- :r
      L2::instructions,                                // arguments...
      L2::instruction::invoke::call::callable,         // call <c> <arity>
      L2::instruction::define::label                   // :r
      >, false> // NOTE(jordan): cannot be checked; variable output
    {};
}

namespace tile::L3::call::intrinsic {
  namespace literal = L3::literal::intrinsic;
  /* call print
   * ?
   *  call print (<argument>)
   * →
   *  mem rsp -8 <- <return_label>
   *  rdi <- <arg0>
   *  call print 1
   *  <return_label>
   */
  struct print
    : tile::specialize<
        call::any,
        specializers::call::intrinsic<literal::print>
      >
    , tile::generates<print, std::tuple<
        L2::instruction::assign::assignable::gets_movable, // rdi <- <0>
        L2::instruction::invoke::call::intrinsic::print
      >>
    {};
  /* call allocate
   * ?
   *  call allocate (<argument0>, <argument1>)
   * →
   *  mem rsp -8 <- <return_label>
   *  rdi <- <arg0>
   *  rsi <- <arg1>
   *  call allocate 2
   *  <return_label>
   */
  struct allocate
    : tile::specialize<
        call::any,
        specializers::call::intrinsic<literal::allocate>
      >
    , tile::generates<allocate, std::tuple<
        L2::instruction::assign::assignable::gets_movable, // rdi <- <0>
        L2::instruction::assign::assignable::gets_movable, // rsi <- <1>
        L2::instruction::invoke::call::intrinsic::allocate
      >>
    {};
  /* call array_error
   * ?
   *  call array_error (<argument0>, <argument1>)
   * →
   *  mem rsp -8 <- <return_label>
   *  rdi <- <arg0>
   *  rsi <- <arg1>
   *  call array_error 2
   *  <return_label>
   */
  struct array_error
    : tile::specialize<
        call::any,
        specializers::call::intrinsic<literal::array_error>
      >
    , tile::generates<array_error, std::tuple<
        L2::instruction::assign::assignable::gets_movable, // rdi <- <0>
        L2::instruction::assign::assignable::gets_movable, // rsi <- <1>
        L2::instruction::invoke::call::intrinsic::array_error
      >>
    {};
}

namespace tile::L3::specializers::shift {
  struct number {
    static const bool accept (view::vec<node> const & window) {
      node const & n     = *window.at(0);
      node const & shift = *n.children.at(2);
      node const & rhs   = *shift.children.at(2);
      return helper::matches<L3::operand::number>(rhs);
    }
  };
  struct variable {
    static const bool accept (view::vec<node> const & window) {
      node const & n     = *window.at(0);
      node const & shift = *n.children.at(2);
      node const & rhs   = *shift.children.at(2);
      return helper::matches<L3::operand::variable>(rhs);
    }
  };
}

namespace tile::L3::assign::variable {
  /* variable loads from memory
   * ?
   *   <variable> <- load <variable>
   * →
   *   <variable> <- mem <variable> 0
   */
  struct gets_load
    : tile::match<L3::instruction::assign::variable::gets_load>
    , tile::generates<gets_load, std::tuple<
        L2::instruction::assign::assignable::gets_relative
      >>
    {};
  /* call function and save result
   * ?
   *   <variable> <- call <callee> <arguments>
   * →
   *   mem rsp -8 <- <return_label>
   *   (... implement calling convention...)
   *   call <callable> <arity>
   *   <return_label>
   *   <variable> <- rax
   */
  struct gets_call
    : tile::match<L3::instruction::assign::variable::gets_call>
    , tile::generates<gets_call, std::tuple<
        L2::instruction::assign::relative::gets_movable, // mem rsp -8 <- :r
        L2::instructions,                                // arguments...
        L2::instruction::invoke::call::callable,         // call <c> <arity>
        L2::instruction::define::label,                  // :r
        L2::instruction::assign::assignable::gets_movable
      >, false> // NOTE(jordan): cannot be checked; variable output
    {};
  /* save the result of some arithmetic
   * ?
   *  <variable> <- <value> <L3::arith_op> <value>
   * →
   *  <variable> <- <value>
   *  <variable> <L2::arith_op> <value>
   */
  struct gets_arithmetic
    : tile::match<L3::instruction::assign::variable::gets_arithmetic>
    , tile::generates<gets_arithmetic, std::tuple<
        L2::instruction::assign::assignable::gets_movable,
        L2::instruction::update::assignable::arithmetic::comparable
      >>
    {};
  /* save the result of a shift
   * ?
   *  <variable> <- <value> <L3::shift_op> <value>
   * →
   *  ! no generators for gets_shift instruction
   *  ! must match one of its specializations
   */
  struct gets_shift
    : tile::match<L3::instruction::assign::variable::gets_shift> {};
  namespace shift {
    namespace specializers = tile::L3::specializers::shift;
    /* save the result of a shift
     * ?
     *  <variable> <- <value> <L3::shift_op> <number>
     * →
     *  <variable> <- <value>
     *  <variable> <L2::shift_op> <number>
     */
    struct number
      : tile::specialize<gets_shift, specializers::number>
      , tile::generates<number, std::tuple<
          L2::instruction::assign::assignable::gets_movable,
          L2::instruction::update::assignable::shift::number
        >>
      {};
    /* save the result of a shift
     * ?
     *  <variable> <- <value> <L3::shift_op> <variable>
     * →
     *  <variable> <- <value>
     *  <variable> <L2::shift_op> <variable>
     */
    struct variable
      : tile::specialize<gets_shift, specializers::variable>
      , tile::generates<variable, std::tuple<
          L2::instruction::assign::assignable::gets_movable,
          L2::instruction::update::assignable::shift::shift
        >>
      {};
  }
  /* save the result of a comparison
   * ?
   *  <variable> <- <value> <L3::comparison_op> <value>
   * →
   *  <variable> <- <value> <L2::comparison_op> <value>
   */
  struct gets_comparison
    : tile::match<L3::instruction::assign::variable::gets_comparison>
    , tile::generates<gets_comparison, std::tuple<
        L2::instruction::assign::assignable::gets_comparison
      >>
    {};
  /* assign a variable to something
   * ?
   *   <variable> <- <movable>
   * →
   *   <variable> <- <movable>
   */
  struct gets_movable
    : tile::match<L3::instruction::assign::variable::gets_movable>
    , tile::generates<gets_movable, std::tuple<
        L2::instruction::assign::assignable::gets_movable
      >>
    {};
}

namespace tile::L3::define {
  /* define a label
   * ?
   *  :label
   * →
   *  :label
   */
  struct label
    : tile::match<L3::instruction::define::label>
    , tile::generates<label, std::tuple<
        L2::instruction::define::label
      >>
    {};
}

namespace tile::L3::branch {
  /* branch unconditionally
   * ?
   *  br :label
   * →
   *  goto :label
   */
  struct unconditional
    : tile::match<L3::instruction::branch::unconditional>
    , tile::generates<unconditional, std::tuple<
        L2::instruction::jump::go2
      >>
    {};
  /* branch on a variable's value
   * ?
   *  br %variable :label
   * →
   *  cjump %variable = 1 :label
   */
  struct variable
    : tile::match<L3::instruction::branch::variable>
    , tile::generates<variable, std::tuple<
        L2::instruction::jump::cjump::when
      >>
    {};
}

namespace tile::registry {
  using namespace tile::L3;
  template <typename Rule>
  auto make_L2 = ast::L2::construct::from_strings<peg::must<Rule>>;
  template<>
  struct generator<ret::nothing> {
    static up_node generate (view::vec<node> const & matched) {
      return make_L2<grammar::L2::instructions>({
        "return"
      });
    }
  };
  template<>
  struct generator<ret::value> {
    static up_node generate (view::vec<node> const & matched) {
      node const & ret = *matched.at(0);
      node const & value = helper::L3::unwrap_assert(ret);
      return make_L2<grammar::L2::instructions>({
        "rax <- ", value.content(), "\n",
        "return",
      });
    }
  };
  template<>
  struct generator<assign::address::gets_movable> {
    static up_node generate (view::vec<node> const & matched) {
      node const & n       = *matched.at(0);
      node const & store   = *n.children.at(0);
      node const & stored  = helper::L3::unwrap_assert(store);
      node const & gets    = *n.children.at(1);
      node const & movable = *n.children.at(2);
      return make_L2<grammar::L2::instructions>({
        "mem ", stored.content(), " 0",
        " ", gets.content(), " ", movable.content(), "\n"
      });
    }
  };
  /* FIXME(jordan): this should be cleaner. We should globalize labels
   * after we've implemented the calling convention.
   */
  namespace label_generator {
    int generated_labels = 0;
    std::string suffix () {
      return "_AUTOGENERATED_LABEL_" + std::to_string(generated_labels++);
    }
  }
  template<>
  struct generator<call::intrinsic::print> {
    static up_node generate (view::vec<node> const & matched) {
      node const & call = helper::L3::unwrap_assert(*matched.at(0));
      node const & arguments = *call.children.at(1);
      assert(arguments.is<grammar::L3::operand::list::arguments>());
      node const & argument0 = helper::L3::unwrap_assert(*arguments.children.at(0));
      return make_L2<grammar::L2::instructions>({
        "rdi <- ", argument0.content(), "\n",
        "call print 1\n",
      });
    }
  };
  template<>
  struct generator<call::intrinsic::allocate> {
    static up_node generate (view::vec<node> const & matched) {
      node const & call = helper::L3::unwrap_assert(*matched.at(0));
      node const & arguments = *call.children.at(1);
      assert(arguments.is<grammar::L3::operand::list::arguments>());
      node const & argument0 = helper::L3::unwrap_assert(*arguments.children.at(0));
      node const & argument1 = helper::L3::unwrap_assert(*arguments.children.at(1));
      return make_L2<grammar::L2::instructions>({
        "rdi <- ", argument0.content(), "\n",
        "rsi <- ", argument1.content(), "\n",
        "call allocate 2\n",
      });
    }
  };
  template<>
  struct generator<call::intrinsic::array_error> {
    static up_node generate (view::vec<node> const & matched) {
      node const & call = helper::L3::unwrap_assert(*matched.at(0));
      node const & arguments = *call.children.at(1);
      assert(arguments.is<grammar::L3::operand::list::arguments>());
      node const & argument0 = helper::L3::unwrap_assert(*arguments.children.at(0));
      node const & argument1 = helper::L3::unwrap_assert(*arguments.children.at(1));
      return make_L2<grammar::L2::instructions>({
        "rdi <- ", argument0.content(), "\n",
        "rsi <- ", argument1.content(), "\n",
        "call array-error 2\n",
      });
    }
  };

  std::vector<std::string> generate_call_expression (const node & call) {
    node const & callable   = *call.children.at(0);
    node const & arguments  = *call.children.at(1);
    bool const is_intrinsic = !callable.is<grammar::L3::operand::callable>();
    node const & callable_name
      = is_intrinsic
      ? callable
      : helper::L3::unwrap_assert(*callable.children.at(0));
    std::string suffix = label_generator::suffix();
    auto return_label
      = ":return_"
      + callable_name.content()
      + suffix;
    // FIXME(jordan): gross. Implements calling convention.
    std::vector<std::string> argument_strings = {};
    for (int i = 0; i < arguments.children.size(); i++) {
      node const & argument
        = helper::L3::unwrap_assert(*arguments.children.at(i));
      if (i == 0) argument_strings.push_back("rdi");
      if (i == 1) argument_strings.push_back("rsi");
      if (i == 2) argument_strings.push_back("rdx");
      if (i == 3) argument_strings.push_back("rcx");
      if (i == 4) argument_strings.push_back("r8");
      if (i == 5) argument_strings.push_back("r9");
      if (i > 5) argument_strings.push_back("mem rsp -" + std::to_string(8 * (i - 4)));
      argument_strings.push_back(" <- ");
      argument_strings.push_back(argument.content());
      argument_strings.push_back("\n");
    }
    std::vector<std::string> instructions = {};
    if (!is_intrinsic)
      helper::collection::append(instructions, {
        "mem rsp -8 <- ", return_label, "\n",
      });
    helper::collection::append(instructions, argument_strings);
    helper::collection::append(instructions, {
      "call ", callable.content(),
      " ", std::to_string(arguments.children.size()), "\n",
    });
    if (!is_intrinsic)
      helper::collection::append(instructions, {
        return_label, "\n",
      });
    return instructions;
  }

  template<>
  struct generator<call::defined> {
    static up_node generate (view::vec<node> const & matched) {
      node const & call = helper::L3::unwrap_assert(*matched.at(0));
      std::vector<std::string> instructions = generate_call_expression(call);
      return make_L2<grammar::L2::instructions>(std::move(instructions));
    }
  };
  template<>
  struct generator<assign::variable::gets_call> {
    static up_node generate (view::vec<node> const & matched) {
      node const & instruction = *matched.at(0);
      assert(instruction.is<grammar::L3::instruction::assign::variable::gets_call>());
      node const & variable = *instruction.children.at(0);
      node const & gets     = *instruction.children.at(1);
      node const & call = helper::L3::unwrap_assert(*instruction.children.at(2));
      std::vector<std::string> call_insts = generate_call_expression(call);
      std::vector<std::string> instructions = call_insts;
      helper::collection::append(instructions, {
        variable.content(), " ", gets.content(), " rax\n"
      });
      return make_L2<grammar::L2::instructions>(std::move(instructions));
    }
  };
  template<>
  struct generator<define::label> {
    static up_node generate (view::vec<node> const & matched) {
      node const & n = *matched.at(0);
      node const & label = *n.children.at(0);
      return make_L2<grammar::L2::instructions>({
        label.content(), "\n"
      });
    }
  };
  template<>
  struct generator<assign::variable::gets_load> {
    static up_node generate (view::vec<node> const & matched) {
      node const & n = *matched.at(0);
      node const & variable = *n.children.at(0);
      node const & gets = *n.children.at(1);
      node const & load = *n.children.at(2);
      node const & loaded = helper::L3::unwrap_assert(load);
      return make_L2<grammar::L2::instructions>({
        variable.content(), " ", gets.content(),
        " mem ", loaded.content(), " 0\n",
      });
    }
  };
  template<>
  struct generator<assign::variable::gets_arithmetic> {
    static up_node generate (view::vec<node> const & matched) {
      node const & n = *matched.at(0);
      node const & variable = *n.children.at(0);
      node const & gets = *n.children.at(1);
      node const & arithmetic = *n.children.at(2);
      node const & lhs = *arithmetic.children.at(0);
      node & op  = *arithmetic.children.at(1);
      node const & rhs = *arithmetic.children.at(2);
      op.realize();
      if (op.is<grammar::L3::op::add>()) {
        op.transform<
          grammar::L3::op::add,
          grammar::L2::op::add
        >("+=");
      } else if (op.is<grammar::L3::op::subtract>()) {
        op.transform<
          grammar::L3::op::subtract,
          grammar::L2::op::subtract
        >("-=");
      } else if (op.is<grammar::L3::op::multiply>()) {
        op.transform<
          grammar::L3::op::multiply,
          grammar::L2::op::multiply
        >("*=");
      } else if (op.is<grammar::L3::op::bitwise_and>()) {
        op.transform<
          grammar::L3::op::bitwise_and,
          grammar::L2::op::bitwise_and
        >("&=");
      }
      return make_L2<grammar::L2::instructions>({
        variable.content(), " ", gets.content(), " ", lhs.content(), "\n",
        variable.content(), " ",   op.content(), " ", rhs.content(), "\n",
      });
    }
  };
  template<>
  struct generator<branch::unconditional> {
    static up_node generate (view::vec<node> const & matched) {
      node const & n = *matched.at(0);
      node const & label = *n.children.at(0);
      return make_L2<grammar::L2::instructions>({
        "goto ", label.content(), "\n"
      });
    }
  };
  template<>
  struct generator<branch::variable> {
    static up_node generate (view::vec<node> const & matched) {
      node const & n = *matched.at(0);
      node const & variable = *n.children.at(0);
      node const & label    = *n.children.at(1);
      return make_L2<grammar::L2::instructions>({
        "cjump ", variable.content(), " = 1 ", label.content(), "\n",
      });
    }
  };
  up_node generate_shift (const node & n) {
    node const & variable = *n.children.at(0);
    node const & gets = *n.children.at(1);
    node const & shift = *n.children.at(2);
    node & lhs = *shift.children.at(0);
    node & op  = *shift.children.at(1);
    node & rhs = *shift.children.at(2);
    op.realize();
    if (true
      && op.is<grammar::L3::op::shift_right>()
      && lhs.is<grammar::L3::operand::number>()
    ) {
      op.transform<
        grammar::L3::op::shift_right,
        grammar::L3::op::shift_left
      >("<<");
      lhs.realize();
      rhs.realize();
      lhs.reset<grammar::L3::operand::value>(rhs.content());
      rhs.reset<grammar::L3::operand::value>(lhs.original_content());
    }
    if (op.is<grammar::L3::op::shift_right>()) {
      op.transform<
        grammar::L3::op::shift_right,
        grammar::L2::op::shift_right
      >(">>=");
    } else if (op.is<grammar::L3::op::shift_left>()) {
      op.transform<
        grammar::L3::op::shift_left,
        grammar::L2::op::shift_left
      >("<<=");
    }
    std::vector<std::string> result = {
      // var <- lhs
      variable.content(), " ", gets.content(), " ", lhs.content(), "\n",
      // var (<<=|>>=) rhs
      variable.content(), " ",   op.content(), " ", rhs.content(), "\n",
    };
    return make_L2<grammar::L2::instructions>(std::move(result));
  }
  template<>
  struct generator<assign::variable::shift::number> {
    static up_node generate (view::vec<node> const & matched) {
      node const & n = *matched.at(0);
      return generate_shift(n);
    }
  };
  template<>
  struct generator<assign::variable::shift::variable> {
    static up_node generate (view::vec<node> const & matched) {
      node const & n = *matched.at(0);
      return generate_shift(n);
    }
  };
  template<>
  struct generator<assign::variable::gets_comparison> {
    static up_node generate (view::vec<node> const & matched) {
      node const & n = *matched.at(0);
      node const & variable = *n.children.at(0);
      node const & gets = *n.children.at(1);
      node const & cmp = *n.children.at(2);
      node & lhs = *cmp.children.at(0);
      node & op  = *cmp.children.at(1);
      node & rhs = *cmp.children.at(2);
      op.realize();
      if (op.is<grammar::L3::op::greater>()) {
        op.transform<
          grammar::L3::op::greater,
          grammar::L3::op::less
        >("<");
        lhs.realize();
        rhs.realize();
        lhs.reset<grammar::L3::operand::value>(rhs.content());
        rhs.reset<grammar::L3::operand::value>(lhs.original_content());
      } else if (op.is<grammar::L3::op::greater_equal>()) {
        op.transform<
          grammar::L3::op::greater_equal,
          grammar::L3::op::less_equal
        >("<=");
        lhs.realize();
        rhs.realize();
        lhs.reset<grammar::L3::operand::value>(rhs.content());
        rhs.reset<grammar::L3::operand::value>(lhs.original_content());
      }
      return make_L2<grammar::L2::instructions>({
        variable.content(),
        " ", gets.content(),
        " ", lhs.content(), " ", op.content(), " ", rhs.content(), "\n"
      });
    }
  };
  template<>
  struct generator<assign::variable::gets_movable> {
    static up_node generate (view::vec<node> const & matched) {
      node const & n = *matched.at(0);
      node const & variable = *n.children.at(0);
      node const & gets     = *n.children.at(1);
      node const & movable  = *n.children.at(2);
      return make_L2<grammar::L2::instructions>({
        variable.content(),
        " ", gets.content(),
        " ", movable.content(), "\n",
      });
    }
  };
}

namespace tile::L3::branchjoin {
  struct variable_unconditional
    : tile::match<
        L3::instruction::branch::variable,
        L3::instruction::branch::unconditional
      >
    , tile::generates<variable_unconditional, std::tuple<
        L2::instruction::jump::cjump::if_else
      >>
    {};
}

namespace tile::registry {
  using tiles = std::tuple<
    // NOTE(jordan): These tiles are all size 1
    tile::L3::ret::nothing,
    tile::L3::ret::value,
    tile::L3::call::intrinsic::print,
    tile::L3::call::intrinsic::allocate,
    tile::L3::call::intrinsic::array_error,
    tile::L3::call::defined,
    tile::L3::assign::variable::gets_call,
    tile::L3::define::label,
    tile::L3::branch::unconditional,
    tile::L3::branch::variable,
    tile::L3::assign::address::gets_movable,
    tile::L3::assign::variable::gets_load,
    tile::L3::assign::variable::gets_arithmetic,
    tile::L3::assign::variable::shift::number,
    tile::L3::assign::variable::shift::variable,
    tile::L3::assign::variable::gets_comparison,
    tile::L3::assign::variable::gets_movable
  >;
  static const size_t REGISTRY_SIZE = std::tuple_size<tiles>::value;
}

namespace tile {
  up_nodes apply (up_nodes const & instructions, bool debug = false) {
    up_nodes roots = {};
    for (int i = 0; i < instructions.size(); i++) {
      up_node const & instruction = instructions.at(i);
      view::vec<node> window = { &*instruction };
      if ((i + 1) < instructions.size()) {
        window.push_back(&*instructions.at(i + 1));
        // TODO(jordan): try tiles with window size of 2
        window.pop_back();
      }
      #define try_tile(I) {                                              \
        using tile = std::tuple_element<I, tile::registry::tiles>::type; \
        if (tile::accept(window, debug)) {                               \
          up_node root = tile::generate(window, debug);                  \
          roots.push_back(std::move(root));                              \
        }                                                                \
      }
      try_tile( 0) try_tile( 1) try_tile( 2) try_tile( 3) try_tile( 4)
      try_tile( 5) try_tile( 6) try_tile( 7) try_tile( 8) try_tile( 9)
      try_tile(10) try_tile(11) try_tile(12) try_tile(13) try_tile(14)
      try_tile(15) try_tile(16)
      #undef try_tile
    }
    return roots;
  }
}
