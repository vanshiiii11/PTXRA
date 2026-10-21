#pragma once
#include <string>
#include <vector>
#include <memory>
#include <optional>
#include <variant>
#include "../lexer/SourceLocation.h"

namespace ptxra {
namespace ast {

// ============================================================
// Forward declarations
// ============================================================
struct Module;
struct Kernel;
struct Param;
struct RegDecl;
struct Stmt;
struct Instruction;
struct Label;
struct Operand;

// ============================================================
// PTX type as it appears in source (before IR lowering)
// ============================================================
enum class PtxType {
    Pred,
    B8, B16, B32, B64,
    U8, U16, U32, U64,
    S8, S16, S32, S64,
    F16, F32, F64,
    Unknown
};

std::string ptxTypeName(PtxType t);
PtxType     ptxTypeFromDotToken(const std::string& text);
int         ptxTypeBits(PtxType t);

// ============================================================
// Address space
// ============================================================
enum class AddrSpace { None, Global, Shared, Param, Local, Generic };
std::string addrSpaceName(AddrSpace s);

// ============================================================
// Memory order / comparison operator
// ============================================================
enum class CmpOp {
    Eq, Ne, Lt, Le, Gt, Ge,
    Equ, Neu, Ltu, Leu, Gtu, Geu,
    Num, Nan, Ls, Hs,
    None
};
std::string cmpOpName(CmpOp op);

// ============================================================
// Rounding mode modifier
// ============================================================
enum class RoundMode { Rn, Rz, Rm, Rp, None };
std::string roundModeName(RoundMode r);

// ============================================================
// Opcode (raw mnemonic before type-decorating modifiers)
// ============================================================
enum class Opcode {
    Mov, Add, Sub, Mul, Mad, Fma,
    Div, Rem, Neg, Abs, Min, Max,
    And, Or, Xor, Not,
    Shl, Shr,
    Cvt, Cvta,
    Setp, Selp,
    Ld, St,
    Bra, Ret, Exit,
    BarSync,
    Atom,
    // synthetic
    Unknown
};
std::string opcodeName(Opcode op);
Opcode opcodeFromString(const std::string& s);

// ============================================================
// Instruction modifiers collected during parse
// ============================================================
struct InsnMods {
    PtxType    type      = PtxType::Unknown;
    PtxType    srcType   = PtxType::Unknown; // cvt source type
    AddrSpace  space     = AddrSpace::None;
    CmpOp      cmpOp    = CmpOp::None;
    RoundMode  round    = RoundMode::None;
    bool       lo       = false;
    bool       hi       = false;
    bool       wide     = false;
    bool       ftz      = false;
    bool       sat      = false;
    bool       toSpace  = false; // cvta.to
    // atom modifier (stored as string: "add","or","xor","min","max","cas","exch")
    std::string atomOp;
};

// ============================================================
// Operand (in AST, before virtual-register assignment)
// ============================================================
struct RegOperand {
    std::string name;     // "%r5" or "%r<10>" placeholder
    int         index;    // -1 if named, else range index
};

struct ImmOperand {
    std::string raw; // as lexed
};

struct LabelOperand {
    std::string name;
};

// address expression: [base + offset]
struct AddrOperand {
    std::string base;
    std::optional<int64_t> offset;
};

using OperandVariant = std::variant<RegOperand, ImmOperand, LabelOperand, AddrOperand>;

struct Operand {
    OperandVariant value;
    SourceLocation loc;

    bool isReg()   const { return std::holds_alternative<RegOperand>(value); }
    bool isImm()   const { return std::holds_alternative<ImmOperand>(value); }
    bool isLabel() const { return std::holds_alternative<LabelOperand>(value); }
    bool isAddr()  const { return std::holds_alternative<AddrOperand>(value); }

    const RegOperand&   asReg()   const { return std::get<RegOperand>(value); }
    const ImmOperand&   asImm()   const { return std::get<ImmOperand>(value); }
    const LabelOperand& asLabel() const { return std::get<LabelOperand>(value); }
    const AddrOperand&  asAddr()  const { return std::get<AddrOperand>(value); }
};

// ============================================================
// Predicate guard: @%p or @!%p
// ============================================================
struct Predicate {
    std::string reg;    // register name (e.g., "%p0")
    bool        negate; // true if @!%p
};

// ============================================================
// AST node: instruction
// ============================================================
struct Instruction {
    Opcode               opcode  = Opcode::Unknown;
    InsnMods             mods;
    std::optional<Predicate> pred;          // predicate guard, if any
    std::vector<Operand> dests;             // destination operands
    std::vector<Operand> srcs;              // source operands
    SourceLocation       loc;
};

// ============================================================
// AST node: label definition
// ============================================================
struct LabelDef {
    std::string    name;
    SourceLocation loc;
};

// ============================================================
// AST statement (a labeled instruction or standalone label)
// ============================================================
struct Stmt {
    std::optional<LabelDef> label;
    std::optional<Instruction> insn;
};

// ============================================================
// Register declaration (.reg .type %name<count>; or .reg .type %name;)
// ============================================================
struct RegDecl {
    PtxType     type  = PtxType::Unknown;
    std::string name;           // "%r" in "%r<10>", or full name "%myReg"
    int         count = 1;      // 1 for scalar, N for array
    SourceLocation loc;
};

// ============================================================
// Kernel parameter (.param .type %name)
// ============================================================
struct Param {
    PtxType     type  = PtxType::Unknown;
    AddrSpace   space = AddrSpace::Param;
    std::string name;
    int         align = 0;
    SourceLocation loc;
};

// ============================================================
// Global variable declaration
// ============================================================
struct GlobalVar {
    AddrSpace   space = AddrSpace::Global;
    PtxType     type  = PtxType::Unknown;
    std::string name;
    int         count = 1;
    int         align = 0;
    SourceLocation loc;
};

// ============================================================
// A kernel (.entry or .func) body
// ============================================================
struct Kernel {
    bool        isEntry = true;   // .entry vs .func
    bool        isVisible = false;
    std::string name;
    std::vector<Param>   params;
    std::vector<RegDecl> regDecls;
    std::vector<Stmt>    body;
    SourceLocation loc;
};

// ============================================================
// Top-level module
// ============================================================
struct Module {
    std::string version;     // e.g., "7.5"
    std::string target;      // e.g., "sm_80"
    int         addressSize  = 64;
    std::vector<GlobalVar>  globals;
    std::vector<Kernel>     kernels;
    std::string             filename;
};

} // namespace ast
} // namespace ptxra
