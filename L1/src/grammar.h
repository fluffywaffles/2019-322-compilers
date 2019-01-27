#include "tao/pegtl.hpp"

namespace peg = tao::pegtl;

namespace L1::grammar {
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

  inline namespace util {
    template <char c> using a = peg::one<c>;
    template <char c, typename... Rules>
      struct exclude : peg::if_must<peg::not_at<a<c>>, Rules...> {};
    template <typename... atoms>
      struct sexp : peg::must<a<'('>, exclude<')', atoms...>, a<')'>> {};
    // Eat left spaces, and right comments
    template <typename Rule>
      using spaced1 = peg::pad<Rule, peg::sor<peg::space, comment>>;
    template <typename... Rules>
      struct spaced  : peg::seq<spaced1<Rules>...> {};
  }

  //
  // Characters
  //

  inline namespace character {
    struct colon        : a<':'> {};
    struct numeric      : peg::range<'0', '9'> {};
    struct lowercase    : peg::range<'a', 'z'> {};
    struct uppercase    : peg::range<'A', 'Z'> {};
    struct alphabetic   : peg::sor<lowercase, uppercase> {};
    struct underscore   : a<'_'> {};
    struct alphanumeric : peg::sor<alphabetic, numeric> {};
  }

  //
  // Identifiers
  //

  // Labels
  namespace literal::identifier {
    namespace at {
      struct zero : peg::sor<alphabetic, underscore>   {};
      struct gte1 : peg::sor<alphanumeric, underscore> {};
    }
    struct name  : peg::seq<at::zero, peg::star<at::gte1>> {};
    // label ::= :[a-zA-Z_][a-zA-Z_0=9]*
    struct label : peg::seq<colon, name> {};
  }

  //
  // Literals
  //

  // Numerals
  namespace literal {
    struct number : peg::seq<peg::opt<a<'-'>>, peg::plus<numeric>> {};
    // N ::= <a literal number>
    using N = number;
  }

  // Numerals for Addressing
  // NOTE: there is no (%x, %y, 0) - only 2, 4, 8.
  // QUESTION: 0 means (%x, %y)?
  namespace literal {
    struct scale : peg::one<'0', '2', '4', '8'> {};
    // E ::= 0 | 2 | 4 | 8
    using E = scale;
  }

  // Rule-based Numerals
  namespace literal {
    // TODO: enforce divisibility by 8 in parsing action?
    struct sum_bytes : number {};
    // M ::= N times 8
    using M = sum_bytes;
  }

  //
  // Operators
  //

  // Quaternary
  //

  namespace op::quaternary {
    namespace address {
      struct at : a<'@'> {};
    }
  }

  // Binary
  //

  // Comparison <cmp>
  namespace op::binary::comparison {
    struct less       : a<'<'> {};
    struct equal      : a<'='> {};
    struct less_equal : peg::seq<less, equal> {};
    // NOTE: order MATTERS!
    struct any : peg::sor<less, less_equal, equal> {};
  }
  // cmp ::= < | <= | =
  namespace op { using cmp = binary::comparison::any; }

  // Shift <sop>
  namespace op::binary::shift {
    struct left  : TAO_PEGTL_STRING("<<=") {};
    struct right : TAO_PEGTL_STRING(">>=") {};
    // NOTE: order MATTERS!
    struct any : peg::sor<left, right> {};
  }
  // sop ::= <<= | >>=
  namespace op { using sop = binary::shift::any; }

  // Arithmetic <aop>
  namespace op::binary::arithmetic {
    struct add         : TAO_PEGTL_STRING("+=") {};
    struct subtract    : TAO_PEGTL_STRING("-=") {};
    struct multiply    : TAO_PEGTL_STRING("*=") {};
    struct bitwise_and : TAO_PEGTL_STRING("&=") {};
    // NOTE: order MATTERS!
    struct any : peg::sor<add, subtract, multiply, bitwise_and> {};
  }
  // aop ::= += | -= | *= | &=
  namespace op { using aop = binary::arithmetic::any; }

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
  // Registers
  //

  // General-purpose
  namespace registers {
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

  //
  // Register sets
  //

  inline namespace register_set {
    using namespace registers;
    // NOTE: order matters! (again!)
    struct r10_15  : peg::sor<r10, r11, r12, r13, r14, r15> {};
    struct shift   : rcx {};
    struct address : peg::sor<rdi, rsi, rdx, rcx, r8, r9> {};
    struct usable  : peg::sor<address, rax, rbx, rbp, r10_15> {};
    struct memory  : peg::sor<usable, rsp> {};
    // sx ::= rcx
    // a  ::= rdi | rsi | rdx | rcx | r8 | r9
    // w  ::= a   | rax | rbx | rbp | r10 | r11 | r12 | r13 | r14 | r15
    // x  ::= w   | rsp
    using sx  = shift;   // I'm pretty sure this is the correct mnemonic
    using a   = address; // But these two, I don't know... Is 'a' address?
    using w   = usable;  // Is 'w'... word? usable is better.
    using x   = memory;  // Is 'x'... yeah I don't know. It's just 'x'.
    using any = memory;
  }

  //
  // Operands (Registers & Labels & [sometimes] Numbers)
  //

  namespace operand {
    using label  = literal::identifier::label;
    using number = literal::number;
    struct movable    : peg::sor<register_set::any, number, label> {};
    struct callable   : peg::sor<register_set::usable, label> {};
    struct comparable : peg::sor<register_set::any, number> {};
    // s ::= t | label
    // u ::= w | label
    // t ::= x | N
    using s = movable;
    using u = callable;
    using t = comparable;
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
      using x = register_set::memory;
      using M = literal::sum_bytes;
      using mem = literal::instruction::mem;
      struct e : spaced<mem, x, M> {};
    }
    using mem = mem_::e;
    // t cmp t
    namespace cmp_ {
      using t = operand::comparable;
      using cmp = op::binary::comparison::any;
      struct e : spaced<t, cmp, t> {};
    }
    using cmp = cmp_::e;
    // <A, B>: A <- B
    namespace gets_ {
      using gets = literal::instruction::gets;
      template <typename dest, typename source>
        struct e : spaced<dest, gets, source> {};
    }
    template <typename d, typename s> using gets = gets_::e<d, s>;
    // cjump t cmp t label [label]
    namespace cjump_ {
      using lbl = literal::identifier::label;
      using cjump = literal::instruction::cjump;
      template <typename... labels> struct e;
      template<> struct e<lbl>      : spaced<cjump, cmp, lbl> {};
      template<> struct e<lbl, lbl> : spaced<cjump, cmp, lbl, lbl> {};
    }
    template<typename... labels> using cjump = cjump_::e<labels...>;
    // call {u,intrinsic} N
    namespace call_ {
      using call = literal::instruction::call;
      template <typename fn, typename num_args>
        struct e : spaced<call, fn, num_args> {};
    }
    namespace call {
      namespace _ = call_;
      template <typename f, char c> struct rt : _::e<f, util::a<c>> {};
      template <typename fn, typename n> struct dyn : _::e<fn, n> {};
    }
    // <A,B> A aop B
    namespace aop {
      using any_aop = op::binary::arithmetic::any;
      using add_op = op::add;
      using bit_and = op::bitwise_and;
      using subtract = op::subtract;
      using multiply = op::multiply;
      template <class D, class S> struct add  : spaced<D, add_op,   S> {};
      template <class D, class S> struct sub  : spaced<D, subtract, S> {};
      template <class D, class S> struct mul  : spaced<D, multiply, S> {};
      template <class D, class S> struct andq : spaced<D, bit_and,  S> {};
      template <class D, class S> struct e    : spaced<D, any_aop,  S> {};
    }
    namespace arithmetic_op {
      namespace _ = aop;
      template <class D, class S> struct add : _::add<D, S> {};
      template <class D, class S> struct subtract : _::sub<D, S> {};
      template <class D, class S> struct multiply : _::mul<D, S> {};
      template <class D, class S> struct bitwise_and : _::andq<D, S> {};
      template <class D, class S> struct all : _::e<D, S> {};
    }
    // <A,B> A sop B
    namespace sop {
      using any_sop = op::binary::shift::any;
      using shift_l = op::shift_left;
      using shift_r = op::shift_right;
      template <class D, class S> struct shl : spaced<D, shift_l, S> {};
      template <class D, class S> struct shr : spaced<D, shift_r, S> {};
      template <class D, class S> struct e   : spaced<D, any_sop, S> {};
    }
    namespace shift_op {
      namespace _ = sop;
      template <class D, class S> struct shift_left  : _::shl<D, S> {};
      template <class D, class S> struct shift_right : _::shr<D, S> {};
      template <class D, class S> struct all : _::e<D, S> {};
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

  // To usable registers
  namespace instruction::assign::usable {
    namespace expr = expression;
    using w = register_set::usable;
    using s = operand::movable;
    using E = literal::scale;
    using at = op::address_at;
    struct gets_address    : spaced<w, at, w, w, E> {};   // w @ w w E
    struct gets_movable    : expr::gets<w, s> {};         // w <- s
    struct gets_relative   : expr::gets<w, expr::mem> {}; // w <- mem x M
    struct gets_comparison : expr::gets<w, expr::cmp> {}; // w <- t cmp t
  }

  // To relative memory locations
  namespace instruction::assign::relative {
    namespace expr = expression;
    using s = operand::movable;
    using mem = expression::mem;
    struct gets_movable : expr::gets<mem, s> {}; // mem x M <- s
  }

  // Update
  //

  // Artihmetic on usable registers
  namespace instruction::update::usable::arithmetic {
    namespace aop = expression::arithmetic_op;
    using w = register_set::usable;
    using t = operand::comparable;
    using mem = expression::mem;
    using plusplus   = op::increment;
    using minusminus = op::decrement;
    struct increment         : spaced<peg::seq<w, plusplus>> {};   // w++
    struct decrement         : spaced<peg::seq<w, minusminus>> {}; // w--
    struct comparable        : aop::all<w, t> {};          // w aop t
    struct add_relative      : aop::add<w, mem> {};        // w += mem x M
    struct subtract_relative : aop::subtract<w, mem> {};   // w -= mem x M
  }

  // Shifts on usable registers
  namespace instruction::update::usable::shift {
    namespace expr = expression;
    using namespace op;
    using w   = register_set::usable;
    using N  = literal::number;
    using sx = register_set::shift;
    struct shift_register : expr::shift_op::all<w, sx> {}; // w sop sx
    struct number         : expr::shift_op::all<w,  N> {}; // w sop N
  }

  // Arithmetic on relative memory locations
  namespace instruction::update::relative::arithmetic {
    namespace aop = expression::arithmetic_op;
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
    struct go2  : peg::seq<go2_literal, label> {}; // goto label
    namespace cjump {
      template <typename... Ts>
        using cjump_expr = expression::cjump<Ts...>;
      using l = label;
      struct when    : cjump_expr<l> {};    // cjump t cmp t label
      struct if_else : cjump_expr<l, l> {}; // cjump t cmp t label label
    }
  }

  // Invoke
  //
  namespace instruction::invoke {
    namespace call::intrinsic {
      namespace call = expression::call;
      using print_ = literal::intrinsic::print;
      using alloc_ = literal::intrinsic::allocate;
      using error_ = literal::intrinsic::array_error;
      struct print       : call::rt<print_, '1'> {}; // call print 1
      struct allocate    : call::rt<alloc_, '2'> {}; // call allocate 2
      struct array_error : call::rt<error_, '2'> {}; // call array-error 2
    }
    namespace call {
      using u = operand::callable;
      using N = literal::number;
      struct callable : expression::call::dyn<u, N> {}; // call u N
    }
    struct ret : spaced<literal::instruction::ret> {};
  }

  namespace instruction {
    // NOTE: order MATTERS here! Follow the grammar.
    struct any : peg::sor<
      // w <- s
      assign::usable::gets_movable,
      // w <- mem x M
      assign::usable::gets_relative,
      // mem x M <- s
      assign::relative::gets_movable,
      // w aop t
      update::usable::arithmetic::comparable,
      // w sop sx
      update::usable::shift::shift_register,
      // w sop N
      update::usable::shift::number,
      // mem x M += t
      update::relative::arithmetic::add_comparable,
      // mem x M -= t
      update::relative::arithmetic::subtract_comparable,
      // w += mem x M
      update::usable::arithmetic::add_relative,
      // w -= mem x M
      update::usable::arithmetic::subtract_relative,
      // w <- t cmp t
      assign::usable::gets_comparison,
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
      update::usable::arithmetic::increment,
      // w--
      update::usable::arithmetic::decrement,
      // w @ w w E
      assign::usable::gets_address
    > {};
  }

  struct function : spaced<sexp<spaced<
    literal::identifier::label,
    literal::number,
    literal::number,
    peg::plus<instruction::any>
  >>> {};

  struct program : spaced<sexp<spaced<
    literal::identifier::label,
    peg::plus<peg::if_must<peg::at<util::a<'('>>, function>>
  >>> {};
}
