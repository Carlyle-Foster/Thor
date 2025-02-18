#include "util/system.h"
#include "util/file.h"

#include "parser.h"

#include <stdio.h>

namespace Thor {
	extern const System STD_SYSTEM;
}

using namespace Thor;

int main(int, char **) {
	auto file = File::open(STD_SYSTEM, "test/ks.odin", File::Access::RD);
	if (!file) {
		return 1;
	}

	const auto& sys = STD_SYSTEM;
	SystemAllocator allocator{sys};

	auto data = file->map(allocator);
	auto view = data.slice().cast<const char>();
	if (auto lexer = Lexer::open(view, allocator)) {
		Parser parser{sys, *lexer};
		auto& ast = parser.ast();
		Array<AstRef<AstStmt>> stmts{allocator};
		for (;;) {
			auto stmt = parser.parse_stmt();
			if (!stmt || !stmts.push_back(stmt)) {
				break;
			}
		}
		StringBuilder builder{allocator};
		Ulen n_stmts = stmts.length();
		for (Ulen i = 0; i < n_stmts; i++) {
			ast[stmts[i]].dump(ast, builder);
		}
		if (auto dump = builder.result()) {
			sys.console.write(sys, *dump);
		}
		return 0;
	}
	return 1;
	/*
	for (;;) {
		auto token = lexer->next();
		token.dump(STD_SYSTEM, view);
		if (token.kind == TokenKind::ENDOF) {
			break;
		}
	}*/
}
