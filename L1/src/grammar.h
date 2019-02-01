#pragma once

#include "tao/pegtl.hpp"

namespace L1::grammar {
  namespace peg = tao::pegtl;

  //
  // Comments
  //

  struct comment : peg::disable<
    TAO_PEGTL_STRING("//"),
    peg::until<peg::eolf>
  > {};

  //
  // Utilities
  //

  namespace util {
    template <char c> using a = peg::one<c>;
    template <char c, typename... Rules>
      struct exclude : peg::if_must<peg::not_at<a<c>>, Rules...> {};
    template <typename Rule>
      using spaced1 = peg::pad<Rule, peg::sor<peg::space, comment>>;
    template <typename... Rules>
      struct spaced : peg::seq<spaced1<Rules>...> {};
    template <typename... atoms>
      struct sexp : spaced<a<'('>, exclude<')', atoms...>, a<')'>> {};
  }

  template <typename... atoms> using sexp = util::sexp<atoms...>;
  template <typename... Rules> using spaced = util::spaced<Rules...>;

  //
  // Characters
  //

  namespace character {
    using plus         = util::a<'+'>;
    using colon        = util::a<':'>;
    using minus        = util::a<'-'>;
    using numeric      = peg::range<'0', '9'>;
    using lowercase    = peg::range<'a', 'z'>;
    using uppercase    = peg::range<'A', 'Z'>;
    using alphabetic   = peg::sor<lowercase, uppercase>;
    using underscore   = util::a<'_'>;
    using alphanumeric = peg::sor<alphabetic, numeric>;
  }

  //
  // Literals
  //

  // Integers
  //

  namespace literal::number::integer {
    using plus    = character::plus;
    using minus   = character::minus;
    using numeric = character::numeric;
    struct positive : peg::seq<peg::opt<plus>, peg::plus<numeric>> {};
    struct negative : peg::seq<minus, positive> {};
    // NOTE: try to match negative first to short-circuit
    // N ::= <a literal number>
    struct any : peg::sor<negative, positive> {};
  }

  // Special Numbers
  //

  namespace literal::number::special {
    // E ::= 1 | 2 | 4 | 8
    struct scale : peg::one<'1', '2', '4', '8'> {};
    // M ::= N times 8
    struct divisible_by8 : integer::any {};
  }

  //
  // Identifiers
  //

  // Labels
  //

  namespace literal::identifier {
    using namespace character;
    namespace at {
      struct zero : peg::sor<alphabetic, underscore>   {};
      struct gte1 : peg::sor<alphanumeric, underscore> {};
    }
    struct name  : peg::seq<at::zero, peg::star<at::gte1>> {};
    // label ::= :[a-zA-Z_][a-zA-Z_0=9]*
    struct label : peg::seq<colon, name> {};
  }

  // Registers
  //

  // General-purpose
  namespace literal::identifier::x86_64_register {
    struct rax : TAO_PEGTL_STRING("rax") {}; // accumulator
    struct rbx : TAO_PEGTL_STRING("rbx") {}; // base index
    struct rcx : TAO_PEGTL_STRING("rcx") {}; // counter
    struct rdx : TAO_PEGTL_STRING("rdx") {}; // accumulator "d"oubler
    struct rsi : TAO_PEGTL_STRING("rsi") {}; // source index
    struct rdi : TAO_PEGTL_STRING("rdi") {}; // destination index
    struct rbp : TAO_PEGTL_STRING("rbp") {}; // base pointer
    struct rsp : TAO_PEGTL_STRING("rsp") {}; // stack pointer
    // 64-bit-mode-only general-purpose
    struct r8  : TAO_PEGTL_STRING("r8")  {};
    struct r9  : TAO_PEGTL_STRING("r9")  {};
    struct r10 : TAO_PEGTL_STRING("r10") {};
    struct r11 : TAO_PEGTL_STRING("r11") {};
    struct r12 : TAO_PEGTL_STRING("r12") {};
    struct r13 : TAO_PEGTL_STRING("r13") {};
    struct r14 : TAO_PEGTL_STRING("r14") {};
    struct r15 : TAO_PEGTL_STRING("r15") {};
  }

  // Helpful alias for identifier literals
  namespace identifier = literal::identifier;

  //
  // Register sets
  //

  namespace register_set {
    using namespace identifier::x86_64_register;
    using r10_15 = peg::sor<r10, r11, r12, r13, r14, r15>;
    // sx ::= rcx
    // a  ::= rdi | rsi | rdx | sx  | r8 | r9
    // w  ::= a   | rax | rbx | rbp | r10 | r11 | r12 | r13 | r14 | r15
    // x  ::= w   | rsp
    struct shift      : rcx {};
    struct argument   : peg::sor<rdi, rsi, rdx, rcx, r8, r9> {};
    struct assignable : peg::sor<argument, rax, rbx, rbp, r10_15> {};
    struct memory     : peg::sor<assignable, rsp> {};
    // NOTE: utility for matching any register
    struct any : memory {};
  }

  //
  // Operators
  //

  /* NOTE: Operator Organization
   * -----------------------------
   * Types of Operators
   * -----------------------------
   *
   * Addressing (w @ w w E, ...)
   * Shift      (<<=, >>=, ...)
   * Comparison (<=, <, =, ...)
   * Arithmetic (++, --, +=, ...)
   *
   * -----------------------------
   * Operator Namespacing
   * -----------------------------
   *
   * op::<arity>::<op-type>::<op>
   *
   * Ex.:
   *
   * op::quaternary::address::at
   * op::binary::comparison::less
   * ...
   *
   */

  // Quaternary
  //

  namespace op::quaternary {
    namespace address {
      struct at : util::a<'@'> {};
    }
  }

  // Binary
  //

  // Comparison <cmp>
  namespace op::binary::comparison {
    struct less       : util::a<'<'> {};
    struct equal      : util::a<'='> {};
    struct less_equal : TAO_PEGTL_STRING("<=") {};
    // NOTE: order MATTERS!
    // cmp ::= < | <= | =
    struct any : peg::sor<less_equal, less, equal> {};
  }

  // Shift <sop>
  namespace op::binary::shift {
    struct left  : TAO_PEGTL_STRING("<<=") {};
    struct right : TAO_PEGTL_STRING(">>=") {};
    // NOTE: order MATTERS!
    // sop ::= <<= | >>=
    struct any : peg::sor<left, right> {};
  }
  namespace op { using sop = binary::shift::any; }

  // Arithmetic <aop>
  namespace op::binary::arithmetic {
    struct add         : TAO_PEGTL_STRING("+=") {};
    struct subtract    : TAO_PEGTL_STRING("-=") {};
    struct multiply    : TAO_PEGTL_STRING("*=") {};
    struct bitwise_and : TAO_PEGTL_STRING("&=") {};
    // NOTE: order MATTERS!
    // aop ::= += | -= | *= | &=
    struct any : peg::sor<add, subtract, multiply, bitwise_and> {};
  }

  // Unary
  //

  // Increment/Decrement
  namespace op::unary::arithmetic {
    struct decrement : TAO_PEGTL_STRING("--") {};
    struct increment : TAO_PEGTL_STRING("++") {};
  }

  // Aliases
  //

  namespace op {
    using add         = binary::arithmetic::add;
    using less        = binary::comparison::less;
    using equal       = binary::comparison::equal;
    using subtract    = binary::arithmetic::subtract;
    using multiply    = binary::arithmetic::multiply;
    using increment   = unary::arithmetic::increment;
    using decrement   = unary::arithmetic::decrement;
    using address_at  = quaternary::address::at;
    using less_equal  = binary::comparison::less_equal;
    using shift_left  = binary::shift::left;
    using bitwise_and = binary::arithmetic::bitwise_and;
    using shift_right = binary::shift::right;
  }

  //
  // Operands (Registers & Labels & [sometimes] Numbers)
  //

  namespace operand {
    using label  = literal::identifier::label;
    using number = literal::number::integer::any;
    /* NOTE: there is no 'argument' operand because production 'a' is
     * never used as an operand for an instruction in the grammar.
     */
    // Register-only operands
    // sx ::= rcx
    // w  ::= a   | rax | rbx | rbp | r10 | r11 | r12 | r13 | r14 | r15
    // x  ::= w   | rsp
    struct shift      : register_set::shift  {};
    struct memory     : register_set::memory {};
    struct assignable : register_set::assignable {};
    // Register-or-value operands
    // s ::= t | label
    // u ::= w | label
    // t ::= x | N
    struct movable    : peg::sor<memory, number, label> {};
    struct callable   : peg::sor<assignable, label> {};
    struct comparable : peg::sor<memory, number> {};
  }

  //
  // Runtime Intrinsics
  //

  namespace literal::intrinsic {
    struct print       : TAO_PEGTL_STRING("print")    {};
    struct allocate    : TAO_PEGTL_STRING("allocate") {};
    // NOTE: oracle L1 supports both 'array_error' and 'array-error'
    namespace spellings::array_error {
      struct dash       : TAO_PEGTL_STRING("array-error") {};
      struct underscore : TAO_PEGTL_STRING("array_error") {};
      // NOTE: for once, I think order does *not* matter.
      struct any        : peg::sor<underscore, dash>      {};
    }
    using array_error = spellings::array_error::any;
  }

  //
  // Instruction Literals
  //

  namespace literal::instruction {
    struct go2   : TAO_PEGTL_STRING("goto")   {};
    struct mem   : TAO_PEGTL_STRING("mem")    {};
    struct ret   : TAO_PEGTL_STRING("return") {};
    struct call  : TAO_PEGTL_STRING("call")   {};
    struct gets  : TAO_PEGTL_STRING("<-")     {};
    struct cjump : TAO_PEGTL_STRING("cjump")  {};
  }

  //
  // Expressions
  //

  namespace expression {
    // mem x M
    namespace mem_ {
      using M = literal::number::special::divisible_by8;
      using mem = literal::instruction::mem;
      template <typename x>
      struct e : spaced<mem, x, M> {};
    }
    using mem = mem_::e<operand::memory>;
    // t cmp t
    namespace cmp_ {
      using cmp = op::binary::comparison::any;
      template <typename t>
      struct e : spaced<t, cmp, t> {};
    }
    using cmp = cmp_::e<operand::comparable>;
  }

  //
  // Statement Helpers
  //

  namespace statement {
    // <A, B>: A <- B
    namespace gets_ {
      using gets = literal::instruction::gets;
      template <typename dest, typename source>
        struct s : spaced<dest, gets, source> {};
    }
    template <typename d, typename s> using gets = gets_::s<d, s>;
    // cjump t cmp t label [label]
    namespace cjump_ {
      using cjump = literal::instruction::cjump;
      template <typename cmp, typename... labels>
        struct s : spaced<cjump, cmp, labels...> {};
    }
    template<typename cmp, typename... labels>
      using cjump = cjump_::s<cmp, labels...>;
    // call {u,intrinsic} N
    namespace call_ {
      using call = literal::instruction::call;
      template <typename fn, typename num_args>
        struct s : spaced<call, fn, num_args> {};
    }
    namespace call {
      namespace _ = call_;
      template <char c> using a = util::a<c>;
      template <typename f, char n> struct intrinsic : _::s<f, a<n>> {};
      template <typename fn, typename n> struct defined : _::s<fn, n> {};
    }
    // <A,B> A aop B
    namespace aop {
      using add_op   = op::add;
      using bit_and  = op::bitwise_and;
      using subtract = op::subtract;
      using multiply = op::multiply;
      template <class D, class S> struct add  : spaced<D, add_op,   S> {};
      template <class D, class S> struct sub  : spaced<D, subtract, S> {};
      template <class D, class S> struct mul  : spaced<D, multiply, S> {};
      template <class D, class S> struct andq : spaced<D, bit_and,  S> {};
      using any_aop  = op::binary::arithmetic::any;
      template <class D, class S> struct s    : spaced<D, any_aop,  S> {};
    }
    namespace arithmetic {
      namespace _ = aop;
      template <class D, class S> struct add : _::add<D, S> {};
      template <class D, class S> struct subtract : _::sub<D, S> {};
      template <class D, class S> struct multiply : _::mul<D, S> {};
      template <class D, class S> struct bitwise_and : _::andq<D, S> {};
      template <class D, class S> struct all : _::s<D, S> {};
    }
    // <A,B> A sop B
    namespace sop {
      using any_sop = op::binary::shift::any;
      using shift_l = op::shift_left;
      using shift_r = op::shift_right;
      template <class D, class S> struct shl : spaced<D, shift_l, S> {};
      template <class D, class S> struct shr : spaced<D, shift_r, S> {};
      template <class D, class S> struct s   : spaced<D, any_sop, S> {};
    }
    namespace shift {
      namespace _ = sop;
      template <class D, class S> struct shift_left  : _::shl<D, S> {};
      template <class D, class S> struct shift_right : _::shr<D, S> {};
      template <class D, class S> struct all : _::s<D, S> {};
    }
  }

  //
  // Instructions
  //

  // Label Definition
  //

  namespace instruction::define {
    struct label : spaced<literal::identifier::label> {};
  }

  // Assignment
  //

  // To assignable registers
  namespace instruction::assign::assignable {
    namespace expr = expression;
    namespace stmt = statement;
    using w = operand::assignable;
    using s = operand::movable;
    using E = literal::number::special::scale;
    using at = op::address_at;
    struct gets_address    : spaced<w, at, w, w, E> {};   // w @ w w E
    struct gets_movable    : stmt::gets<w, s> {};         // w <- s
    struct gets_relative   : stmt::gets<w, expr::mem> {}; // w <- mem x M
    struct gets_comparison : stmt::gets<w, expr::cmp> {}; // w <- t cmp t
  }

  // To relative memory locations
  namespace instruction::assign::relative {
    namespace expr = expression;
    namespace stmt = statement;
    using s = operand::movable;
    struct gets_movable : stmt::gets<expr::mem, s> {}; // mem x M <- s
  }

  // Update
  //

  // Artihmetic on assignable registers
  namespace instruction::update::assignable::arithmetic {
    namespace aop = statement::arithmetic;
    using w = operand::assignable;
    using t = operand::comparable;
    using mem = expression::mem;
    using plusplus   = op::increment;
    using minusminus = op::decrement;
    struct increment         : spaced<w, plusplus> {};   // w++
    struct decrement         : spaced<w, minusminus> {}; // w--
    struct comparable        : aop::all<w, t> {};        // w aop t
    struct add_relative      : aop::add<w, mem> {};      // w += mem x M
    struct subtract_relative : aop::subtract<w, mem> {}; // w -= mem x M
  }

  // Shifts on assignable registers
  namespace instruction::update::assignable::shift {
    namespace stmt = statement;
    using w  = operand::assignable;
    using N  = literal::number::integer::any;
    using sx = operand::shift;
    struct shift  : stmt::shift::all<w, sx> {}; // w sop sx
    struct number : stmt::shift::all<w,  N> {}; // w sop N
  }

  // Arithmetic on relative memory locations
  namespace instruction::update::relative::arithmetic {
    namespace aop = statement::arithmetic;
    using t = operand::comparable;
    using mem = expression::mem;
    struct add_comparable      : aop::add<mem, t> {};      // mem x M += t
    struct subtract_comparable : aop::subtract<mem, t> {}; // mem x M -= t
  }

  // Jump
  //
  namespace instruction::jump {
    using label = literal::identifier::label;
    using go2_literal = literal::instruction::go2;
    struct go2 : spaced<go2_literal, label> {}; // goto label
    namespace cjump {
      namespace stmt = statement;
      using cmp   = expression::cmp;
      using label = label;
      struct when                             // cjump t cmp t label
        : stmt::cjump<cmp, label> {};
      struct if_else                          // cjump t cmp t label label
        : stmt::cjump<cmp, label, label> {};
    }
  }

  // Invoke
  //
  namespace instruction::invoke {
    namespace call::intrinsic {
      template <typename i, char c >
        using call = statement::call::intrinsic<i, c>;
      namespace literal = literal::intrinsic;
      struct print                              // call print 1
        : call<literal::print, '1'> {};
      struct allocate                           // call allocate 2
        : call<literal::allocate, '2'> {};
      struct array_error                        // call array-error 2
        : call<literal::array_error, '2'> {};
    }
    namespace call {
      using u = operand::callable;
      using N = literal::number::integer::positive;
      struct callable : statement::call::defined<u, N> {}; // call u N
    }
    struct ret : spaced<literal::instruction::ret> {};
  }

  namespace instruction {
    // NOTE: order MATTERS here! Follow the grammar.
    struct any : peg::sor<
      // w <- t cmp t
      assign::assignable::gets_comparison,
      // w <- s
      assign::assignable::gets_movable,
      // w <- mem x M
      assign::assignable::gets_relative,
      // mem x M <- s
      assign::relative::gets_movable,
      // w aop t
      update::assignable::arithmetic::comparable,
      // w sop sx
      update::assignable::shift::shift,
      // w sop N
      update::assignable::shift::number,
      // mem x M += t
      update::relative::arithmetic::add_comparable,
      // mem x M -= t
      update::relative::arithmetic::subtract_comparable,
      // w += mem x M
      update::assignable::arithmetic::add_relative,
      // w -= mem x M
      update::assignable::arithmetic::subtract_relative,
      // cjump t cmp t label label
      jump::cjump::if_else,
      // cjump t cmp t label
      jump::cjump::when,
      // label
      define::label,
      // goto label
      jump::go2,
      // return
      invoke::ret,
      // call u N
      invoke::call::callable,
      // call print 1
      invoke::call::intrinsic::print,
      // call allocate 2
      invoke::call::intrinsic::allocate,
      // call array-error 2
      invoke::call::intrinsic::array_error,
      // w++
      update::assignable::arithmetic::increment,
      // w--
      update::assignable::arithmetic::decrement,
      // w @ w w E
      assign::assignable::gets_address
    > {};
  }

  namespace function {
    struct arg_count    : literal::number::integer::positive {};
    struct local_count  : literal::number::integer::positive {};
    struct instructions : peg::plus<instruction::any> {};

    struct define : sexp<spaced<
      literal::identifier::label,
      arg_count,
      local_count,
      instructions
    >> {};
  }

  namespace program {
    struct functions : peg::plus<spaced<function::define>> {};

    struct define : sexp<spaced<
      literal::identifier::label,
      functions
    >> {};
  }

  struct entry : spaced<program::define> {};
}
