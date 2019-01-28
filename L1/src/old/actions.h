#pragma once
#include <iostream>
#include "tao/pegtl.hpp"
#include "tao/pegtl/contrib/changes.hpp"
#include "grammar.h"
#include "L1.h"

namespace peg = tao::pegtl;

namespace L1::action {
  //
  // Actions
  //

  // Base actions
  template <typename Rule> struct action : peg::nothing<Rule> {};
  template <typename Rule> using dispatch = action<Rule>;

  struct context {
    L1::Program program;
    std::vector<L1::Function> functions;
    std::vector<L1::Instruction> instructions;
    std::vector<L1::Expression> expressions;
    std::vector<L1::Literal> literals;
  };

  // Case utility macros
  #define rule(T) \
    template<> struct action <grammar::T>
  #define instruction_rule(T) \
    template<> struct action <grammar::instruction::T>
  #define action_apply                                                   \
    template <typename Input>                                            \
      static void apply (const Input & in, context & context)

  template <typename T> T pop_vec (std::vector<T> & vec) {
    T result = vec.back();
    vec.pop_back();
    return result;
  }

  template <typename T>
  void take_end_vec (int count, std::vector<T> & dest, std::vector<T> & source) {
    for (auto it = (source.end() - count); it != source.end(); it++)
      dest.push_back(*it);
    source.erase(source.end() - count, source.end());
  }

  // Program
  //

  rule (program) {
    action_apply {
      std::cout << "Program\n";
    }
  };

  // Function
  //

  rule (function) {
    action_apply {
      std::cout << "Function\n";
    }
  };

  // Instruction
  //

  instruction_rule (assign::usable::gets_movable) {
    action_apply {
      std::cout << "Instruction - usable::gets_movable \n";
      Instruction i;
      i.tag = Tag::Instruction::assign_usable_gets_movable;
      take_end_vec(2, i.literals, context.literals);
      assert(i.literals.at(0).tag == Tag::Literal::register_usable);
      assert(i.literals.at(1).tag == Tag::Literal::operand_movable);
      context.instructions.push_back(i);
    }
  };

  instruction_rule (assign::usable::gets_relative) {
    action_apply {
      std::cout << "Instruction - usable::gets_relative \n";
      Instruction i;
      i.tag = Tag::Instruction::assign_usable_gets_relative;
      Expression expr = pop_vec(context.expressions);
      assert(expr.tag == Tag::Expression::mem);
      take_end_vec(1, i.literals, context.literals);
      take_end_vec(2, i.literals, expr.literals);
      assert(i.literals.at(0).tag == Tag::Literal::register_usable);
      context.instructions.push_back(i);
    }
  };

  instruction_rule (assign::relative::gets_movable) {
    action_apply {
      std::cout << "Instruction - relative::gets_movable \n";
      Instruction i;
      i.tag = Tag::Instruction::assign_relative_gets_movable;
      Expression expr = pop_vec(context.expressions);
      assert(expr.tag == Tag::Expression::mem);
      take_end_vec(2, i.literals, expr.literals);
      take_end_vec(1, i.literals, context.literals);
      assert(i.literals.at(2).tag == Tag::Literal::operand_movable);
      context.instructions.push_back(i);
    }
  };

  instruction_rule (update::usable::arithmetic::comparable) {
    action_apply {
      std::cout << "Instruction - usable::arithmetic::comparable \n";
      Instruction i;
      i.tag = Tag::Instruction::update_usable_arithmetic_comparable;
      take_end_vec(3, i.literals, context.literals);
      assert(i.literals.at(0).tag  == Tag::Literal::register_usable);
      assert(i.literals.at(1).kind == Kind::Literal::op);
      assert(i.literals.at(2).tag  == Tag::Literal::operand_comparable);
      context.instructions.push_back(i);
    }
  };

  instruction_rule (update::usable::shift::shift_register) {
    action_apply {
      std::cout << "Instruction - usable::shift::sx \n";
      Instruction i;
      i.tag = Tag::Instruction::update_usable_shift_shift_register;
      take_end_vec(3, i.literals, context.literals);
      assert(i.literals.at(0).tag  == Tag::Literal::register_usable);
      assert(i.literals.at(1).kind == Kind::Literal::op);
      assert(i.literals.at(2).tag  == Tag::Literal::register_shift);
      context.instructions.push_back(i);
    }
  };

  instruction_rule (update::usable::shift::number) {
    action_apply {
      std::cout << "Instruction - usable::shift::number \n";
      Instruction i;
      i.tag = Tag::Instruction::update_usable_shift_number;
      take_end_vec(3, i.literals, context.literals);
      assert(i.literals.at(0).tag  == Tag::Literal::register_usable);
      assert(i.literals.at(1).kind == Kind::Literal::op);
      assert(i.literals.at(2).tag  == Tag::Literal::number_integer);
      context.instructions.push_back(i);
    }
  };

  instruction_rule (update::relative::arithmetic::add_comparable) {
    action_apply {
      std::cout << "Instruction - relative::arithmetic::add_comparable \n";
      Instruction i;
      i.tag = Tag::Instruction::update_relative_arithmetic_add_comparable;
      Expression expr = pop_vec(context.expressions);
      assert(expr.tag == Tag::Expression::mem);
      take_end_vec(2, i.literals, expr.literals);
      take_end_vec(2, i.literals, context.literals);
      assert(i.literals.at(2).tag == Tag::Literal::op_add);
      assert(i.literals.at(3).tag == Tag::Literal::operand_comparable);
      context.instructions.push_back(i);
    }
  };

  instruction_rule (update::relative::arithmetic::subtract_comparable) {
    action_apply {
      std::cout << "Instruction - relative::arithmetic::subtract_comparable \n";
      Instruction i;
      i.tag = Tag::Instruction::update_relative_arithmetic_subtract_comparable;
      Expression expr = pop_vec(context.expressions);
      assert(expr.tag == Tag::Expression::mem);
      take_end_vec(2, i.literals, expr.literals);
      take_end_vec(2, i.literals, context.literals);
      assert(i.literals.at(2).tag == Tag::Literal::op_subtract);
      assert(i.literals.at(3).tag == Tag::Literal::operand_comparable);
      context.instructions.push_back(i);
    }
  };

  instruction_rule (update::usable::arithmetic::add_relative) {
    action_apply {
      std::cout << "Instruction - usable::arithmetic::add_relative \n";
      Instruction i;
      i.tag = Tag::Instruction::update_usable_arithmetic_add_relative;
      Expression expr = pop_vec(context.expressions);
      assert(expr.tag == Tag::Expression::mem);
      take_end_vec(2, i.literals, context.literals);
      take_end_vec(2, i.literals, expr.literals);
      assert(i.literals.at(0).tag == Tag::Literal::register_usable);
      assert(i.literals.at(1).tag == Tag::Literal::op_add);
      context.instructions.push_back(i);
    }
  };

  instruction_rule (update::usable::arithmetic::subtract_relative) {
    action_apply {
      std::cout << "Instruction - usable::arithmetic::subtract_relative \n";
      Instruction i;
      i.tag = Tag::Instruction::update_usable_arithmetic_subtract_relative;
      Expression expr = pop_vec(context.expressions);
      assert(expr.tag == Tag::Expression::mem);
      take_end_vec(2, i.literals, context.literals);
      take_end_vec(2, i.literals, expr.literals);
      assert(i.literals.at(0).tag == Tag::Literal::register_usable);
      assert(i.literals.at(1).tag == Tag::Literal::op_subtract);
      context.instructions.push_back(i);
    }
  };

  instruction_rule (assign::usable::gets_comparison) {
    action_apply {
      std::cout << "Instruction - usable::gets_comparison \n";
      Instruction instruction;
      instruction.tag = Tag::Instruction::assign_usable_gets_comparison;
      context.instructions.push_back(instruction);
    }
  };

  instruction_rule (jump::cjump::if_else) {
    action_apply {
      std::cout << "Instruction - jump::cjump::if_else \n";
      Instruction instruction;
      instruction.tag = Tag::Instruction::jump_cjump_if_else;
      context.instructions.push_back(instruction);
    }
  };

  instruction_rule (jump::cjump::when) {
    action_apply {
      std::cout << "Instruction - jump::cjump::when \n";
      Instruction instruction;
      instruction.tag = Tag::Instruction::jump_cjump_when;
      context.instructions.push_back(instruction);
    }
  };

  instruction_rule (define::label) {
    action_apply {
      std::cout << "Instruction - define::label \n";
      Instruction instruction;
      instruction.tag = Tag::Instruction::define_label;
      context.instructions.push_back(instruction);
    }
  };

  instruction_rule (jump::go2) {
    action_apply {
      std::cout << "Instruction - jump::go2 \n";
      Instruction instruction;
      instruction.tag = Tag::Instruction::jump_go2;
      context.instructions.push_back(instruction);
    }
  };

  instruction_rule (invoke::ret) {
    action_apply {
      std::cout << "Instruction - invoke::ret \n";
      Instruction instruction;
      instruction.tag = Tag::Instruction::invoke_ret;
      context.instructions.push_back(instruction);
    }
  };

  instruction_rule (invoke::call::callable) {
    action_apply {
      std::cout << "Instruction - invoke::call::callable \n";
      Instruction instruction;
      instruction.tag = Tag::Instruction::invoke_call_callable;
      context.instructions.push_back(instruction);
    }
  };

  instruction_rule (invoke::call::intrinsic::print) {
    action_apply {
      std::cout << "Instruction - invoke::call::intrinsic::print \n";
      Instruction instruction;
      instruction.tag = Tag::Instruction::invoke_call_intrinsic_print;
      context.instructions.push_back(instruction);
    }
  };

  instruction_rule (invoke::call::intrinsic::allocate) {
    action_apply {
      std::cout << "Instruction - invoke::call:intrinsic::allocate \n";
      Instruction instruction;
      instruction.tag = Tag::Instruction::invoke_call_intrinsic_allocate;
      context.instructions.push_back(instruction);
    }
  };

  instruction_rule (invoke::call::intrinsic::array_error) {
    action_apply {
      std::cout << "Instruction - invoke::call::intrinsic::array_error \n";
      Instruction instruction;
      instruction.tag = Tag::Instruction::invoke_call_intrinsic_array_error;
    }
  };

  instruction_rule (update::usable::arithmetic::increment) {
    action_apply {
      std::cout << "Instruction - update::usable::arithmetic::increment \n";
      Instruction instruction;
      instruction.tag = Tag::Instruction::update_usable_arithmetic_increment;
      context.instructions.push_back(instruction);
    }
  };

  instruction_rule (update::usable::arithmetic::decrement) {
    action_apply {
      std::cout << "Instruction - update::usable::arithmetic::decrement \n";
      Instruction instruction;
      instruction.tag = Tag::Instruction::update_usable_arithmetic_decrement;
    }
  };

  instruction_rule (assign::usable::gets_address) {
    action_apply {
      std::cout << "Instruction - assign::usable::gets_address \n";
      Instruction instruction;
      instruction.tag = Tag::Instruction::assign_usable_gets_address;
      context.instructions.push_back(instruction);
    }
  };

  // Number
  //

  rule (literal::number::integer::positive) {
    action_apply {
      std::cout << "Number - positive\n";
      Literal literal;
      literal.tag   = Tag::Literal::number_integer_positive;
      literal.kind  = Kind::Literal::integer;
      literal.source = in.string();
      literal.int_value = std::stoi(in.string());
      context.literals.push_back(literal);
    }
  };

  rule (literal::number::integer::negative) {
    action_apply {
      std::cout << "Number - negative -- performing negation\n";
      Literal literal = context.literals.back();
      assert(literal.kind == Kind::Literal::integer);
      literal.int_value *= -1;
    }
  };

  rule (literal::number::special::divisible_by8) {
    action_apply {
      Literal literal = context.literals.back();
      bool ok = literal.int_value % 8 == 0;
      std::cout << "Number - divisible_by8 -- check? " << ok << "\n";
      assert(ok);
    }
  };

  // Label
  //

  rule (identifier::label) {
    action_apply {
      std::cout << "Label\n";
      Literal literal;
      literal.tag = Tag::Literal::identifier_label;
      literal.kind = Kind::Literal::string;
      literal.source = in.string();
      literal.source.erase(0, 1); // drop ':'
      context.literals.push_back(literal);
    }
  };

  // Operands
  //

  rule (operand::movable) {
    action_apply {
      std::cout << "Operand - movable\n";
      // The register, number, or label we last parsed is movable
      context.literals.back().tag = Tag::Literal::operand_movable;
      /* Literal literal; */
      /* literal.tag = Tag::Literal::operand_movable; */
      /* literal.kind = Kind::Literal::string; */
      /* literal.source = in.string(); */
      /* context.literals.push_back(literal); */
    }
  };

  rule (operand::callable) {
    action_apply {
      std::cout << "Operand - callable\n";
      // The register or label we last parsed is callable
      context.literals.back().tag = Tag::Literal::operand_movable;
      /* Literal literal; */
      /* literal.tag = Tag::Literal::operand_callable; */
      /* literal.kind = Kind::Literal::string; */
      /* literal.source = in.string(); */
      /* context.literals.push_back(literal); */
    }
  };

  rule (operand::comparable) {
    action_apply {
      std::cout << "Operand - comparable\n";
      // The register or number we last parsed is comparable
      context.literals.back().tag = Tag::Literal::operand_comparable;
      /* Literal literal; */
      /* literal.tag = Tag::Literal::operand_comparable; */
      /* literal.kind = Kind::Literal::string; */
      /* literal.source = in.string(); */
      /* context.literals.push_back(literal); */
    }
  };

  // Registers
  //

  rule (register_set::usable) {
    action_apply {
      std::cout << "Register - usable\n";
      Literal literal;
      literal.tag = Tag::Literal::register_usable;
      literal.kind = Kind::Literal::x86_64_register;
      literal.source = in.string();
      context.literals.push_back(literal);
    }
  };

  rule (register_set::memory) {
    action_apply {
      std::cout << "Register - memory\n";
      Literal literal;
      literal.tag = Tag::Literal::register_memory;
      literal.kind = Kind::Literal::x86_64_register;
      literal.source = in.string();
      context.literals.push_back(literal);
    }
  };

  rule (register_set::shift) {
    action_apply {
      std::cout << "Register - shift\n";
      Literal literal;
      literal.tag = Tag::Literal::register_shift;
      literal.kind = Kind::Literal::x86_64_register;
      literal.source = in.string();
      context.literals.push_back(literal);
    }
  };

  // Expressions
  //

  rule (expression::mem) {
    action_apply {
      std::cout << "Expression - mem x M\n";
      Expression expr;
      expr.tag = Tag::Expression::mem;
      take_end_vec(2, expr.literals, context.literals);
      context.expressions.push_back(expr);
    }
  };

  rule (expression::cmp) {
    action_apply {
      std::cout << "Expression - t cmp t\n";
      Expression expr;
      expr.tag = Tag::Expression::cmp;
      take_end_vec(2, expr.literals, context.literals);
      context.expressions.push_back(expr);
    }
  };

  // Operators
  //

  Literal op_literal (Tag::Literal tag, std::string value) {
    Literal literal;
    literal.tag = tag;
    literal.kind = Kind::Literal::op;
    literal.source = value;
    return literal;
  }

  // aop
  rule (op::add) {
    action_apply {
      std::cout << "Op - +=\n";
      Literal literal
        = op_literal(Tag::Literal::op_add, in.string());
      context.literals.push_back(literal);
    }
  };

  rule (op::subtract) {
    action_apply {
      std::cout << "Op - -=\n";
      Literal literal
        = op_literal(Tag::Literal::op_subtract, in.string());
      context.literals.push_back(literal);
    }
  };

  rule (op::multiply) {
    action_apply {
      std::cout << "Op - *=\n";
      Literal literal
        = op_literal(Tag::Literal::op_multiply, in.string());
      context.literals.push_back(literal);
    }
  };

  rule (op::bitwise_and) {
    action_apply {
      std::cout << "Op - &=\n";
      Literal literal
        = op_literal(Tag::Literal::op_bitwise_and, in.string());
      context.literals.push_back(literal);
    }
  };

  // cmp
  rule (op::less) {
    action_apply {
      std::cout << "Op - <\n";
      Literal literal
        = op_literal(Tag::Literal::op_less, in.string());
      context.literals.push_back(literal);
    }
  };

  rule (op::equal) {
    action_apply {
      std::cout << "Op - =\n";
      Literal literal
        = op_literal(Tag::Literal::op_equal, in.string());
      context.literals.push_back(literal);
    }
  };

  rule (op::less_equal) {
    action_apply {
      std::cout << "Op - <=\n";
      Literal literal
        = op_literal(Tag::Literal::op_less_equal, in.string());
      context.literals.push_back(literal);
    }
  };

  // sop
  rule (op::shift_left) {
    action_apply {
      std::cout << "Op - <<=\n";
      Literal literal
        = op_literal(Tag::Literal::op_shift_left, in.string());
      context.literals.push_back(literal);
    }
  };

  rule (op::shift_right) {
    action_apply {
      std::cout << "Op - >>=\n";
      Literal literal
        = op_literal(Tag::Literal::op_shift_right, in.string());
      context.literals.push_back(literal);
    }
  };

  //
  // Controls
  //

  // Base control
  template <typename Rule> struct control : peg::normal<Rule> {};

  // Control utility macros
  /* #define change_context(C, S)                                             \ */
  /*   template <> struct control <grammar::C>                              \ */
  /*     : peg::change_context<grammar::C, L1::S> {} */
  /* #define switch_context(C, S, A)                                        \ */
  /*   template <> struct control <grammar::C>                              \ */
  /*     : peg::change_context_and_action<grammar::C, L1::S, A> {} */

  /* change_state (program, Program); */
  /* change_state (function, Function); */
  /* change_state (identifier::label, Literal<>); */
  /* change_state (literal::number::integer::positive, Literal<>); */

  /* switch_context (instruction::any, Instruction<>, instruction); */
  /* switch_context (register_set::shift, Literal<>, register_set); */
  /* switch_context (register_set::usable, Literal<>, register_set); */
  /* switch_context (register_set::memory, Literal<>, register_set); */

  /* change_state (literal::number::integer::any, Literal<>); */
  /* change_state (literal::number::special::scale, Literal<>); */
  /* change_state (literal::number::integer::negative, Literal<>); */
  /* change_state (literal::number::special::divisible_by8, Literal<>); */
}
