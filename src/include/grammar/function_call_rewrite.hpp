#pragma once

#include "grammar/function_call.hpp"

#include <functional>

namespace duckdb {

//! Changes how calls to one or more functions are parsed. Add a new rewrite by writing a file in src/grammar/
//! that returns one of these, and listing it in GetFunctionCallRewrites().
struct FunctionCallRewrite {
	//! Unqualified function names this rewrite applies to, matched case-insensitively
	vector<string> names;
	//! Adjusts the call before DuckDB builds the expression
	std::function<void(FunctionCall &call)> rewrite_call;
	//! Optionally wraps or replaces the expression DuckDB built
	std::function<unique_ptr<ParsedExpression>(unique_ptr<ParsedExpression> expr)> rewrite_result;
};

//! Every rewrite the snowflake grammar applies
const vector<FunctionCallRewrite> &GetFunctionCallRewrites();

FunctionCallRewrite ListaggRewrite();
FunctionCallRewrite ArrayAggRewrite();
FunctionCallRewrite DateaddRewrite();

//! COALESCE(expr, fallback), keeping expr's text as the column name
unique_ptr<ParsedExpression> CoalesceTo(unique_ptr<ParsedExpression> expr, unique_ptr<ParsedExpression> fallback);

} // namespace duckdb
