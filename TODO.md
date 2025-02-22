# TODO List
## Parser
### Types
* StructType
* ProcType
* BitSetType
  - `'bit_set' '[' Expr (';' Type)? ']'`
### Expressions
* RangeExpr
  - `Expr '..' ('=' | '<') Expr`
* ImagExpr
  - `ImagLiteral`
### Statements
* IfStmt
* ForeignStmt
* ForeignImportStmt
* ForStmt
* SwitchStmt
* DeclStmt
  - `Ident (',' Ident)* ':' Type? (('=' | ':') (Expr ',')* Expr?)?`
## IR
Everything
## Driver
Everything
## CG
Everything