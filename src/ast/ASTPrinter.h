#pragma once
#include "AST.h"
#include <string>
#include <ostream>

namespace ptxra {
namespace ast {

/// Reconstructs canonical PTX text from an AST.
/// The output is parseable back to an equivalent AST (round-trip property).
class ASTPrinter {
public:
    explicit ASTPrinter(std::ostream& out) : out_(out) {}

    void print(const Module& m);
    void print(const Kernel& k);
    void print(const Param& p);
    void print(const RegDecl& rd);
    void print(const Stmt& st);
    void print(const Instruction& insn);
    void print(const Operand& op);
    void print(const GlobalVar& g);

private:
    std::ostream& out_;
    int indent_ = 0;

    void newline()       { out_ << '\n'; }
    void ind()           { for (int i = 0; i < indent_; ++i) out_ << "    "; }
    void emit(std::string_view s) { out_ << s; }

    std::string printMods(const InsnMods& m, Opcode opcode);
};

} // namespace ast
} // namespace ptxra
