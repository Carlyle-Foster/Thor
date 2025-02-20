#include "lexer.h"
#include "string.h"

#include "util/file.h"

namespace Thor {

Maybe<Lexer> Lexer::open(System& sys, StringView filename) {
	if (filename.is_empty()) {
		return {};
	}
	auto file = File::open(sys, filename, File::Access::RD);
	if (!file) {
		return {};
	}
	const auto length = file->tell();
	if (length == 0 || length >= 0xff'ff'ff'ff_ulen) {
		return {};
	}
	auto map = file->map(sys.allocator);
	if (map.is_empty()) {
		return {};
	}
	return Lexer{move(map)};
}

void Lexer::eat() {
	if (rune_ == '\n') {
		position_.advance_line();
	}
	if (position_.next_offset >= input_.length()) {
		rune_ = 0; // EOF
		return;
	}
	auto rune = input_[position_.next_offset];
	if (rune == 0) {
		// Error: Unexpected NUL
	} else if (rune & 0x80) {
		// TODO(dweiler): UTF-8
	}
	position_.advance_column();
	rune_ = rune;
}

void Lexer::scan_escape() {
	Uint32 l = 0;
	Uint32 b = 0;
	switch (rune_) {
	case 'a': case 'b':
	case 'e': case 'f':
	case 'n': case 'r':
	case 't': case 'v':
	case '\\':
	case '\'':
	case '\"':
		eat();
		return;
	case '0': case '1': case '2': case '3':
	case '4': case '5': case '6': case '7':
		l = 3;
		b = 8;
		eat();
		break;
	case 'x':
		eat(); // Eat 'x'
		l = 2;
		b = 16;
		break;
	case 'u':
		[[fallthrough]];
	case 'U':
		eat(); // Eat 'u'
		l = 4;
		b = 16;
		break;
	default:
		// ERROR: Unknown escape sequence in string
		return;
	}
	for (Ulen i = 0; i < l; i++) {
		Uint32 d = rune_ - '0';
		if (d >= b) {
			if (rune_ == 0) {
				// ERROR: Unterminated escape sequence
			} else {
				// ERROR: Unknown character in escape sequence
			}
			return;
		}
		eat(); // Eat character in escape sequence
	}
}

Token Lexer::scan_string() {
	const auto beg = position_.this_offset;
	const auto quote = rune_;
	const auto raw = quote == '`';
	for (;;) {
		if (!raw) {
			if (rune_ == '\n' || rune_ == 0) {
				// ERROR: String literal is not terminated.
				break;
			}
		}
		eat();
		if (rune_ == quote) {
			eat(); // Consume quote
			break;
		}
		if (!raw && rune_ == '\\') {
			scan_escape();
		}
	}
	return { LiteralKind::STRING, beg, position_.delta(beg) };
}

Token Lexer::scan_number(Bool leading_period) {

	const auto beg = position_.this_offset;
	Token token = { TokenKind::LITERAL, beg, 1_u16 };

	if (leading_period) {
		token.as_literal = LiteralKind::FLOAT;
	}

	if (rune_ == '0') {
		eat(); // Eat '0'
		switch (rune_) {
		case 'b':
			eat();
			while(rune_.is_digit(2) || rune_ == '_') eat();
			break;
		case 'o':
			eat();
			while(rune_.is_digit(8) || rune_ == '_') eat();
			break;
		case 'd':
			eat();
			while(rune_.is_digit(10) || rune_ == '_') eat();
			break;
		case 'z':
			eat();
			while(rune_.is_digit(12) || rune_ == '_') eat();
			break;
		case 'x':
			eat();
			while(rune_.is_digit(16) || rune_ == '_') eat();
			break;
		case 'h':
			eat();
			while(rune_.is_digit(16) || rune_ == '_') eat();
			break;
			// TODO(Oliver): hexadecimal floats
		default:
			while(rune_.is_digit(16) || rune_ == '_') eat();
			break;
		}
	}

	if (rune_ == '.') {
		eat(); // Eat '.'
		while (rune_.is_digit(10) || rune_ == '_') eat();
	}

	if(rune_ == 'e' || rune_ == 'E') {
		token.as_literal = LiteralKind::FLOAT;
		eat(); // Eat 'e' or 'E'
		if (rune_ == '-' || rune_ == '+') {
			eat(); // Eat '-' or '+'
		}
		while (rune_.is_digit(10) || rune_ == '_') eat();
	}

	switch (rune_) {
	case 'i':
	case 'j':
	case 'k':
		eat();
		token.as_literal = LiteralKind::IMAGINARY;
	default:
		break;
	}

	token.length = position_.delta(beg);
	return token;
}

Token Lexer::advance() {
	// Skip whitespace
	for (;;) {
		switch (rune_) {
		case ' ': case '\t': case '\r':
			eat(); // Consume
			continue;
		case '\n':
			if (!asi_) {
				eat();
				continue;
			}
		}
		break;
	}
	const auto beg = position_.this_offset;
	if (rune_.is_char()) {
		while (rune_.is_alpha()) eat();
		const auto len = position_.delta(beg);
		const auto str = input_.slice(beg).truncate(len);
		static constexpr const struct {
			StringView   match;
			OperatorKind kind;
		} OPERATORS[] = {
			#define OPERATOR_true(ENUM, MATCH) \
				{ MATCH, OperatorKind::ENUM },
			#define OPERATOR_false(...)
			#define OPERATOR(ENUM, NAME, MATCH, PREC, NAMED, ASI) \
				OPERATOR_ ## NAMED (ENUM, MATCH)
			#include "lexer.inl"
		};
		for (Ulen i = 0; i < countof(OPERATORS); i++) {
			if (const auto& op = OPERATORS[i]; op.match == str) {
				return { op.kind, beg, len };
			}
		}
		static constexpr const struct {
			StringView  match;
			KeywordKind kind;
		} KEYWORDS[] = {
			#define KEYWORD(ENUM, MATCH, ASI) \
				{ MATCH, KeywordKind::ENUM },
			#include "lexer.inl"
		};
		for (Ulen i = 0; i < countof(KEYWORDS); i++) {
			if (const auto& kw = KEYWORDS[i]; kw.match == str) {
				return { kw.kind, beg, len };
			}
		}
		return { TokenKind::IDENTIFIER, beg, len };
	}
	switch (rune_) {
	case '0': case '1': case '2': case '3': case '4':
	case '5': case '6': case '7': case '8': case '9':
		return scan_number(false);
	case 0:
		// EOF
		eat(); // Eat EOF
		if (asi_) {
			asi_ = false;
			return { TokenKind::SEMICOLON, beg, 1_u16 };
		}
		return { TokenKind::ENDOF, beg, 1_u16 };
	case '\n':
		eat(); // Eat '\n'
		asi_ = false;
		return { TokenKind::SEMICOLON, beg, 1_u16 };
	case '\\':
		eat();
		// eat();
		asi_ = false;
		return next(); // Recurse
		// TODO
		// if next.line == position_.line we're missing a newline after '\'
	case '@': eat(); return { TokenKind::ATTRIBUTE,   beg, 1_u16 };
	case '$': eat(); return { TokenKind::CONST,       beg, 1_u16 };
	case ';': eat(); return { TokenKind::SEMICOLON,   beg, 1_u16 };
	case ',': eat(); return { TokenKind::COMMA,       beg, 1_u16 };
	case '{': eat(); return { TokenKind::LBRACE,      beg, 1_u16 };
	case '}': eat(); return { TokenKind::RBRACE,      beg, 1_u16 };
	case '(': eat(); return { OperatorKind::LPAREN,   beg, 1_u16 };
	case ')': eat(); return { OperatorKind::RPAREN,   beg, 1_u16 };
	case '[': eat(); return { OperatorKind::LBRACKET, beg, 1_u16 };
	case ']': eat(); return { OperatorKind::RBRACKET, beg, 1_u16 };
	case '?': eat(); return { OperatorKind::QUESTION, beg, 1_u16 };
	case ':': eat(); return { OperatorKind::COLON,    beg, 1_u16 };
	case '%':
		eat(); // Eat '%'
		switch (rune_) {
		case '=':
			eat(); // Eat '='
			return { AssignKind::MOD, beg, 2_u16 };
		case '%':
			eat(); // Eat '%'
			if (rune_ == '=') {
				eat(); // Eat '='
				return { AssignKind::MOD, beg, 3_u16 };
			}
			return { OperatorKind::MOD, beg, 2_u16 };
		}
		return { OperatorKind::MOD, beg, 1_u16 };
	case '*':
		eat(); // Eat '*'
		if (rune_ == '=') {
			return { AssignKind::MUL, beg, 2_u16 };
		}
		return { OperatorKind::MUL, beg, 1_u16 };
	case '/':
		eat(); // Eat '/'
		switch (rune_) {
		case '/':
			// Scan to EOL or EOF
			eat(); // Eat '/'
			while (rune_ != '\n' && rune_ != 0) eat();
			eat(); // Eat '\n'
			return { TokenKind::COMMENT, beg, position_.delta(beg) }; // '//'
		case '*':
			eat(); // Eat '*'
			for (Ulen i = 1; i != 0; /**/) switch (rune_) {
			case 0:
				// EOF
				i = 0;
				break;
			case '/':
				eat(); // Eat '/'
				if (rune_ == '*') {
					eat(); // Eat '*'
					i++;
				}
				break;
			case '*':
				eat(); // Eat '*'
				if (rune_ == '/') {
					eat(); // Eat '/'
					i--;
				}
				break;
			default:
				eat(); // Eat what ever is in the comment
				break;
			}
			// This also limits comments to no more than 64 KiB
			return { TokenKind::COMMENT, beg, position_.delta(beg) }; // '/*'
		case '=':
			eat(); // '='
			return { AssignKind::QUO, beg, 2_u16 }; // '/='
		}
		return { OperatorKind::QUO, beg, 1_u16 }; // '/'
	case '~':
		eat(); // Eat '~'
		if (rune_ == '=') {
			eat(); // Eat '='
			return { AssignKind::XOR, beg, 2_u16 };
		}
		return { OperatorKind::XOR, beg, 1_u16 };
	case '!':
		eat(); // Eat '!'
		if (rune_ == '=') {
			eat(); // Eat '='
			return { OperatorKind::NEQ, beg, 2_u16 };
		}
		return { OperatorKind::LNOT, beg, 1_u16 };
	case '+':
		eat(); // Eat '+'
		if (rune_ == '=') {
			eat(); // Eat '='
			return { AssignKind::ADD, beg, 2_u16 };
		}
		return { OperatorKind::ADD, beg, 1_u16 };
	case '-':
		eat(); // Eat '-'
		switch (rune_) {
		case '=':
			eat(); // Eat '='
			return { AssignKind::SUB, beg, 2_u16 };
		case '>':
			eat(); // Eat '>'
			return { OperatorKind::ARROW, beg, 2_u16 };
		case '-':
			eat(); // Eat '-'
			if (rune_ == '-') {
				return { TokenKind::UNDEFINED, beg, 3_u16 };
			} else {
				// ERROR: Decrement operator '--' isn't allowed in Odin
			}
		}
		return { OperatorKind::SUB, beg, 1_u16 };
	case '=':
		eat(); // Eat '='
		if (rune_ == '=') {
			return { AssignKind::EQ, beg, 2_u16 };
		}
		return { OperatorKind::EQ, beg, 1_u16 };
	case '.':
		eat(); // Eat '.'
		switch (rune_) {
		case '.':
			eat(); // Eat '.'
			switch (rune_) {
			case '<': // ..<
				eat(); // Eat '<'
				return { OperatorKind::RANGEHALF, beg, position_.delta(beg) }; // '..<'
			case '=': // ..=
				eat(); // Eat '='
				return { OperatorKind::RANGEFULL, beg, position_.delta(beg) }; // '..='
			}
			return { OperatorKind::ELLIPSIS, beg, 2_u16 }; // '..'
		case '0': case '1': case '2': case '3': case '4':
		case '5': case '6': case '7': case '8': case '9':
			return scan_number(true);
		}
		return { OperatorKind::PERIOD, beg, 1_u16 }; // '.'
	case '<':
		eat(); // Eat '<'
		switch (rune_) {
		case '=':
			eat(); // Eat '='
			return { OperatorKind::LT, beg, 2_u16 }; // '<='
		case '<':
			eat(); // Eat '<'
			if (rune_ == '=') {
				eat(); // Eat '='
				return { AssignKind::SHL, beg, 3_u16 }; // '<<'
			}
			return { OperatorKind::SHL, beg, 2_u16 }; // '<<='
		}
		return { OperatorKind::LT, beg, 1_u16 }; // '<'
	case '>':
		eat(); // Eat '>'
		switch (rune_) {
		case '=':
			eat(); // Eat '='
			return { OperatorKind::GT, beg, 2_u16 }; // '>='
		case '>':
			eat(); // Eat '>'
			if (rune_ == '=') {
				eat(); // Eat '='
				return { AssignKind::SHR, beg, 3_u16 }; // '>>'
			}
			return { OperatorKind::SHR, beg, 2_u16 }; // '>>='
		}
		return { OperatorKind::GT, beg, 1_u16 }; // '>'
	case '&':
		eat(); // Eat '&'
		switch (rune_) {
		case '~':
			eat(); // Eat '~'
			if (rune_ == '=') {
				eat(); // Eat '='
				return { AssignKind::ANDNOT, beg, 3_u16 }; // '&~='
			}
			return { OperatorKind::ANDNOT, beg, 2_u16 }; // '&~'
		case '=':
			eat(); // Eat '='
			return { AssignKind::BAND, beg, 2_u16 }; // '&='
		case '&':
			eat(); // Eat '&'
			if (rune_ == '=') {
				eat(); // Eat '='
				return { AssignKind::LAND, beg, 3_u16 }; // '&&='
			}
			return { OperatorKind::LAND, beg, 2_u16 }; // '&&'
		}
		return { OperatorKind::BAND, beg, 1_u16 };
	case '|':
		eat(); // Eat '|'
		switch (rune_) {
		case '=':
			eat(); // Eat '='
			return { AssignKind::BOR, beg, 2_u16 }; // '|='
		case '|':
			eat(); // Eat '|'
			if (rune_ == '=') {
				eat(); // Eat '='
				return { AssignKind::LOR, beg, 3_u16 }; // '||='
			}
			return { OperatorKind::LOR, beg, 2_u16 }; // '|='
		}
		return { OperatorKind::BOR, beg, 1_u16 }; // '|'
	case '`':
		[[fallthrough]];
	case '"':
		return scan_string();
	default:
		eat();
		return { TokenKind::INVALID, beg, 1_u16 };
	}
	return { TokenKind::ENDOF, beg, 1_u16 };
}

Token Lexer::next() {
	static constexpr const Bool KIND_ASI[] = {
		#define KIND(ENUM, NAME, ASI) ASI,
		#include "lexer.inl"
	};
	static constexpr const Bool OPERATOR_ASI[] = {
		#define OPERATOR(ENUM, NAME, MATCH, PREC, NAMEED, ASI) ASI,
		#include "lexer.inl"
	};
	static constexpr const Bool KEYWORD_ASI[] = {
		#define KEYWORD(ENUM, MATCH, ASI) ASI,
		#include "lexer.inl"
	};
	const auto token = advance();
	Bool asi = false;
	switch (token.kind) {
	case TokenKind::OPERATOR:
		asi = OPERATOR_ASI[Uint32(token.as_operator)];
		break;
	case TokenKind::KEYWORD:
		asi = KEYWORD_ASI[Uint32(token.as_keyword)];
		break;
	default:
		asi = KIND_ASI[Uint32(token.kind)];
		break;
	}
	asi_ = asi;
	return token;
}

} // namespace Thor
