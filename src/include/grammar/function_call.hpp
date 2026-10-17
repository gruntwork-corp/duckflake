#pragma once

#include "duckdb/parser/peg/ast/method_arguments.hpp"
#include "duckdb/parser/peg/transformer/peg_transformer.hpp"

namespace duckdb {

//! A function call as the parser sees it, before DuckDB turns it into an expression:
//!   name ( [DISTINCT] arguments [ORDER BY ...] ) [WITHIN GROUP (ORDER BY ...)] [FILTER (WHERE ...)] [OVER (...)]
//! Rewrites change the call through this class, so only function_call.cpp knows DuckDB's internal layout.
class FunctionCall {
public:
	FunctionCall(PEGTransformer &transformer, GeneratedTransformProcess &process);

	const QualifiedName &Name();
	//! Call a different function, keeping any schema qualification
	void Rename(const string &name);

	//! The arguments, DISTINCT flag and ORDER BY inside the parentheses
	MethodArguments &Arguments();
	void AddArgument(unique_ptr<ParsedExpression> argument);

	//! Turn WITHIN GROUP (ORDER BY ...) into the aggregate's own ORDER BY
	void MoveWithinGroupToOrderBy();

	//! Add a FILTER condition, ANDed with any FILTER the query already has
	void AddFilter(unique_ptr<ParsedExpression> condition);

private:
	PEGTransformer &transformer;
	GeneratedTransformProcess &process;
};

} // namespace duckdb
