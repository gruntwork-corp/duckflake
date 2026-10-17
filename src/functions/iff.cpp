// Snowflake: IFF( condition, expr1, expr2 )
// Returns expr1 when condition is TRUE, otherwise expr2 (FALSE and NULL both pick expr2).
// A macro over CASE, so only the chosen branch is evaluated and the result type follows CASE's rules.
// https://docs.snowflake.com/en/sql-reference/functions/iff

#include "duckdb/catalog/default/default_functions.hpp"
#include "duckdb/parser/parsed_data/create_macro_info.hpp"
#include "functions/functions.hpp"

namespace duckdb {

static const DefaultMacro IFF_MACRO = {DEFAULT_SCHEMA, "iff",
                                       "(condition, expr1, expr2) AS CASE WHEN condition THEN expr1 ELSE expr2 END"};

void RegisterIff(ExtensionLoader &loader) {
	auto info = DefaultFunctionGenerator::CreateInternalMacroInfo(IFF_MACRO);
	loader.RegisterFunction(*info);
}

} // namespace duckdb
