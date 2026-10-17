// Snowflake: ARRAY_AGG( [DISTINCT] x ) [WITHIN GROUP (ORDER BY ...)]   (alias ARRAYAGG)
// DuckDB:    COALESCE(array_agg([DISTINCT] x ORDER BY ...) FILTER (WHERE x IS NOT NULL), [])
// https://docs.snowflake.com/en/sql-reference/functions/array_agg

#include "duckdb/parser/expression/function_expression.hpp"
#include "duckdb/parser/expression/operator_expression.hpp"
#include "grammar/function_call_rewrite.hpp"

namespace duckdb {

static void RewriteCall(FunctionCall &call) {
	call.Rename("array_agg");
	call.MoveWithinGroupToOrderBy();
	// Snowflake omits NULLs; DuckDB keeps them
	auto &arguments = call.Arguments().arguments;
	if (arguments.size() == 1) {
		call.AddFilter(
		    make_uniq<OperatorExpression>(ExpressionType::OPERATOR_IS_NOT_NULL, arguments[0].GetExpression().Copy()));
	}
}

// Snowflake returns [] rather than NULL for empty input
static unique_ptr<ParsedExpression> RewriteResult(unique_ptr<ParsedExpression> expr) {
	auto empty_list = make_uniq<FunctionExpression>(Identifier("list_value"), vector<FunctionArgument>());
	return CoalesceTo(std::move(expr), std::move(empty_list));
}

FunctionCallRewrite ArrayAggRewrite() {
	return {{"array_agg", "arrayagg"}, RewriteCall, RewriteResult};
}

} // namespace duckdb
