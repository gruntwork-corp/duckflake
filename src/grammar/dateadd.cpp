// Snowflake: DATEADD( day, 1, d )   (also TIMEADD, TIMESTAMPADD)
// DuckDB:    dateadd('day', 1, d)
// Snowflake writes the date or time part as a bare word, which DuckDB would read as a column name. Turn it into a
// string so the dateadd function in src/functions/dateadd.cpp receives it.

#include "duckdb/parser/expression/columnref_expression.hpp"
#include "duckdb/parser/expression/constant_expression.hpp"
#include "grammar/function_call_rewrite.hpp"

namespace duckdb {

static void RewriteCall(FunctionCall &call) {
	auto &arguments = call.Arguments().arguments;
	if (arguments.empty() || arguments[0].HasName()) {
		return;
	}
	auto &part = arguments[0].GetExpressionMutable();
	if (part->GetExpressionClass() != ExpressionClass::COLUMN_REF) {
		return;
	}
	auto &column_ref = part->Cast<ColumnRefExpression>();
	if (column_ref.IsQualified()) {
		return;
	}
	part = make_uniq<ConstantExpression>(Literal::String(column_ref.GetColumnName().GetIdentifierName()));
}

FunctionCallRewrite DateaddRewrite() {
	return {{"dateadd", "timeadd", "timestampadd"}, RewriteCall, nullptr};
}

} // namespace duckdb
