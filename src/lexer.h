#ifndef THOR_LEXER_H
#define THOR_LEXER_H
#include "util/array.h"
#include "util/maybe.h"
#include "util/string.h"
#include "util/unicode.h"

namespace Thor {

struct System;

enum class TokenKind : Uint8 {
	#define KIND(ENUM, NAME, ASI) ENUM,
	#include "lexer.inl"
};

enum class AssignKind : Uint8 {
	#define ASSIGN(ENUM, NAME, MATCH) ENUM,
	#include "lexer.inl"
};

enum class LiteralKind : Uint8 {
	#define LITERAL(ENUM, NAME) ENUM,
	#include "lexer.inl"
};

enum class OperatorKind : Uint8 {
	#define OPERATOR(ENUM, NAME, MATCH, PREC, NAMED, ASI) ENUM,
	#include "lexer.inl"
};

enum class KeywordKind : Uint8 {
	#define KEYWORD(ENUM, MATCH, ASI) ENUM,
	#include "lexer.inl"
};

enum class DirectiveKind : Uint8 {
	#define DIRECTIVE(ENUM, MATCH) ENUM,
	#include "lexer.inl"
};

enum class CConvKind : Uint8 {
	#define CCONV(ENUM, MATCH) ENUM,
	#include "lexer.inl"
};

template<typename T, Ulen E>
constexpr Ulen countof(const T(&)[E]) {
	return E;
}

struct Token {
	THOR_FORCEINLINE constexpr Token(TokenKind kind, Uint32 offset, Uint16 length)
		: kind{kind}
		, length{length}
		, offset{offset}
	{
	}
	THOR_FORCEINLINE constexpr Token(AssignKind kind, Uint32 offset, Uint16 length)
		: Token{TokenKind::ASSIGNMENT, offset, length}
	{
		as_assign = kind;
	}
	THOR_FORCEINLINE constexpr Token(LiteralKind kind, Uint32 offset, Uint16 length)
		: Token{TokenKind::LITERAL, offset, length}
	{
		as_literal = kind;
	}
	THOR_FORCEINLINE constexpr Token(OperatorKind kind, Uint32 offset, Uint16 length)
		: Token{TokenKind::OPERATOR, offset, length}
	{
		as_operator = kind;
	}
	THOR_FORCEINLINE constexpr Token(KeywordKind kind, Uint32 offset, Uint16 length)
		: Token{TokenKind::KEYWORD, offset, length}
	{
		as_keyword = kind;
	}
	THOR_FORCEINLINE constexpr Token(DirectiveKind kind, Uint32 offset, Uint16 length)
		: Token{TokenKind::DIRECTIVE, offset, length}
	{
		as_directive = kind;
	}
	void dump(System& sys, StringView input);
	TokenKind kind; // 1b
	union {
		Nat           as_nat{};
		AssignKind    as_assign;
		LiteralKind   as_literal;
		OperatorKind  as_operator;
		KeywordKind   as_keyword;
		DirectiveKind as_directive;
	};              // 1b
	Uint16 length;  // 2b (if 0, length is too long and must be re-lexed at the given offset)
	Uint32 offset;  // 4b
};
static_assert(sizeof(Token) == 8, "Token cannot be larger than 64-bits");

struct Allocator;

struct Position {
	Uint32 line        = 0;
	Uint32 column      = 0;
	Uint32 next_offset = 0;
	Uint32 this_offset = 0;
	void advance_column() {
		this_offset = next_offset;
		next_offset++;
		column++;
	}
	void advance_line() {
		line++;
		column = 0;
	}
	Uint16 delta(Uint32 beg) const {
		const auto diff = this_offset - beg;
		if (diff >= 0xffff_u16) {
			// Too large to fit in 16-bits.
			return 0;
		}
		return static_cast<Uint16>(diff);
	}
};

struct Lexer {
	static Maybe<Lexer> open(System& sys, StringView file);
	Lexer(Lexer&& other)
		: map_{move(other.map_)}
		, input_{other.input_}
		, position_{other.position_}
		, rune_{exchange(other.rune_, 0)}
		, asi_{exchange(other.asi_, false)}
	{
	}
	Token next();
	void eat();
	THOR_FORCEINLINE constexpr StringView input() const {
		return input_;
	}
	THOR_FORCEINLINE constexpr StringView string(Token token) const {
		return input_.slice(token.offset).truncate(token.length);
	}

	// Calculate the source position for a given token.
	struct SourcePosition {
		Uint32 line   = 0;
		Uint32 column = 0;
	};
	SourcePosition position(Uint32 offset) const {
		Uint32 line   = 1;
		Uint32 column = 1;
		for (Uint32 i = 0; i < offset; i++) {
			if (input_[i] == '\n') {
				column = 1;
				line++;
			} else {
				column++;
			}
		}
		return SourcePosition { line, column };
	}

private:
	Token advance();
	Token scan_string();
	void scan_escape();
	Token scan_number(Bool leading_period);
	Lexer(Array<Uint8>&& map)
		: map_{move(map)}
		, input_{map_.slice().cast<const char>()}
	{
		eat();
	}
	Array<Uint8> map_;
	StringView   input_;
	Position     position_;
	Rune         rune_ = 0;
	Bool         asi_  = false;
};

} // namespace Thor

#endif // THOR_LEXER_H
