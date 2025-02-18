#include "util/file.h"

#include "ast.h"

namespace Thor {

void AstPackageStmt::dump(StringBuilder& builder) const {
	builder.put("package");
	builder.put(' ');
	builder.put(name);
	builder.put(';');
	builder.put('\n');
}

void AstImportStmt::dump(StringBuilder& builder) const {
	builder.put("import");
	builder.put(' ');
	builder.put(path);
	builder.put(';');
	builder.put('\n');
}

} // namespace Thor