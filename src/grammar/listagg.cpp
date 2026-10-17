// Snowflake: LISTAGG( [DISTINCT] x [, delimiter] ) [WITHIN GROUP (ORDER BY ...)]
// DuckDB:    COALESCE(listagg([DISTINCT] x, delimiter ORDER BY ...), '')
// https://docs.snowflake.com/en/sql-reference/functions/listagg

#include "duckdb/parser/expression/constant_expression.hpp"
#include "grammar/function_call_rewrite.hpp"

namespace duckdb {

static unique_ptr<ParsedExpression> EmptyString() {
	return make_uniq<ConstantExpression>(Literal::String(""));
}

static void RewriteCall(FunctionCall &call) {
	call.MoveWithinGroupToOrderBy();
	// Snowflake's default delimiter is the empty string; DuckDB's is ','
	if (call.Arguments().arguments.size() == 1) {
		call.AddArgument(EmptyString());
	}
}

// Snowflake returns '' rather than NULL for empty or all-NULL input
static unique_ptr<ParsedExpression> RewriteResult(unique_ptr<ParsedExpression> expr) {
	return CoalesceTo(std::move(expr), EmptyString());
}

FunctionCallRewrite ListaggRewrite() {
	return {{"listagg"}, RewriteCall, RewriteResult};
}

} // namespace duckdb
