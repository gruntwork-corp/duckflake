// Snowflake: BOOLOR_AGG( expr )
// TRUE if any non-NULL value is TRUE; numbers count as TRUE when non-zero. NULL for empty or all-NULL input.
// A macro, so it works as an aggregate and as a window function (OVER) like bool_or.
// https://docs.snowflake.com/en/sql-reference/functions/boolor_agg

#include "duckdb/catalog/default/default_functions.hpp"
#include "duckdb/parser/parsed_data/create_macro_info.hpp"
#include "functions/functions.hpp"

namespace duckdb {

static const DefaultMacro BOOLOR_AGG_MACRO = {DEFAULT_SCHEMA, "boolor_agg", "(expr) AS bool_or(CAST(expr AS BOOLEAN))"};

void RegisterBoolorAgg(ExtensionLoader &loader) {
	auto info = DefaultFunctionGenerator::CreateInternalMacroInfo(BOOLOR_AGG_MACRO);
	loader.RegisterFunction(*info);
}

} // namespace duckdb
