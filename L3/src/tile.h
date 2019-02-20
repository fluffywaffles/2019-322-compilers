#pragma once

#include <tuple>
#include <typeinfo>
#include <algorithm>

#include "L2/transform.h"

#include "helper.h"
#include "grammar.h"
#include "ast.h"

namespace tile {
  namespace view = helper::view;
  using node     = ast::L3::node;
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
    static const int match_size = sizeof...(Rules);
    static const bool accept (
      view::vec<node> const & window,
      bool debug = false
    ) {
      assert(true
        && match_size == window.size()
        && match_size == std::tuple_size<RulesTuple>::value
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
  template <typename MatchedTile, typename Expect>
  struct generates {
    /* NOTE(jordan): only allow generate to be invoked if a generator is
     * registered for the matched tile.
     */
    static const up_node generate (
      view::vec<node> const & matched,
      bool debug = false
    ) {
      using generator = tile::registry::generator<MatchedTile>;
      up_node result  = std::move(generator::generate(matched));
      // FIXME(jordan): special-cased to L2 function::instruction
      assert(result->is<grammar::L2::function::instructions>());
      view::vec<node> result_view;
      for (up_node const & instruction_wrapper : result->children) {
        node const & actual_instruction
          = helper::L3::unwrap_assert(*instruction_wrapper);
        result_view.push_back(&actual_instruction);
      }
      if (debug) std::cerr << "» tile::generator::check_generate...\n";
      assert(tile::match<Expect>::accept(result_view, debug));
      if (debug) std::cerr << "» SXS  tile::generator::check_generate\n";
      return result;
    }
  };
}

namespace tile::L3 {
  namespace L3 = grammar::L3;
  namespace L2 = grammar::L2;
  namespace ret { struct nothing; struct value; }
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

namespace tile::registry {
  template<> up_node generator<
    tile::L3::ret::nothing
  >::generate(view::vec<node> const & matched) {
    return ast::L3::construct::from_string<
      grammar::L2::function::instructions,
      ast::L2::filter::selector
    >(
      "return"
    );
  }
  template<> up_node generator<
    tile::L3::ret::value
  >::generate(view::vec<node> const & matched) {
    node const & ret = *matched.at(0);
    node const & value = helper::L3::unwrap_assert(ret);
    return ast::L3::construct::from_strings<
      grammar::L2::function::instructions,
      ast::L2::filter::selector
    >({
      "rax <- ", value.content(), "\n",
      "return",
    });
  }
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
        L2::instruction::assign::relative::gets_movable,
        L2::function::instructions,
        L2::instruction::invoke::call::callable,
        L2::instruction::define::label
      >>
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
        L2::instruction::assign::relative::gets_movable,
        L2::instruction::assign::assignable::gets_movable, // rdi <- <0>
        L2::instruction::invoke::call::intrinsic::print,
        L2::instruction::define::label
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
        L2::instruction::assign::relative::gets_movable,
        L2::instruction::assign::assignable::gets_movable, // rdi <- <0>
        L2::instruction::assign::assignable::gets_movable, // rsi <- <1>
        L2::instruction::invoke::call::intrinsic::allocate,
        L2::instruction::define::label
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
        L2::instruction::assign::relative::gets_movable,
        L2::instruction::assign::assignable::gets_movable, // rdi <- <0>
        L2::instruction::assign::assignable::gets_movable, // rsi <- <1>
        L2::instruction::invoke::call::intrinsic::array_error,
        L2::instruction::define::label
      >>
    {};
}
