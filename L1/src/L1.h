#pragma once
#include <string>
#include <vector>
#include <iostream>

namespace L1 {
  // TODO: this is a parser state, not an L1 type
  struct Base {
    public:
    Base  () = default;
    ~Base () = default;
    // Delete the move constructors
    Base (Base &&)      = delete;
    Base (const Base &) = delete;

    // Delete the move assignment operators
    void operator= (Base &&)     = delete;
    void operator= (const Base &) = delete;
  };

  struct Program;
  struct Function;

  namespace Tag {
    enum class Literal {
      // label ::= :[a-zA-Z_][a-zA-Z_0=9]*
      identifier_label,
      // N ::= <a literal number>
      number_integer,
      number_integer_positive,
      number_integer_negative,
      // E ::= 0 | 2 | 4 | 8
      number_special_scale,
      // M ::= N times 8
      number_special_divisible_by8,
      // s  ::= t | label
      operand_movable,
      // u  ::= w | label
      operand_callable,
      // w  ::= a  | rax | rbx | rbp | r10 | r11 | r12 | r13 | r14 | r15
      register_usable,
      // x  ::= w  | rsp
      register_memory,
      // sx ::= rcx
      register_shift,
      // t  ::= x | N
      operand_comparable,
      // aop
      op_add,
      op_subtract,
      op_multiply,
      op_bitwise_and,
      // cmp
      op_less,
      op_equal,
      op_less_equal,
      // sop
      op_shift_left,
      op_shift_right,
      // NOTE: for failure detection
      ERROR,
    };
  }

  namespace Kind {
    enum class Literal {
      op,
      string,
      integer,
      x86_64_register,
      ERROR,
    };
  }

  struct Literal {
    Tag::Literal tag = Tag::Literal::ERROR;
    Kind::Literal kind = Kind::Literal::ERROR;
    std::string source;
    int int_value;
  };

  namespace Tag {
    enum class Expression {
      // mem x M
      mem,
      // t cmp t
      cmp,
      // NOTE: for failure detection
      ERROR,
    };
  }

  struct Expression {
    Tag::Expression tag = Tag::Expression::ERROR;
    std::vector<Literal> literals;
  };

  namespace Tag {
    enum class Instruction {
      // w <- s
      assign_usable_gets_movable,
      // w <- mem x M
      assign_usable_gets_relative,
      // mem x M <- s
      assign_relative_gets_movable,
      // w aop t
      update_usable_arithmetic_comparable,
      // w sop sx
      update_usable_shift_shift_register,
      // w sop N
      update_usable_shift_number,
      // mem x M += t
      update_relative_arithmetic_add_comparable,
      // mem x M -= t
      update_relative_arithmetic_subtract_comparable,
      // w += mem x M
      update_usable_arithmetic_add_relative,
      // w -= mem x M
      update_usable_arithmetic_subtract_relative,
      // w <- t cmp t
      assign_usable_gets_comparison,
      // cjump t cmp t label label
      jump_cjump_if_else,
      // cjump t cmp t label
      jump_cjump_when,
      // label
      define_label,
      // goto label
      jump_go2,
      // return
      invoke_ret,
      // call u N
      invoke_call_callable,
      // call print 1
      invoke_call_intrinsic_print,
      // call allocate 2
      invoke_call_intrinsic_allocate,
      // call array-error 2
      invoke_call_intrinsic_array_error,
      // w++
      update_usable_arithmetic_increment,
      // w--
      update_usable_arithmetic_decrement,
      // w @ w w E
      assign_usable_gets_address,
      // NOTE: for failure detection
      ERROR,
    };
  }

  struct Instruction {
    Tag::Instruction tag = Tag::Instruction::ERROR;
    std::vector<Literal> literals;
  };

  struct Function {
    struct Arity {
      Literal arguments;
      Literal locals;
    };

    Arity arity;
    Literal name;
    std::vector<Instruction> instructions;
  };

  struct Program {
    Literal entry;
    std::vector<Function> functions;
  };
}
