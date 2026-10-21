#include "AST.h"
#include <stdexcept>
#include <unordered_map>

namespace ptxra {
namespace ast {

// ---- ptxTypeName ----
std::string ptxTypeName(PtxType t) {
    switch (t) {
    case PtxType::Pred: return ".pred";
    case PtxType::B8:   return ".b8";
    case PtxType::B16:  return ".b16";
    case PtxType::B32:  return ".b32";
    case PtxType::B64:  return ".b64";
    case PtxType::U8:   return ".u8";
    case PtxType::U16:  return ".u16";
    case PtxType::U32:  return ".u32";
    case PtxType::U64:  return ".u64";
    case PtxType::S8:   return ".s8";
    case PtxType::S16:  return ".s16";
    case PtxType::S32:  return ".s32";
    case PtxType::S64:  return ".s64";
    case PtxType::F16:  return ".f16";
    case PtxType::F32:  return ".f32";
    case PtxType::F64:  return ".f64";
    case PtxType::Unknown: return ".??";
    }
    return ".??";
}

PtxType ptxTypeFromDotToken(const std::string& text) {
    static const std::unordered_map<std::string, PtxType> table = {
        {".pred", PtxType::Pred},
        {".b8",   PtxType::B8},  {".b16", PtxType::B16},
        {".b32",  PtxType::B32}, {".b64", PtxType::B64},
        {".u8",   PtxType::U8},  {".u16", PtxType::U16},
        {".u32",  PtxType::U32}, {".u64", PtxType::U64},
        {".s8",   PtxType::S8},  {".s16", PtxType::S16},
        {".s32",  PtxType::S32}, {".s64", PtxType::S64},
        {".f16",  PtxType::F16}, {".f32", PtxType::F32},
        {".f64",  PtxType::F64},
    };
    auto it = table.find(text);
    return (it != table.end()) ? it->second : PtxType::Unknown;
}

int ptxTypeBits(PtxType t) {
    switch (t) {
    case PtxType::Pred: return 1;
    case PtxType::B8:  case PtxType::U8:  case PtxType::S8:  return 8;
    case PtxType::B16: case PtxType::U16: case PtxType::S16:
    case PtxType::F16:                                         return 16;
    case PtxType::B32: case PtxType::U32: case PtxType::S32:
    case PtxType::F32:                                         return 32;
    case PtxType::B64: case PtxType::U64: case PtxType::S64:
    case PtxType::F64:                                         return 64;
    case PtxType::Unknown: return 0;
    }
    return 0;
}

// ---- addrSpaceName ----
std::string addrSpaceName(AddrSpace s) {
    switch (s) {
    case AddrSpace::None:    return "";
    case AddrSpace::Global:  return ".global";
    case AddrSpace::Shared:  return ".shared";
    case AddrSpace::Param:   return ".param";
    case AddrSpace::Local:   return ".local";
    case AddrSpace::Generic: return ".generic";
    }
    return "";
}

// ---- cmpOpName ----
std::string cmpOpName(CmpOp op) {
    switch (op) {
    case CmpOp::Eq:  return ".eq";
    case CmpOp::Ne:  return ".ne";
    case CmpOp::Lt:  return ".lt";
    case CmpOp::Le:  return ".le";
    case CmpOp::Gt:  return ".gt";
    case CmpOp::Ge:  return ".ge";
    case CmpOp::Equ: return ".equ";
    case CmpOp::Neu: return ".neu";
    case CmpOp::Ltu: return ".ltu";
    case CmpOp::Leu: return ".leu";
    case CmpOp::Gtu: return ".gtu";
    case CmpOp::Geu: return ".geu";
    case CmpOp::Num: return ".num";
    case CmpOp::Nan: return ".nan";
    case CmpOp::Ls:  return ".ls";
    case CmpOp::Hs:  return ".hs";
    case CmpOp::None: return "";
    }
    return "";
}

// ---- roundModeName ----
std::string roundModeName(RoundMode r) {
    switch (r) {
    case RoundMode::Rn: return ".rn";
    case RoundMode::Rz: return ".rz";
    case RoundMode::Rm: return ".rm";
    case RoundMode::Rp: return ".rp";
    case RoundMode::None: return "";
    }
    return "";
}

// ---- opcodeName ----
std::string opcodeName(Opcode op) {
    switch (op) {
    case Opcode::Mov:     return "mov";
    case Opcode::Add:     return "add";
    case Opcode::Sub:     return "sub";
    case Opcode::Mul:     return "mul";
    case Opcode::Mad:     return "mad";
    case Opcode::Fma:     return "fma";
    case Opcode::Div:     return "div";
    case Opcode::Rem:     return "rem";
    case Opcode::Neg:     return "neg";
    case Opcode::Abs:     return "abs";
    case Opcode::Min:     return "min";
    case Opcode::Max:     return "max";
    case Opcode::And:     return "and";
    case Opcode::Or:      return "or";
    case Opcode::Xor:     return "xor";
    case Opcode::Not:     return "not";
    case Opcode::Shl:     return "shl";
    case Opcode::Shr:     return "shr";
    case Opcode::Cvt:     return "cvt";
    case Opcode::Cvta:    return "cvta";
    case Opcode::Setp:    return "setp";
    case Opcode::Selp:    return "selp";
    case Opcode::Ld:      return "ld";
    case Opcode::St:      return "st";
    case Opcode::Bra:     return "bra";
    case Opcode::Ret:     return "ret";
    case Opcode::Exit:    return "exit";
    case Opcode::BarSync: return "bar.sync";
    case Opcode::Atom:    return "atom";
    case Opcode::Unknown: return "???";
    }
    return "???";
}

Opcode opcodeFromString(const std::string& s) {
    static const std::unordered_map<std::string, Opcode> table = {
        {"mov",      Opcode::Mov},
        {"add",      Opcode::Add},
        {"sub",      Opcode::Sub},
        {"mul",      Opcode::Mul},
        {"mad",      Opcode::Mad},
        {"fma",      Opcode::Fma},
        {"div",      Opcode::Div},
        {"rem",      Opcode::Rem},
        {"neg",      Opcode::Neg},
        {"abs",      Opcode::Abs},
        {"min",      Opcode::Min},
        {"max",      Opcode::Max},
        {"and",      Opcode::And},
        {"or",       Opcode::Or},
        {"xor",      Opcode::Xor},
        {"not",      Opcode::Not},
        {"shl",      Opcode::Shl},
        {"shr",      Opcode::Shr},
        {"cvt",      Opcode::Cvt},
        {"cvta",     Opcode::Cvta},
        {"setp",     Opcode::Setp},
        {"selp",     Opcode::Selp},
        {"ld",       Opcode::Ld},
        {"st",       Opcode::St},
        {"bra",      Opcode::Bra},
        {"ret",      Opcode::Ret},
        {"exit",     Opcode::Exit},
        {"bar",      Opcode::BarSync},  // bar.sync parsed as one
        {"atom",     Opcode::Atom},
    };
    auto it = table.find(s);
    return (it != table.end()) ? it->second : Opcode::Unknown;
}

} // namespace ast
} // namespace ptxra
