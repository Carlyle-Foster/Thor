#ifndef KIND
#define KIND(...)
#endif
#ifndef ASSIGN
#define ASSIGN(...)
#endif
#ifndef LITERAL
#define LITERAL(...)
#endif
#ifndef OPERATOR
#define OPERATOR(...)
#endif
#ifndef KEYWORD
#define KEYWORD(...)
#endif
#ifndef DIRECTIVE
#define DIRECTIVE(...)
#endif
#ifndef CCONV
#define CCONV(...)
#endif

// Token kinds
//   ENUM        NAME           ASI
KIND(ENDOF,      "eof",         false)
KIND(INVALID,    "invalid",     false)
KIND(COMMENT,    "comment",     false)
KIND(IDENTIFIER, "identifier",  true)
KIND(LITERAL,    "literal",     true)
KIND(OPERATOR,   "operator",    false) // ASI handled separate
KIND(KEYWORD,    "keyword",     false) // ASI handled separate
KIND(ASSIGNMENT, "assignment",  false)
KIND(DIRECTIVE,  "directive",   false) // '#'
KIND(ATTRIBUTE,  "attribute",   false) // '@'
KIND(CONST,      "const",       false) // '$'
KIND(SEMICOLON,  "semicolon",   false) // ';'
KIND(COMMA,      "comma",       false) // ','
KIND(LBRACE,     "left brace",  false) // '{'
KIND(RBRACE,     "right brace", true)  // '}'
KIND(UNDEFINED,  "undefined",   true)  // '---'

// Assignment tokens
//
// Different from operators because assignments are strictly statements.
//
//     ENUM    NAME           MATCH
ASSIGN(EQ,     "eq",          "=")
ASSIGN(ADD,    "add",         "+=")
ASSIGN(SUB,    "sub",         "-=")
ASSIGN(MUL,    "mul",         "*=")
ASSIGN(QUO,    "quo",         "/=")
ASSIGN(MOD,    "mod",         "%=")
ASSIGN(REM,    "rem",         "%%=")
ASSIGN(BAND,   "bit and",     "&=")
ASSIGN(BOR,    "bit or",      "|=")
ASSIGN(XOR,    "xor",         "~=")
ASSIGN(ANDNOT, "and not",     "&~=")
ASSIGN(SHL,    "shift left",  "<<=")
ASSIGN(SHR,    "shift right", ">>=")
ASSIGN(LAND,   "logical and", "&&=")
ASSIGN(LOR,    "logical or",  "||=")

// Literal tokens
//
//      ENUM       NAME
LITERAL(INTEGER,   "integer")
LITERAL(FLOAT,     "float")
LITERAL(IMAGINARY, "imaginary")
LITERAL(RUNE,      "rune")
LITERAL(STRING,    "string")
LITERAL(BOOLEAN,   "boolean")

// Operators
//       ENUM       NAME                      MATCH        PREC NAMED  ASI
OPERATOR(LNOT,      "logical not",            "!",         0,   false, false)
OPERATOR(POINTER,   "pointer",                "^",         0,   false, true)
OPERATOR(ARROW,     "arrow",                  "->",        0,   false, false)
OPERATOR(LPAREN,    "left paren",             "(",         0,   false, false)
OPERATOR(RPAREN,    "right paren",            ")",         0,   false, true)
OPERATOR(LBRACKET,  "left bracket",           "[",         0,   false, false)
OPERATOR(RBRACKET,  "right bracket",          "]",         0,   false, true)
OPERATOR(COLON,     "colon",                  ":",         0,   false, false)
OPERATOR(PERIOD,    "period",                 ".",         0,   false, false)
OPERATOR(IN,        "in",                     "in",        6,   true,  false)
OPERATOR(NOT_IN,    "not_in",                 "not_in",    6,   true,  false)
OPERATOR(AUTO_CAST, "auto_cast",              "auto_cast", 0,   true,  false)
OPERATOR(CAST,      "cast",                   "cast",      0,   true,  false)
OPERATOR(TRANSMUTE, "transmute",              "transmute", 0,   true,  false)
OPERATOR(OR_ELSE,   "or_else",                "or_else",   1,   true,  false)
OPERATOR(OR_RETURN, "or_return",              "or_return", 1,   true,  true)
OPERATOR(QUESTION,  "question",               "?",         1,   false, true)
OPERATOR(ELLIPSIS,  "ellipsis",               "..",        2,   false, false)
OPERATOR(RANGEFULL, "full range",             "..=",       2,   false, false)
OPERATOR(RANGEHALF, "half range",             "..<",       2,   false, false)
OPERATOR(LOR,       "logical or",             "||",        3,   false, false)
OPERATOR(LAND,      "logical and",            "&&",        4,   false, false)
OPERATOR(EQ,        "equal",                  "==",        5,   false, false)
OPERATOR(NEQ,       "not equal",              "!=",        5,   false, false)
OPERATOR(LT,        "less-than",              "<",         5,   false, false)
OPERATOR(GT,        "greater-than",           ">",         5,   false, false)
OPERATOR(LTEQ,      "less-than or equal",     "<=",        5,   false, false)
OPERATOR(GTEQ,      "greater-than or equal",  ">=",        5,   false, false)
OPERATOR(ADD,       "addition",               "+",         6,   false, false)
OPERATOR(SUB,       "subtraction",            "-",         6,   false, false)
OPERATOR(BOR,       "bit or",                 "|",         6,   false, false)
OPERATOR(XOR,       "xor",                    "~",         6,   false, false)
OPERATOR(QUO,       "quo",                    "/",         7,   false, false)
OPERATOR(MUL,       "mul",                    "*",         7,   false, false)
OPERATOR(MOD,       "mod",                    "%",         7,   false, false)
OPERATOR(REM,       "rem",                    "%%",        7,   false, false)
OPERATOR(BAND,      "bit and",                "&",         7,   false, false)
OPERATOR(ANDNOT,    "and not",                "&~",        7,   false, false)
OPERATOR(SHL,       "shift left",             "<<",        7,   false, false)
OPERATOR(SHR,       "shift right",            ">>",        7,   false, false)

// Keyword tokens
//
//      ENUM         MATCH          ASI
KEYWORD(IMPORT,      "import",      false)
KEYWORD(FOREIGN,     "foreign",     false)
KEYWORD(PACKAGE,     "package",     false)
KEYWORD(WHERE,       "where",       false)
KEYWORD(WHEN,        "when",        false) // Can also be an operator (x when y else z)
KEYWORD(IF,          "if",          false) // Can also be an operator (x if y else z)
KEYWORD(FOR,         "for",         false)
KEYWORD(SWITCH,      "switch",      false)
KEYWORD(DO,          "do",          false)
KEYWORD(CASE,        "case",        false)
KEYWORD(BREAK,       "break",       true)
KEYWORD(CONTINUE,    "continue",    true)
KEYWORD(OR_BREAK,    "or_break",    true)
KEYWORD(OR_CONTINUE, "or_continue", true)
KEYWORD(FALLTHROUGH, "fallthrough", true)
KEYWORD(DEFER,       "defer",       false)
KEYWORD(RETURN,      "return",      true)
KEYWORD(PROC,        "proc",        false)
KEYWORD(STRUCT,      "struct",      false)
KEYWORD(UNION,       "union",       false)
KEYWORD(ENUM,        "enum",        false)
KEYWORD(BITSET,      "bit_set",     false)
KEYWORD(MAP,         "map",         false)
KEYWORD(DYNAMIC,     "dynamic",     false)
KEYWORD(DISTINCT,    "distinct",    false)
KEYWORD(USING,       "using",       false)
KEYWORD(CONTEXT,     "context",     false)
KEYWORD(MATRIX,      "matrix",      false)

// Directive tokens
//
//        ENUM                      MATCH
DIRECTIVE(OPTIONAL_OK,              "optional_ok")
DIRECTIVE(OPTIONAL_ALLOCATOR_ERROR, "optional_allocator_error")
DIRECTIVE(BOUNDS_CHECK,             "bounds_check")
DIRECTIVE(NO_BOUNDS_CHECK,          "no_bounds_check")
DIRECTIVE(TYPE_ASSERT,              "type_assert")
DIRECTIVE(NO_TYPE_ASSERT,           "no_type_assert")
DIRECTIVE(ALIGN,                    "align")
DIRECTIVE(RAW_UNION,                "raw_union")
DIRECTIVE(PACKED,                   "packed")
DIRECTIVE(TYPE,                     "type")
DIRECTIVE(SIMD,                     "simd")
DIRECTIVE(SOA,                      "soa")
DIRECTIVE(PARTIAL,                  "partial")
DIRECTIVE(SPARSE,                   "sparse")
DIRECTIVE(FORCE_INLINE,             "force_inline")
DIRECTIVE(FORCE_NO_INLINE,          "force_no_inline")
DIRECTIVE(NO_NIL,                   "no_nil")
DIRECTIVE(SHARED_NIL,               "shared_nil")
DIRECTIVE(NO_ALIAS,                 "no_alias")
DIRECTIVE(C_VARARG,                 "c_vararg")
DIRECTIVE(ANY_INT,                  "any_int")
DIRECTIVE(SUBTYPE,                  "subtype")
DIRECTIVE(BY_PTR,                   "by_ptr")
DIRECTIVE(ASSERT,                   "assert")
DIRECTIVE(PANIC,                    "panic")
DIRECTIVE(UNROLL,                   "unroll")
DIRECTIVE(LOCATION,                 "location")
DIRECTIVE(PROCEDURE,                "procedure")
DIRECTIVE(FILE,                     "file")
DIRECTIVE(LOAD,                     "load")
DIRECTIVE(LOAD_HASH,                "load_hash")
DIRECTIVE(LOAD_DIRECTORY,           "load_directory")
DIRECTIVE(DEFINED,                  "defined")
DIRECTIVE(CONFIG,                   "config")
DIRECTIVE(MAYBE,                    "maybe")
DIRECTIVE(CALLER_LOCATION,          "caller_location")
DIRECTIVE(CALLER_EXPRESSION,        "caller_expression")
DIRECTIVE(NO_COPY,                  "no_copy")
DIRECTIVE(CONST,                    "const")

// Calling conventions
//
//    ENUM         MATCH
CCONV(ODIN,        "odin")
CCONV(CONTEXTLESS, "contextless")
CCONV(CDECL,       "cdecl")
CCONV(STDCALL,     "stdcall")
CCONV(FASTCALL,    "fastcall")
CCONV(NONE,        "none")
CCONV(NAKED,       "naked")
CCONV(WIN64,       "win64")
CCONV(SYSV,        "sysv")
CCONV(SYSTEM,      "system")

#undef KIND
#undef ASSIGN
#undef LITERAL
#undef OPERATOR
#undef KEYWORD
#undef DIRECTIVE
#undef CCONV
