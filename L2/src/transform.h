// vim: set foldmethod=marker:
#include <sstream>
#include <iostream>

#include "L1/codegen.h"
#include "grammar.h"
#include "ast.h"

namespace transform::L2::spill {
  namespace grammar = grammar::L2;
  using namespace ast::L2;
  using namespace grammar::instruction;

  // helper {{{
  // FIXME(jordan): copy/pasta
  namespace helper {
    namespace L1_helper = codegen::L1::generate::helper;
    template <class R>
    bool matches (const node & n)  { return L1_helper::matches<R>(n); }
    template <class R>
    bool matches (const std::string & s) { return L1_helper::matches<R>(s); }
  }

  namespace helper {
    int integer (const node & n) {
      assert(n.has_content() && "helper::integer: no content!");
      return std::stoi(n.content());
    }
  }

  namespace helper {
    std::string relative (const node & n) {
      assert(n.children.size() == 2 && "mem: incorrect # children");
      const node & base   = *n.children.at(0);
      const node & offset = *n.children.at(1);
      return "mem " + base.content() + " " + offset.content();
    }
  }

  namespace helper {
    bool spills (const node & operand, const node & target) {
      assert(operand.has_content() && "spills: operand has no content!");
      return true
        && matches<grammar::identifier::variable>(operand)
        && operand.content() == target.content();
    }
    std::string make_spill (const node & prefix, int & spills) {
      return prefix.content() + std::to_string(spills++);
    }
    void get_spill (const int & offset, std::ostream & os) {
      os << "mem rsp " << offset;
    }
    void save_spill (const std::string & spilled, const int & offset, std::ostream & os) {
      helper::get_spill(offset, os);
      os << " <- " << spilled;
    }
    void load_spill (const std::string & dest, const int & offset, std::ostream & os) {
      os << dest << " <- ";
      helper::get_spill(offset, os);
    }
  }
  // }}}

  void instruction (
    const node & n,
    const node & target,
    const node & prefix,
    int & spills,
    const int & offset,
    std::ostream & os
  ) {
    if (n.is<grammar::instruction::any>()) {
      assert(n.children.size() == 1);
      const node & unwrapped = *n.children.at(0);
      spill::instruction(unwrapped, target, prefix, spills, offset, os);
      return;
    }

    if (n.is<assign::assignable::gets_stack_arg>()) {
      assert(n.children.size() == 2);
      const node & dest = *n.children.at(0);
      const node & stack_arg = *n.children.at(1);
      if (helper::spills(dest, target)) {
        // %SN <- stack-arg M
        // %SN <- mem rsp OFF
        std::string spilled = helper::make_spill(prefix, spills);
        helper::load_spill(spilled, offset, os);
        os << "\n\t";
        // %SN <- stack-arg M
        os << spilled << " <- " << stack_arg.content();
        os << "\n\t";
        // mem rsp OFF <- %SN
        helper::save_spill(spilled, offset, os);
      } else {
        os << dest.content() << " <- " << stack_arg.content();
      }
    }

    if (n.is<assign::assignable::gets_movable>()) {
      assert(n.children.size() == 2);
      const node & dest = *n.children.at(0);
      const node & src  = *n.children.at(1);
      if (helper::spills(dest, target) && helper::spills(src, target)) {
        /**
         * NOTE(jordan):
         *
         * This transformation is a little silly. In some contrived
         * situations (on platforms other than x86_64), then maybe the
         * semantics of the program will change when 'x <- x'. But they
         * *shouldn't*, and so this is a dumb no-op to preserve!
         *
         */
        // %SN <- %SN
        // %SN <- mem rsp OFF
        std::string spilled = helper::make_spill(prefix, spills);
        helper::load_spill(spilled, offset, os);
        os << "\n\t";
        // %SN <- %SN
        os << spilled << " <- " << spilled;
        os << "\n\t";
        // mem rsp OFF <- %SN
        helper::save_spill(spilled, offset, os);
      } else if (helper::spills(dest, target)) {
        // %SN <- <movable>
        std::string spilled = helper::make_spill(prefix, spills);
        os << spilled << " <- " << src.content();
        os << "\n\t";
        // mem rsp OFF <- %SN
        helper::save_spill(spilled, offset, os);
      } else if (helper::spills(src, target)) {
        /**
         * QUESTION(jordan): Why not just:
         *
         * w <- mem rsp OFF
         *
         * ?? Tests will spill to variable.
         */
        // w <-  %SN
        // %SN <- mem rsp OFF
        std::string spilled = helper::make_spill(prefix, spills);
        helper::load_spill(spilled, offset, os);
        os << "\n\t";
        // X <- %SN
        os << dest.content() << " <- " << spilled;
      } else {
        os << dest.content() << " <- " << src.content();
      }
      return;
    }

    if (n.is<assign::assignable::gets_relative>()) {
      assert(n.children.size() == 2);
      const node & dest = *n.children.at(0);
      const node & src  = *n.children.at(1);
      if (helper::spills(dest, target)) {
        // %SN <- mem rsp OFF
        std::string spilled = helper::make_spill(prefix, spills);
        // %SN <- mem x M
        os << spilled << " <- " << helper::relative(src);
        os << "\n\t";
        // mem rsp OFF <- %SN
        helper::save_spill(spilled, offset, os);
      } else {
        os << dest.content() << " <- " << helper::relative(src);
      }
      return;
    }

    if (n.is<assign::relative::gets_movable>()) {
      assert(n.children.size() == 2);
      const node & dest = *n.children.at(0);
      const node & src  = *n.children.at(1);
      if (helper::spills(src, target)) {
        // %SN <- mem rsp OFF
        std::string spilled = helper::make_spill(prefix, spills);
        helper::load_spill(spilled, offset, os);
        os << "\n\t";
        // mem x M <- %SN
        os << helper::relative(dest) << " <- " << spilled;
      } else {
        os << helper::relative(dest) << " <- " << src.content();
      }
      return;
    }

    if (n.is<update::assignable::arithmetic::comparable>()) {
      assert(n.children.size() == 3);
      const node & dest = *n.children.at(0);
      const node & op   = *n.children.at(1);
      const node & src  = *n.children.at(2);
      // w {+,*,-,&}= t
      if (helper::spills(dest, target) && helper::spills(src, target)) {
        // %SN OP %SN
        // %SN <- mem rsp OFF
        std::string spilled = helper::make_spill(prefix, spills);
        helper::load_spill(spilled, offset, os);
        os << "\n\t";
        // %SN OP= %SN
        os << spilled
          << " " << op.content()
          << " " << spilled;
        os << "\n\t";
        // mem rsp OFF <- %SN
        helper::save_spill(spilled, offset, os);
      } else if (helper::spills(dest, target)) {
        // %SN OP t
        // %SN <- mem rsp OFF
        std::string spilled = helper::make_spill(prefix, spills);
        helper::load_spill(spilled, offset, os);
        os << "\n\t";
        // %SN OP= t
        os << spilled
          << " " << op.content()
          << " " << src.content();
        os << "\n\t";
        // mem rsp OFF <- %SN
        helper::save_spill(spilled, offset, os);
      } else if (helper::spills(src, target)) {
        // w OP %SN
        // += and -= allow mem x M; otherwise, no.
        if (op.is<grammar::op::add>() || op.is<grammar::op::subtract>()) {
          // w OP mem rsp OFF
          os << dest.content()
            << " " << op.content()
            << " ";
          helper::get_spill(offset, os);
        } else {
          // w OP %SN
          // %SN <- mem rsp OFF
          std::string spilled = helper::make_spill(prefix, spills);
          helper::load_spill(spilled, offset, os);
          os << "\n\t";
          // w OP %SN
          os << dest.content()
            << " " << op.content()
            << " " << spilled;
        }
      } else {
        os << dest.content()
          << " " << op.content()
          << " " << src.content();
      }
      return;
    }

    if (n.is<update::assignable::shift::shift>()) {
      assert(n.children.size() == 3);
      const node & dest = *n.children.at(0);
      const node & op   = *n.children.at(1);
      const node & src  = *n.children.at(2);
      if (helper::spills(dest, target) && helper::spills(src, target)) {
        // %SN SOP= %SN
        // %SN <- mem rsp OFF
        std::string spilled = helper::make_spill(prefix, spills);
        helper::load_spill(spilled, offset, os);
        os << "\n\t";
        // %SN SOP= %SN
        os << spilled << " " << op.content() << " " << spilled;
        os << "\n\t";
        // mem rsp OFF <- %SN
        helper::save_spill(spilled, offset, os);
      } else if (helper::spills(dest, target)) {
        // %SN SOP= sx
        // %SN <- mem rsp OFF
        std::string spilled = helper::make_spill(prefix, spills);
        helper::load_spill(spilled, offset, os);
        os << "\n\t";
        // %SN SOP= sx
        os << spilled << " " << op.content() << " " << src.content();
        os << "\n\t";
        // mem rsp OFF <- %SN
        helper::save_spill(spilled, offset, os);
      } else if (helper::spills(src, target)) {
        // w SOP %SN -- no relative.
        // %SN <- mem rsp OFF
        std::string spilled = helper::make_spill(prefix, spills);
        helper::load_spill(spilled, offset, os);
        os << "\n\t";
        // w SOP= %SN
        os << dest.content() << " " << op.content() << " " << spilled;
      } else {
        os << dest.content()
          << " " << op.content()
          << " " << src.content();
      }
      return;
    }

    if (n.is<update::assignable::shift::number>()) {
      assert(n.children.size() == 3);
      const node & dest = *n.children.at(0);
      const node & op   = *n.children.at(1);
      const node & con  = *n.children.at(2);
      if (helper::spills(dest, target)) {
        // %SN SOP= N -- no relative.
        // %SN <- mem rsp OFF
        std::string spilled = helper::make_spill(prefix, spills);
        helper::load_spill(spilled, offset, os);
        os << "\n\t";
        // %SN SOP= N
        os << spilled << " " << op.content() << " " << con.content();
        os << "\n\t";
        // mem rsp OFF <- %SN
        helper::save_spill(spilled, offset, os);
      } else {
        os << dest.content()
          << " " << op.content()
          << " " << con.content();
      }
      return;
    }

    if (n.is<update::relative::arithmetic::add_comparable>()
        || n.is<update::relative::arithmetic::subtract_comparable>()) {
      assert(n.children.size() == 3);
      const node & dest = *n.children.at(0);
      const node & op   = *n.children.at(1);
      const node & src  = *n.children.at(2);
      if (helper::spills(src, target)) {
        // mem x M <- %SN
        // %SN <- mem rsp OFF
        std::string spilled = helper::make_spill(prefix, spills);
        helper::load_spill(spilled, offset, os);
        os << "\n\t";
        // mem x M <- %SN
        os << helper::relative(dest) << " " << op.content() << " " << spilled;
      } else {
        os << helper::relative(dest)
          << " " << op.content()
          << " " << src.content();
      }
      return;
    }

    if (n.is<update::assignable::arithmetic::add_relative>()
        || n.is<update::assignable::arithmetic::subtract_relative>()) {
      assert(n.children.size() == 3);
      const node & dest = *n.children.at(0);
      const node & op   = *n.children.at(1);
      const node & src  = *n.children.at(2);
      if (helper::spills(dest, target)) {
        // %SN <- mem x M
        // %SN <- mem rsp OFF
        std::string spilled = helper::make_spill(prefix, spills);
        helper::load_spill(spilled, offset, os);
        os << "\n\t";
        // %SN <- mem x M
        os << spilled << " " << op.content() << " " << helper::relative(src);
        os << "\n\t";
        // mem rsp OFF <- %SN
        helper::save_spill(spilled, offset, os);
      } else {
        os << dest.content()
          << " " << op.content()
          << " " << helper::relative(src);
      }
      return;
    }

    if (n.is<assign::assignable::gets_comparison>()) {
      assert(n.children.size() == 2);
      const node & dest = *n.children.at(0);
      const node & cmp  = *n.children.at(1);
      assert(cmp.children.size() == 3);
      const node & lhs  = *cmp.children.at(0);
      const node & op   = *cmp.children.at(1);
      const node & rhs  = *cmp.children.at(2);
      if (true
        && helper::spills(dest, target)
        && helper::spills(lhs, target)
        && helper::spills(rhs, target)
      ) {
        // %SN <- %SN CMP %SN
        // %SN <- mem rsp OFF
        std::string spilled = helper::make_spill(prefix, spills);
        helper::load_spill(spilled, offset, os);
        os << "\n\t";
        // %SN <- %SN CMP %SN
        os << spilled
          << " <-"
          << " " << spilled
          << " " << op.content()
          << " " << spilled;
        os << "\n\t";
        // mem rsp OFF <- %SN
        helper::save_spill(spilled, offset, os);
      } else if (true
        && helper::spills(dest, target)
        && helper::spills(lhs, target)
      ) {
        // %SN <- %SN CMP t
        // %SN <- mem rsp OFF
        std::string spilled = helper::make_spill(prefix, spills);
        helper::load_spill(spilled, offset, os);
        os << "\n\t";
        // %SN <- %SN CMP t
        os << spilled
          << " <-"
          << " " << spilled
          << " " << op.content()
          << " " << rhs.content();
        os << "\n\t";
        // mem rsp OFF <- %SN
        helper::save_spill(spilled, offset, os);
      } else if (true
        && helper::spills(dest, target)
        && helper::spills(rhs, target)
      ) {
        // %SN <- t CMP %SN
        // %SN <- mem rsp OFF
        std::string spilled = helper::make_spill(prefix, spills);
        helper::load_spill(spilled, offset, os);
        os << "\n\t";
        // %SN <- t CMP %SN
        os << spilled
          << " <-"
          << " " << lhs.content()
          << " " << op.content()
          << " " << spilled;
        os << "\n\t";
        // mem rsp OFF <- %SN
        helper::save_spill(spilled, offset, os);
      } else if (true
        && helper::spills(lhs, target)
        && helper::spills(rhs, target)
      ) {
        // w <- %SN CMP %SN
        // %SN <- mem rsp OFF
        std::string spilled = helper::make_spill(prefix, spills);
        helper::load_spill(spilled, offset, os);
        os << "\n\t";
        // w <- %SN CMP %SN
        os << dest.content()
          << " <-"
          << " " << spilled
          << " " << op.content()
          << " " << spilled;
      } else if (true
        && helper::spills(lhs, target)
      ) {
        // w <- %SN CMP t
        // %SN <- mem rsp OFF
        std::string spilled = helper::make_spill(prefix, spills);
        helper::load_spill(spilled, offset, os);
        os << "\n\t";
        // w <- %SN CMP t
        os << dest.content()
          << " <-"
          << " " << spilled
          << " " << op.content()
          << " " << rhs.content();
      } else if (true
        && helper::spills(rhs, target)
      ) {
        // w <- t CMP %SN
        // %SN <- mem rsp OFF
        std::string spilled = helper::make_spill(prefix, spills);
        helper::load_spill(spilled, offset, os);
        os << "\n\t";
        // w <- t CMP %SN
        os << dest.content()
          << " <-"
          << " " << lhs.content()
          << " " << op.content()
          << " " << spilled;
      } else {
        os << dest.content()
          << " <- "
          << lhs.content()
          << " " << op.content()
          << " " << rhs.content();
      }
      return;
    }

    if (n.is<jump::cjump::if_else>()) {
      assert(n.children.size() == 3);
      const node & cmp  = *n.children.at(0);
      assert(cmp.children.size() == 3);
      const node & lhs  = *cmp.children.at(0);
      const node & op   = *cmp.children.at(1);
      const node & rhs  = *cmp.children.at(2);
      const node & then = *n.children.at(1);
      const node & els  = *n.children.at(2);
      if (true
        && helper::spills(lhs, target)
        && helper::spills(rhs, target)
      ) {
        // cjump %SN CMP %SN :then :else
        // %SN <- mem rsp OFF
        std::string spilled = helper::make_spill(prefix, spills);
        helper::load_spill(spilled, offset, os);
        os << "\n\t";
        // cjump %SN CMP %SN :then :else
        os << "cjump"
          << " " << spilled
          << " " << op.content()
          << " " << spilled
          << " " << then.content()
          << " " << els.content();
      } else if (true
        && helper::spills(lhs, target)
      ) {
        // cjump %SN CMP t :then :else
        // %SN <- mem rsp OFF
        std::string spilled = helper::make_spill(prefix, spills);
        helper::load_spill(spilled, offset, os);
        os << "\n\t";
        // cjump %SN CMP %SN :then :else
        os << "cjump"
          << " " << spilled
          << " " << op.content()
          << " " << rhs.content()
          << " " << then.content()
          << " " << els.content();
      } else if (true
        && helper::spills(rhs, target)
      ) {
        // cjump t CMP %SN :then :else
        // %SN <- mem rsp OFF
        std::string spilled = helper::make_spill(prefix, spills);
        helper::load_spill(spilled, offset, os);
        os << "\n\t";
        // cjump %SN CMP %SN :then :else
        os << "cjump"
          << " " << lhs.content()
          << " " << op.content()
          << " " << spilled
          << " " << then.content()
          << " " << els.content();
      } else {
        os << "cjump"
          << " " << lhs.content()
          << " " << op.content()
          << " " << rhs.content()
          << " " << then.content()
          << " " << els.content();
      }
      return;
    }

    if (n.is<jump::cjump::when>()) {
      assert(n.children.size() == 2);
      const node & cmp  = *n.children.at(0);
      assert(cmp.children.size() == 3);
      const node & lhs  = *cmp.children.at(0);
      const node & op   = *cmp.children.at(1);
      const node & rhs  = *cmp.children.at(2);
      const node & then = *n.children.at(1);
      if (true
        && helper::spills(lhs, target)
        && helper::spills(rhs, target)
      ) {
        // cjump %SN CMP %SN :then
        // %SN <- mem rsp OFF
        std::string spilled = helper::make_spill(prefix, spills);
        helper::load_spill(spilled, offset, os);
        os << "\n\t";
        // cjump %SN CMP %SN :then
        os << "cjump"
          << " " << spilled
          << " " << op.content()
          << " " << spilled
          << " " << then.content();
      } else if (true
        && helper::spills(lhs, target)
      ) {
        // cjump %SN CMP t :then
        // %SN <- mem rsp OFF
        std::string spilled = helper::make_spill(prefix, spills);
        helper::load_spill(spilled, offset, os);
        os << "\n\t";
        // cjump %SN CMP %SN :then
        os << "cjump"
          << " " << spilled
          << " " << op.content()
          << " " << rhs.content()
          << " " << then.content();
      } else if (true
        && helper::spills(rhs, target)
      ) {
        // cjump t CMP %SN :then
        // %SN <- mem rsp OFF
        std::string spilled = helper::make_spill(prefix, spills);
        helper::load_spill(spilled, offset, os);
        os << "\n\t";
        // cjump %SN CMP %SN :then
        os << "cjump"
          << " " << lhs.content()
          << " " << op.content()
          << " " << spilled
          << " " << then.content();
      } else {
        os << "cjump"
          << " " << lhs.content()
          << " " << op.content()
          << " " << rhs.content()
          << " " << then.content();
      }
      return;
    }

    if (n.is<define::label>()) {
      assert(n.children.size() == 1);
      const node & label = *n.children.at(0);
      os << label.content();
      return;
    }

    if (n.is<jump::go2>()) {
      assert(n.children.size() == 1);
      const node & label = *n.children.at(0);
      os << "goto"
        << " " << label.content();
      return;
    }

    if (n.is<invoke::ret>()) {
      os << "return";
      return;
    }

    if (n.is<invoke::call::callable>()) {
      assert(n.children.size() == 2);
      const node & callable = *n.children.at(0);
      const node & integer  = *n.children.at(1);
      assert(callable.children.size() == 1);
      const node & value = *callable.children.at(0);
      int args  = helper::integer(integer);
      if (helper::spills(callable, target)) {
        // call %SN N
        // %SN <- mem rsp OFF
        std::string spilled = helper::make_spill(prefix, spills);
        helper::load_spill(spilled, offset, os);
        os << "\n\t";
        // call %SN N
        os << "call"
          << " " << spilled
          << " " << args;
      } else {
        os << "call"
          << " " << callable.content()
          << " " << args;
      }
      return;
    }

    if (n.is<invoke::call::intrinsic::print>()) {
      os << "call print 1";
      return;
    }

    if (n.is<invoke::call::intrinsic::allocate>()) {
      os << "call allocate 2";
      return;
    }

    if (n.is<invoke::call::intrinsic::array_error>()) {
      os << "call array_error 2";
      return;
    }

    if (n.is<update::assignable::arithmetic::increment>()) {
      assert(n.children.size() == 2); // ignore '++'
      const node & dest = *n.children.at(0);
      if (helper::spills(dest, target)) {
        // %SN++
        // %SN <- mem rsp OFF
        std::string spilled = helper::make_spill(prefix, spills);
        helper::load_spill(spilled, offset, os);
        os << "\n\t";
        // %SN++
        os << spilled << "++";
        os << "\n\t";
        // mem rsp OFF <- %SN
        helper::save_spill(spilled, offset, os);
      } else {
        os << dest.content() << "++";
      }
      return;
    }

    if (n.is<update::assignable::arithmetic::decrement>()) {
      assert(n.children.size() == 2); // ignore '--'
      const node & dest = *n.children.at(0);
      if (helper::spills(dest, target)) {
        // %SN--
        // %SN <- mem rsp OFF
        std::string spilled = helper::make_spill(prefix, spills);
        helper::load_spill(spilled, offset, os);
        os << "\n\t";
        // %SN--
        os << spilled << "--";
        os << "\n\t";
        // mem rsp OFF <- %SN
        helper::save_spill(spilled, offset, os);
      } else {
        os << dest.content() << "--";
      }
      return;
    }

    if (n.is<assign::assignable::gets_address>()) {
      assert(n.children.size() == 5);
      const node & dest           = *n.children.at(0);
      const node & op             = *n.children.at(1); // ignore '@'
      const node & base           = *n.children.at(2);
      const node & address_offset = *n.children.at(3);
      const node & scale          = *n.children.at(4);
      if (true
        && helper::spills(dest, target)
        && helper::spills(address_offset, target)
        && helper::spills(base, target)
      ) {
        // %SN @ %SN %SN E
        // %SN <- mem rsp OFF
        std::string spilled = helper::make_spill(prefix, spills);
        helper::load_spill(spilled, offset, os);
        os << "\n\t";
        // %SN @ %SN %SN E
        os << spilled
          << " " << op.content()
          << " " << spilled
          << " " << spilled
          << " " << scale.content();
        // mem rsp OFF <- %SN
        helper::save_spill(spilled, offset, os);
      } else if (true
        && helper::spills(dest, target)
        && helper::spills(base, target)
      ) {
        // %SN @ %SN w E
        // %SN <- mem rsp OFF
        std::string spilled = helper::make_spill(prefix, spills);
        helper::load_spill(spilled, offset, os);
        os << "\n\t";
        os << spilled
          << " " << op.content()
          << " " << spilled
          << " " << address_offset.content()
          << " " << scale.content();
        // mem rsp OFF <- %SN
        helper::save_spill(spilled, offset, os);
      } else if (true
        && helper::spills(dest, target)
        && helper::spills(address_offset, target)
      ) {
        // %SN @ w %SN E
        // %SN <- mem rsp OFF
        std::string spilled = helper::make_spill(prefix, spills);
        helper::load_spill(spilled, offset, os);
        os << "\n\t";
        // %SN @ w %SN E
        os << spilled
          << " " << op.content()
          << " " << base.content()
          << " " << spilled
          << " " << scale.content();
        // mem rsp OFF <- %SN
        helper::save_spill(spilled, offset, os);
      } else if (true
        && helper::spills(base, target)
        && helper::spills(address_offset, target)
      ) {
        // w @ %SN %SN E
        // %SN <- mem rsp OFF
        std::string spilled = helper::make_spill(prefix, spills);
        helper::load_spill(spilled, offset, os);
        os << "\n\t";
        os << dest.content()
          << " " << op.content()
          << " " << spilled
          << " " << spilled
          << " " << scale.content();
      } else if (true
        && helper::spills(dest, target)
      ) {
        // %SN @ w w E
        // %SN <- mem rsp OFF
        std::string spilled = helper::make_spill(prefix, spills);
        helper::load_spill(spilled, offset, os);
        os << "\n\t";
        // %SN @ w w E
        os << spilled
          << " " << op.content()
          << " " << base.content()
          << " " << address_offset.content()
          << " " << scale.content();
        // mem rsp OFF <- %SN
        helper::save_spill(spilled, offset, os);
      } else if (true
        && helper::spills(base, target)
      ) {
        // w @ %SN w E
        // %SN <- mem rsp OFF
        std::string spilled = helper::make_spill(prefix, spills);
        helper::load_spill(spilled, offset, os);
        os << "\n\t";
        // w @ %SN w E
        os << dest.content()
          << " " << op.content()
          << " " << spilled
          << " " << address_offset.content()
          << " " << scale.content();
      } else if (true
        && helper::spills(address_offset, target)
      ) {
        // w @ w %SN E
        // %SN <- mem rsp OFF
        std::string spilled = helper::make_spill(prefix, spills);
        helper::load_spill(spilled, offset, os);
        os << "\n\t";
        // w @ w %SN E
        os << dest.content()
          << " " << op.content()
          << " " << base.content()
          << " " << spilled
          << " " << scale.content();
      } else {
        os << dest.content()
          << " " << op.content()
          << " " << base.content()
          << " " << address_offset.content()
          << " " << scale.content();
      }
      return;
    }

    std::cout << "something went wrong at\n\t" << n.name() << "\n";
    assert(false && "generate::instruction: unreachable!");
  }

  void instructions (
    const node & instructions,
    const node & target,
    const node & prefix,
    int & spills,
    const int & offset,
    std::ostream & os
  ) {
    for (auto & child : instructions.children) {
      assert(child->is<grammar::instruction::any>()
          && "instructions: got non-instruction!");
      os << "\t";
      spill::instruction(*child, target, prefix, spills, offset, os);
      os << "\n";
    }
    return;
  }

  void execute (
    const node & function,
    const node & target,
    const node & prefix,
    std::ostream & os
  ) {
    assert(function.children.size() == 4);
    const node & name         = *function.children.at(0);
    const node & arg_count    = *function.children.at(1);
    const node & local_count  = *function.children.at(2);
    const node & instructions = *function.children.at(3);
    int args   = helper::integer(arg_count);
    int locals = helper::integer(local_count);
    int spills = 0;
    int offset = 8 * locals;
    if (args > 6) offset += 8 * (args - 6);
    std::ostringstream ss;
    spill::instructions(instructions, target, prefix, spills, offset, ss);
    if (spills > 0) locals += 1;
    // redefine the function:
    // (<name>
    //    <args> <locals>
    //    <instructions>...
    // )
    os << "(" << name.content() << "\n";
    os << "\t" << args << " " << locals << "\n";
    os << ss.str();
    os << ")\n";
    return;
  }
}
