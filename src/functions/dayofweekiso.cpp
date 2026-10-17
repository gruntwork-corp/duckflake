// Snowflake: DAYOFWEEKISO( date_or_timestamp )
// ISO day of the week: 1 = Monday ... 7 = Sunday, independent of the WEEK_START session parameter.
// https://docs.snowflake.com/en/sql-reference/functions/year

#include "duckdb/catalog/default/default_functions.hpp"
#include "duckdb/parser/parsed_data/create_macro_info.hpp"
#include "functions/functions.hpp"

namespace duckdb {

static const DefaultMacro DAYOFWEEKISO_MACRO = {DEFAULT_SCHEMA, "dayofweekiso",
                                                "(date_or_timestamp) AS isodow(date_or_timestamp)"};

void RegisterDayOfWeekIso(ExtensionLoader &loader) {
	auto info = DefaultFunctionGenerator::CreateInternalMacroInfo(DAYOFWEEKISO_MACRO);
	loader.RegisterFunction(*info);
}

} // namespace duckdb
