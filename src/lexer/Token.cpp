#include "Token.h"
#include <unordered_map>

namespace ptxra {

std::string_view tokenKindName(TokenKind k) {
    switch (k) {
    case TokenKind::IntLiteral:      return "IntLiteral";
    case TokenKind::FloatLiteral:    return "FloatLiteral";
    case TokenKind::StringLiteral:   return "StringLiteral";
    case TokenKind::Identifier:      return "Identifier";
    case TokenKind::Register:        return "Register";
    case TokenKind::Label:           return "Label";
    case TokenKind::LabelRef:        return "LabelRef";
    case TokenKind::DotVersion:      return ".version";
    case TokenKind::DotTarget:       return ".target";
    case TokenKind::DotAddressSize:  return ".address_size";
    case TokenKind::DotVisible:      return ".visible";
    case TokenKind::DotEntry:        return ".entry";
    case TokenKind::DotFunc:         return ".func";
    case TokenKind::DotParam:        return ".param";
    case TokenKind::DotReg:          return ".reg";
    case TokenKind::DotShared:       return ".shared";
    case TokenKind::DotGlobal:       return ".global";
    case TokenKind::DotLocal:        return ".local";
    case TokenKind::DotAlign:        return ".align";
    case TokenKind::DotByte:         return ".byte";
    case TokenKind::DotWord:         return ".word";
    case TokenKind::DotU32:          return ".u32";
    case TokenKind::DotU64:          return ".u64";
    case TokenKind::DotS32:          return ".s32";
    case TokenKind::DotS64:          return ".s64";
    case TokenKind::DotB8:           return ".b8";
    case TokenKind::DotB16:          return ".b16";
    case TokenKind::DotB32:          return ".b32";
    case TokenKind::DotB64:          return ".b64";
    case TokenKind::DotU8:           return ".u8";
    case TokenKind::DotU16:          return ".u16";
    case TokenKind::DotS8:           return ".s8";
    case TokenKind::DotS16:          return ".s16";
    case TokenKind::DotF16:          return ".f16";
    case TokenKind::DotF32:          return ".f32";
    case TokenKind::DotF64:          return ".f64";
    case TokenKind::DotPred:         return ".pred";
    case TokenKind::DotV2:           return ".v2";
    case TokenKind::DotV4:           return ".v4";
    case TokenKind::DotLo:           return ".lo";
    case TokenKind::DotHi:           return ".hi";
    case TokenKind::DotWide:         return ".wide";
    case TokenKind::DotRn:           return ".rn";
    case TokenKind::DotRz:           return ".rz";
    case TokenKind::DotRm:           return ".rm";
    case TokenKind::DotRp:           return ".rp";
    case TokenKind::DotFtz:          return ".ftz";
    case TokenKind::DotSat:          return ".sat";
    case TokenKind::DotEq:           return ".eq";
    case TokenKind::DotNe:           return ".ne";
    case TokenKind::DotLt:           return ".lt";
    case TokenKind::DotLe:           return ".le";
    case TokenKind::DotGt:           return ".gt";
    case TokenKind::DotGe:           return ".ge";
    case TokenKind::DotEqu:          return ".equ";
    case TokenKind::DotNeu:          return ".neu";
    case TokenKind::DotLtu:          return ".ltu";
    case TokenKind::DotLeu:          return ".leu";
    case TokenKind::DotGtu:          return ".gtu";
    case TokenKind::DotGeu:          return ".geu";
    case TokenKind::DotNum:          return ".num";
    case TokenKind::DotNan:          return ".nan";
    case TokenKind::DotLs:           return ".ls";
    case TokenKind::DotHs:           return ".hs";
    case TokenKind::DotGlobal_space: return ".global";
    case TokenKind::DotShared_space: return ".shared";
    case TokenKind::DotParam_space:  return ".param";
    case TokenKind::DotLocal_space:  return ".local";
    case TokenKind::DotGeneric:      return ".generic";
    case TokenKind::DotAnd:          return ".and";
    case TokenKind::DotOr:           return ".or";
    case TokenKind::DotAdd:          return ".add";
    case TokenKind::DotXor:          return ".xor";
    case TokenKind::DotMin:          return ".min";
    case TokenKind::DotMax:          return ".max";
    case TokenKind::DotCas:          return ".cas";
    case TokenKind::DotExch:         return ".exch";
    case TokenKind::DotTo:           return ".to";
    case TokenKind::DotSync:         return ".sync";
    case TokenKind::DotCta:          return ".cta";
    case TokenKind::DotGpu:          return ".gpu";
    case TokenKind::DotSys:          return ".sys";
    case TokenKind::LParen:          return "(";
    case TokenKind::RParen:          return ")";
    case TokenKind::LBrace:          return "{";
    case TokenKind::RBrace:          return "}";
    case TokenKind::LBracket:        return "[";
    case TokenKind::RBracket:        return "]";
    case TokenKind::Comma:           return ",";
    case TokenKind::Semicolon:       return ";";
    case TokenKind::Colon:           return ":";
    case TokenKind::At:              return "@";
    case TokenKind::Bang:            return "!";
    case TokenKind::Plus:            return "+";
    case TokenKind::Minus:           return "-";
    case TokenKind::Star:            return "*";
    case TokenKind::Pipe:            return "|";
    case TokenKind::Ampersand:       return "&";
    case TokenKind::Caret:           return "^";
    case TokenKind::Tilde:           return "~";
    case TokenKind::Equals:          return "=";
    case TokenKind::Lt:              return "<";
    case TokenKind::Gt:              return ">";
    case TokenKind::AngleLt:         return "<";
    case TokenKind::AngleGt:         return ">";
    case TokenKind::Eof:             return "<EOF>";
    case TokenKind::Unknown:         return "<unknown>";
    }
    return "<invalid>";
}

// ---- Token member implementations ----

bool Token::isTypeQualifier() const {
    switch (kind) {
    case TokenKind::DotPred:
    case TokenKind::DotB8:  case TokenKind::DotB16:
    case TokenKind::DotB32: case TokenKind::DotB64:
    case TokenKind::DotU8:  case TokenKind::DotU16:
    case TokenKind::DotU32: case TokenKind::DotU64:
    case TokenKind::DotS8:  case TokenKind::DotS16:
    case TokenKind::DotS32: case TokenKind::DotS64:
    case TokenKind::DotF16: case TokenKind::DotF32:
    case TokenKind::DotF64:
        return true;
    default:
        return false;
    }
}

bool Token::isAddressSpace() const {
    switch (kind) {
    case TokenKind::DotGlobal:
    case TokenKind::DotGlobal_space:
    case TokenKind::DotShared:
    case TokenKind::DotShared_space:
    case TokenKind::DotParam:
    case TokenKind::DotParam_space:
    case TokenKind::DotLocal:
    case TokenKind::DotLocal_space:
    case TokenKind::DotGeneric:
        return true;
    default:
        return false;
    }
}

bool Token::isCmpOp() const {
    switch (kind) {
    case TokenKind::DotEq:  case TokenKind::DotNe:
    case TokenKind::DotLt:  case TokenKind::DotLe:
    case TokenKind::DotGt:  case TokenKind::DotGe:
    case TokenKind::DotEqu: case TokenKind::DotNeu:
    case TokenKind::DotLtu: case TokenKind::DotLeu:
    case TokenKind::DotGtu: case TokenKind::DotGeu:
    case TokenKind::DotNum: case TokenKind::DotNan:
    case TokenKind::DotLs:  case TokenKind::DotHs:
        return true;
    default:
        return false;
    }
}

} // namespace ptxra
