#pragma once

#include <cassert>
#include <fstream>
#include <iostream>

#include "tao/pegtl.hpp"

#include "grammar.h"
#include "ast.h"

namespace codegen::L1::generate {
  namespace grammar = grammar::L1;
  namespace ast = ast::L1;
  using namespace ast;

  namespace helper {
    /* NOTE(jordan): nothing about this helper is L1-specific due to
     * templating; should move it into a common utils?
     */
    template <typename Rule>
    bool matches (const std::string string) {
      // WAFKQUSKWLZAQWAAAA YES I AM THE T<EMPL>ATE RELEASER OF Z<ALGO>
      using namespace tao::pegtl;
      memory_input<> string_input (string, "");
      return normal<Rule>::template
        match<apply_mode::NOTHING, rewind_mode::DONTCARE, nothing, normal>
        (string_input);
    }
    template <typename Rule>
    bool matches (const node & n) {
      assert(n.has_content() && "matches: must have content!");
      const std::string & content = n.content();
      return matches<Rule>(content);
    }
  }

  namespace helper {
    std::string register_to_lower8 (std::string rxx) {
      if (rxx ==  "r8") return  "r8b";
      if (rxx ==  "r9") return  "r9b";
      if (rxx == "r10") return "r10b";
      if (rxx == "r11") return "r11b";
      if (rxx == "r12") return "r12b";
      if (rxx == "r13") return "r13b";
      if (rxx == "r14") return "r14b";
      if (rxx == "r15") return "r15b";
      if (rxx == "rax") return   "al";
      if (rxx == "rbp") return  "bpl";
      if (rxx == "rbx") return   "bl";
      if (rxx == "rcx") return   "cl";
      if (rxx == "rdi") return  "dil";
      if (rxx == "rdx") return   "dl";
      if (rxx == "rsi") return  "sil";
      assert(false && "register_to_lower8: unreachable!");
    }
    std::string register_to_lower8 (const node & n) {
      assert(n.has_content()
          && "helper::register_to_lower8: no content!");
      return register_to_lower8(n.content());
    }
  }

  namespace helper {
    char constant_prefix = '$';
    void constant  (const node & n, std::ostream & os) {
      assert(n.has_content() && "helper::constant: no content!");
      os << constant_prefix << n.content();
    }
  }

  namespace helper {
    void gas_register (const node & n, std::ostream & os) {
      assert(n.has_content() && "helper::gas_register: no content!");
      std::string name = n.content();
      os << "%" << name;
    }
    void gas_register (std::string name, std::ostream & os) {
      os << "%" << name;
    }
  }

  namespace helper {
    int integer (const node & n) {
      assert(n.has_content() && "helper::integer: no content!");
      return std::stoi(n.content());
    }
  }

  namespace helper::label {
    std::string get_underscore_name (const node & n) {
      assert(n.children.size() == 1 && "label: no name child!");
      const node & name = *n.children.at(0);
      assert(name.has_content() && "label: name has no content!");
      return "_" + name.content();
    }
  }

  namespace helper::expression {
    void mem (const node & n, std::ostream & os) {
      assert(n.children.size() == 2);
      node & reg     = *n.children.at(0);
      node & integer = *n.children.at(1);
      assert(integer.has_content()
          && "helper::expression::mem: integer: no content!");
      std::string offset = integer.content();
      os << offset << "(";
      helper::gas_register(reg, os);
      os << ")";
    }
  }

  namespace helper::op {
    void aop (const node & n, std::ostream & os) {
      if (n.is<grammar::op::add>())         { os <<  "addq"; return; }
      if (n.is<grammar::op::subtract>())    { os <<  "subq"; return; }
      if (n.is<grammar::op::multiply>())    { os << "imulq"; return; }
      if (n.is<grammar::op::bitwise_and>()) { os <<  "andq"; return; }
      assert(false && "op::aop: unreachable!");
    }
    void sop (const node & n, std::ostream & os) {
      if (n.is<grammar::op::shift_left>())  { os << "salq"; return; }
      if (n.is<grammar::op::shift_right>()) { os << "sarq"; return; }
      assert(false && "op::sop: unreachable!");
    }
  }

  namespace helper::cmp {
    std::string op_l_suffix (std::string op) {
      if (op ==  "=") return "e";
      if (op ==  "<") return "l";
      if (op == "<=") return "le";
      assert(false && "op_l_suffix: unreachable!");
    }
    std::string op_l_suffix (const node & n) {
      assert(n.has_content() && "helper::cmp::op_l_suffix: no content!");
      return op_l_suffix(n.content());
    }
    std::string op_g_suffix (std::string op) {
      if (op ==  "=") return "e";
      if (op ==  "<") return "g";
      if (op == "<=") return "ge";
      assert(false && "op_g_suffix: unreachable!");
    }
    std::string op_g_suffix (const node & n) {
      assert(n.has_content() && "helper::cmp::op_g_suffix: no content!");
      return op_g_suffix(n.content());
    }
    void setl_le_e (
      const node & dest,
      const node & op,
      std::ostream & os
    ) {
      std::string d8 = helper::register_to_lower8(dest);
      os << "set" << helper::cmp::op_l_suffix(op);
      os  << " ";
      helper::gas_register(d8, os);
    }
    void setg_ge_e (
      const node & dest,
      const node & op,
      std::ostream & os
    ) {
      std::string d8 = helper::register_to_lower8(dest);
      os << "set" << helper::cmp::op_g_suffix(op);
      os << " ";
      helper::gas_register(d8, os);
    }
    void jl_le_e (const node & op, std::ostream & os) {
      os << "j" << helper::cmp::op_l_suffix(op);
    }
    void jg_ge_e (const node & op, std::ostream & os) {
      os << "j" << helper::cmp::op_g_suffix(op);
    }
    void movzbq (const node & dest, std::ostream & os) {
      std::string d8 = helper::register_to_lower8(dest);
      os << "movzbq ";
      helper::gas_register(d8, os);
      os << ", ";
      helper::gas_register(dest, os);
    }
    void rr (const node & lhs, const node & rhs, std::ostream & os) {
      // at&t is bardswack but that's just how it do
      os << "cmpq ";
      helper::gas_register(rhs, os);
      os << ", ";
      helper::gas_register(lhs, os);
    }
    void rc (const node & reg, const node & con, std::ostream & os) {
      os << "cmpq ";
      helper::constant(con, os);
      os << ", ";
      helper::gas_register(reg, os);
    }
  }

  namespace helper::cmp::predicate {
    bool constant (const node & n) {
      assert(n.children.size() == 1);
      const node & value = *n.children.at(0);
      return n.is<grammar::operand::comparable>()
        && helper::matches<grammar::literal::number::integer::any>(value);
    }
    bool reg (const node & n) {
      assert(n.children.size() == 1);
      const node & value = *n.children.at(0);
      return n.is<grammar::operand::comparable>()
        && helper::matches<grammar::register_set::any>(value);
    }
  }

  void label (const node & n, std::ostream & os) {
    os << helper::label::get_underscore_name(n);
  }

  namespace operand {
    void movable (const node & n, std::ostream & os) {
      assert(n.children.size() == 1);
      const node & value = *n.children.at(0);
      if (value.is<grammar::literal::number::integer::any>()) {
        helper::constant(value, os);
        return;
      }
      if (value.is<grammar::identifier::label>()) {
        os << helper::constant_prefix;
        generate::label(value, os);
        return;
      }
      if (helper::matches<grammar::register_set::any>(value)) {
        helper::gas_register(value, os);
        return;
      }
      assert(false && "operand::movable: unreachable!");
    }
    void comparable (const node & n, std::ostream & os) {
      assert(n.children.size() == 1);
      const node & value = *n.children.at(0);
      if (value.is<grammar::literal::number::integer::any>()) {
        helper::constant(value, os);
        return;
      }
      if (helper::matches<grammar::register_set::any>(value)) {
        helper::gas_register(value, os);
        return;
      }
      assert(false && "operand::comparable: unreachable!");
    }
  }

  void instruction (
    const node & n,
    int args,
    int locals,
    std::ostream & os
  ) {
    using namespace grammar::instruction;

    if (n.is<grammar::instruction::any>()) {
      assert(n.children.size() == 1);
      const node & actual_instruction = *n.children.at(0);
      generate::instruction(actual_instruction, args, locals, os);
      return;
    }

    if (n.is<assign::assignable::gets_movable>()) {
      assert(n.children.size() == 2);
      const node & dest = *n.children.at(0);
      const node & src  = *n.children.at(1);
      os << "movq ";
      operand::movable(src, os);
      os << ", ";
      helper::gas_register(dest, os);
      return;
    }

    if (n.is<assign::assignable::gets_relative>()) {
      assert(n.children.size() == 2);
      const node & dest = *n.children.at(0);
      const node & src  = *n.children.at(1);
      os << "movq ";
      helper::expression::mem(src, os);
      os << ", ";
      helper::gas_register(dest, os);
      return;
    }

    if (n.is<assign::relative::gets_movable>()) {
      assert(n.children.size() == 2);
      const node & dest = *n.children.at(0);
      const node & src  = *n.children.at(1);
      os << "movq ";
      operand::movable(src, os);
      os << ", ";
      helper::expression::mem(dest, os);
      return;
    }

    if (n.is<update::assignable::arithmetic::comparable>()) {
      assert(n.children.size() == 3);
      const node & dest = *n.children.at(0);
      const node & op   = *n.children.at(1);
      const node & src  = *n.children.at(2);
      helper::op::aop(op, os);
      os << " ";
      operand::comparable(src, os);
      os << ", ";
      helper::gas_register(dest, os);
      return;
    }

    if (n.is<update::assignable::shift::shift>()) {
      assert(n.children.size() == 3);
      const node & dest = *n.children.at(0);
      const node & op   = *n.children.at(1);
      const node & src  = *n.children.at(2);
      auto src8 = helper::register_to_lower8(src);
      helper::op::sop(op, os);
      os << " ";
      helper::gas_register(src8, os);
      os << ", ";
      helper::gas_register(dest, os);
      return;
    }

    if (n.is<update::assignable::shift::number>()) {
      assert(n.children.size() == 3);
      const node & dest = *n.children.at(0);
      const node & op   = *n.children.at(1);
      const node & con  = *n.children.at(2);
      helper::op::sop(op, os);
      os << " ";
      helper::constant(con, os);
      os << ", ";
      helper::gas_register(dest, os);
      return;
    }

    if (n.is<update::relative::arithmetic::add_comparable>()
        || n.is<update::relative::arithmetic::subtract_comparable>()) {
      assert(n.children.size() == 3);
      const node & dest = *n.children.at(0);
      const node & op   = *n.children.at(1);
      const node & src  = *n.children.at(2);
      helper::op::aop(op, os);
      os << " ";
      operand::comparable(src, os);
      os << ", ";
      helper::expression::mem(dest, os);
      return;
    }

    if (n.is<update::assignable::arithmetic::add_relative>()
        || n.is<update::assignable::arithmetic::subtract_relative>()) {
      assert(n.children.size() == 3);
      const node & dest = *n.children.at(0);
      const node & op   = *n.children.at(1);
      const node & src  = *n.children.at(2);
      helper::op::aop(op, os);
      os << " ";
      helper::expression::mem(src, os);
      os << ", ";
      helper::gas_register(dest, os);
      return;
    }

    if (n.is<assign::assignable::gets_comparison>()) {
      namespace predicate = helper::cmp::predicate;
      assert(n.children.size() == 2);
      const node & dest = *n.children.at(0);
      const node & cmp  = *n.children.at(1);
      assert(cmp.children.size() == 3);
      const node & lhs  = *cmp.children.at(0);
      const node & op   = *cmp.children.at(1);
      const node & rhs  = *cmp.children.at(2);
      if (predicate::constant(lhs) && predicate::constant(rhs)) {
        assert(op.has_content() && "gets_comparison: op: no content!");
        assert(lhs.has_content() && "gets_comparison: lhs: no content!");
        assert(rhs.has_content() && "gets_comparison: rhs: no content!");
        std::string op_  = op.content();
        int lhs_ = helper::integer(lhs);
        int rhs_ = helper::integer(rhs);
        os << "movq ";
        if      (op_ ==  "<" && lhs_  < rhs_) os << "$1";
        else if (op_ ==  "=" && lhs_ == rhs_) os << "$1";
        else if (op_ == "<=" && lhs_ <= rhs_) os << "$1";
        else                                  os << "$0";
        os << ", ";
        helper::gas_register(dest, os);
        return;
      } else if (predicate::constant(lhs) && predicate::reg(rhs)) {
        helper::cmp::rc(rhs, lhs, os);        os << "\n  ";
        helper::cmp::setg_ge_e(dest, op, os); os << "\n  ";
        helper::cmp::movzbq(dest, os);
        return;
      } else if (predicate::reg(lhs) && predicate::constant(rhs)) {
        helper::cmp::rc(lhs, rhs, os);        os << "\n  ";
        helper::cmp::setl_le_e(dest, op, os); os << "\n  ";
        helper::cmp::movzbq(dest, os);
        return;
      } else if (predicate::reg(lhs) && predicate::reg(rhs)) {
        helper::cmp::rr(lhs, rhs, os);        os << "\n  ";
        helper::cmp::setl_le_e(dest, op, os); os << "\n  ";
        helper::cmp::movzbq(dest, os);
        return;
      }
      assert(false
          && "assign::assignable::gets_comparison: unreachable!");
    }

    if (n.is<jump::cjump::if_else>()) {
      namespace predicate = helper::cmp::predicate;
      assert(n.children.size() == 3);
      const node & cmp  = *n.children.at(0);
      assert(cmp.children.size() == 3);
      const node & lhs  = *cmp.children.at(0);
      const node & op   = *cmp.children.at(1);
      const node & rhs  = *cmp.children.at(2);
      const node & then = *n.children.at(1);
      const node & els  = *n.children.at(2);
      std::string then_label = helper::label::get_underscore_name(then);
      std::string else_label = helper::label::get_underscore_name(els);
      if (predicate::constant(lhs) && predicate::constant(rhs)) {
        assert(op.has_content() && "gets_comparison: op: no content!");
        assert(lhs.has_content() && "gets_comparison: lhs: no content!");
        assert(rhs.has_content() && "gets_comparison: rhs: no content!");
        std::string op_  = op.content();
        int lhs_ = helper::integer(lhs);
        int rhs_ = helper::integer(rhs);
        os << "jmp ";
        if      (op_ ==  "<" && lhs_  < rhs_) os << then_label;
        else if (op_ ==  "=" && lhs_ == rhs_) os << then_label;
        else if (op_ == "<=" && lhs_ <= rhs_) os << then_label;
        else                                  os << else_label;
        return;
      } else if (predicate::constant(lhs) && predicate::reg(rhs)) {
        helper::cmp::rc(rhs, lhs, os);
        os << "\n  ";
        helper::cmp::jg_ge_e(op, os);
        os << " "    << then_label << "\n  ";
        os << "jmp " << else_label;
        return;
      } else if (predicate::reg(lhs) && predicate::constant(rhs)) {
        helper::cmp::rc(lhs, rhs, os);
        os << "\n  ";
        helper::cmp::jl_le_e(op, os);
        os << " "    << then_label << "\n  ";
        os << "jmp " << else_label;
        return;
      } else if (predicate::reg(lhs) && predicate::reg(rhs)) {
        helper::cmp::rr(lhs, rhs, os);
        os << "\n  ";
        helper::cmp::jl_le_e(op, os);
        os << " "    << then_label << "\n  ";
        os << "jmp " << else_label;
        return;
      }
      assert(false && "jump::cjump::if_else: unreachable!");
    }

    if (n.is<jump::cjump::when>()) {
      namespace predicate = helper::cmp::predicate;
      assert(n.children.size() == 2);
      const node & cmp  = *n.children.at(0);
      assert(cmp.children.size() == 3);
      const node & lhs  = *cmp.children.at(0);
      const node & op   = *cmp.children.at(1);
      const node & rhs  = *cmp.children.at(2);
      const node & then = *n.children.at(1);
      std::string then_label = helper::label::get_underscore_name(then);
      if (predicate::constant(lhs) && predicate::constant(rhs)) {
        assert(op.has_content() && "gets_comparison: op: no content!");
        assert(lhs.has_content() && "gets_comparison: lhs: no content!");
        assert(rhs.has_content() && "gets_comparison: rhs: no content!");
        std::string op_  = op.content();
        std::string lhs_ = lhs.content();
        std::string rhs_ = rhs.content();
        if      (op_ ==  "<" && lhs_  < rhs_) os << "jmp " << then_label;
        else if (op_ ==  "=" && lhs_ == rhs_) os << "jmp " << then_label;
        else if (op_ == "<=" && lhs_ <= rhs_) os << "jmp " << then_label;
        return;
      } else if (predicate::constant(lhs) && predicate::reg(rhs)) {
        helper::cmp::rc(rhs, lhs, os);
        os << "\n  ";
        helper::cmp::jg_ge_e(op, os);
        os << " " << then_label;
        return;
      } else if (predicate::reg(lhs) && predicate::constant(rhs)) {
        helper::cmp::rc(lhs, rhs, os);
        os << "\n  ";
        helper::cmp::jl_le_e(op, os);
        os << " " << then_label;
        return;
      } else if (predicate::reg(lhs) && predicate::reg(rhs)) {
        helper::cmp::rr(lhs, rhs, os);
        os << "\n  ";
        helper::cmp::jl_le_e(op, os);
        os << " " << then_label;
        return;
      }
      assert(false && "jump::cjump::when: unreachable!");
    }

    if (n.is<define::label>()) {
      assert(n.children.size() == 1);
      const node & label = *n.children.at(0);
      generate::label(label, os);
      os << ":";
      return;
    }

    if (n.is<jump::go2>()) {
      assert(n.children.size() == 1);
      const node & label = *n.children.at(0);
      os << "jmp ";
      generate::label(label, os);
      return;
    }

    if (n.is<invoke::ret>()) {
      int stack = 8 * locals;
      if (args  > 6) stack += 8 * (args - 6);
      if (stack > 0) {
        os << "addq $" << stack << ", %rsp\n";
        os << "  "; // Reindent
      }
      os << "ret";
      return;
    }

    if (n.is<invoke::call::callable>()) {
      assert(n.children.size() == 2);
      const node & callable = *n.children.at(0);
      const node & integer  = *n.children.at(1);
      assert(callable.children.size() == 1);
      const node & value = *callable.children.at(0);
      int args  = helper::integer(integer);
      int spill = 8; // return address!
      if (args  > 6) spill += 8 * (args - 6);
      os << "subq $" << spill << ", %rsp\n  ";
      os << "jmp ";
      if (value.is<grammar::operand::assignable>()) {
        os << "*"; // at&t indirect jump
        helper::gas_register(value, os);
        return;
      }
      if (value.is<grammar::identifier::label>()) {
        label(value, os);
        return;
      }
      assert(false && "invoke::call::callable: unreachable!");
    }

    if (n.is<invoke::call::intrinsic::print>()) {
      os << "call print";
      return;
    }

    if (n.is<invoke::call::intrinsic::allocate>()) {
      os << "call allocate";
      return;
    }

    if (n.is<invoke::call::intrinsic::array_error>()) {
      os << "call array_error";
      return;
    }

    if (n.is<update::assignable::arithmetic::increment>()) {
      assert(n.children.size() == 2); // ignore '++'
      const node & dest = *n.children.at(0);
      os << "inc ";
      helper::gas_register(dest, os);
      return;
    }

    if (n.is<update::assignable::arithmetic::decrement>()) {
      assert(n.children.size() == 2); // ignore '--'
      const node & dest = *n.children.at(0);
      os << "dec ";
      helper::gas_register(dest, os);
      return;
    }

    if (n.is<assign::assignable::gets_address>()) {
      assert(n.children.size() == 5);
      const node & dest   = *n.children.at(0);
      const node & _op    = *n.children.at(1); // ignore '@'
      const node & base   = *n.children.at(2);
      const node & offset = *n.children.at(3);
      const node & scale  = *n.children.at(4);
      os << "lea ";
        os << "(" ; helper::gas_register(base, os);
        os << ", "; helper::gas_register(offset, os);
        os << ", "; os << helper::integer(scale);
        os << ")";
      os << ", ";
      helper::gas_register(dest, os);
      return;
    }

    std::cout << "something went wrong at\n\t" << n.name() << "\n";
    assert(false && "generate::instruction: unreachable!");
  }

  void instructions (
    const node & n,
    int args,
    int locals,
    std::ostream & os
  ) {
    for (auto & child : n.children) {
      assert(child->is<grammar::instruction::any>()
          && "instructions: got non-instruction!");
      os << "  "; // indent the instruction
      generate::instruction(*child, args, locals, os);
      os << "\n"; // make a new line after it
    }
    return;
  }

  void function (const node & n, std::ostream & os) {
    assert(n.children.size() == 4);
    const node & name         = *n.children.at(0);
    const node & arg_count    = *n.children.at(1);
    const node & local_count  = *n.children.at(2);
    const node & instructions = *n.children.at(3);
    int args   = helper::integer(arg_count);
    int locals = helper::integer(local_count);
    label(name, os); os << ":\n";
    if (locals > 0)  os << "  subq $" << 8 * locals << ", %rsp\n";
    return generate::instructions(instructions, args, locals, os);
  }

  void functions (const node & n, std::ostream & os) {
    for (auto & child : n.children) {
      assert(child->is<grammar::function::define>()
          && "functions: got non-function!");
      generate::function(*child, os);
    }
    return;
  }

  void program (const node & n, std::ostream & os) {
    assert(n.children.size() == 2);
    assert(n.is<grammar::program::define>() && "top is not a program!");
    const node & entry     = *n.children.at(0);
    const node & functions = *n.children.at(1);
    os << ".text\n";
    os << "  .globl go\n";
    os << "go:\n";
    os << "  pushq %rbx\n";
    os << "  pushq %rbp\n";
    os << "  pushq %r12\n";
    os << "  pushq %r13\n";
    os << "  pushq %r14\n";
    os << "  pushq %r15\n";
    os << "  call "; generate::label(entry, os); os << "\n";
    os << "  popq %r15\n";
    os << "  popq %r14\n";
    os << "  popq %r13\n";
    os << "  popq %r12\n";
    os << "  popq %rbp\n";
    os << "  popq %rbx\n";
    os << "  retq\n";
    return generate::functions(functions, os);
  }

  void root (const node & root, std::ostream & os) {
    assert(root.is_root() && "generate: got a non-root node!");
    assert(!root.children.empty() && "generate: got an empty AST!");
    assert(root.children.size() == 1);
    return generate::program(*root.children.at(0), os);
  }

  void to_file (std::string file_name, const node & root) {
    std::ofstream out;
    out.open(file_name);
    generate::root(root, out);
    out.close();
  }
}
