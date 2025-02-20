#include "util/system.h"
#include "util/file.h"
#include "util/map.h"

#include "parser.h"

namespace Thor {
	extern const Filesystem STD_FILESYSTEM;
	extern const Heap       STD_HEAP;
	extern const Console    STD_CONSOLE;
}

using namespace Thor;

int main(int, char **) {
	System sys {
		STD_FILESYSTEM,
		STD_HEAP,
		STD_CONSOLE
	};

	auto parser = Parser::open(sys, "test/ks.odin");
	if (!parser) {
		return 1;
	}
	auto& ast = parser->ast();
	Array<AstRef<AstStmt>> stmts{sys.allocator};
	for (;;) {
		auto stmt = parser->parse_stmt();
		if (!stmt || !stmts.push_back(move(stmt))) {
			break;
		}
	}
	StringBuilder builder{sys.allocator};
	for (auto stmt : stmts) {
		ast[stmt].dump(ast, builder, 0);
	}
	if (auto result = builder.result()) {
		sys.console.write(sys, *result);
	}
}
