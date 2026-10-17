// Snowflake: MONTHS_BETWEEN( date_expr1, date_expr2 )
// Whole months from the year and month parts, plus a fraction from the day and time parts, counting every month
// as 31 days. The fraction is 0 when both inputs fall on the same day of the month, or both on the last day of their
// month. Negative when date_expr1 is earlier. Returns DOUBLE.
// https://docs.snowflake.com/en/sql-reference/functions/months_between

#include "duckdb/catalog/default/default_functions.hpp"
#include "duckdb/parser/parsed_data/create_macro_info.hpp"
#include "functions/functions.hpp"

namespace duckdb {

// Both inputs are cast to TIMESTAMP first so DATE, TIMESTAMP and untyped NULL all bind
static const DefaultMacro MONTHS_BETWEEN_MACRO = {
    DEFAULT_SCHEMA, "months_between",
    "(date_expr1, date_expr2) AS CAST("
    "  (year(CAST(date_expr1 AS TIMESTAMP)) - year(CAST(date_expr2 AS TIMESTAMP))) * 12"
    "  + month(CAST(date_expr1 AS TIMESTAMP)) - month(CAST(date_expr2 AS TIMESTAMP))"
    "  + CASE"
    "      WHEN day(CAST(date_expr1 AS TIMESTAMP)) = day(CAST(date_expr2 AS TIMESTAMP)) THEN 0"
    "      WHEN last_day(CAST(date_expr1 AS DATE)) = CAST(date_expr1 AS DATE)"
    "       AND last_day(CAST(date_expr2 AS DATE)) = CAST(date_expr2 AS DATE) THEN 0"
    "      ELSE (day(CAST(date_expr1 AS TIMESTAMP)) - day(CAST(date_expr2 AS TIMESTAMP))"
    "            + (epoch(CAST(date_expr1 AS TIMESTAMP)) - epoch(date_trunc('day', CAST(date_expr1 AS TIMESTAMP)))"
    "               - epoch(CAST(date_expr2 AS TIMESTAMP)) + epoch(date_trunc('day', CAST(date_expr2 AS TIMESTAMP))))"
    "              / 86400) / 31"
    "    END"
    " AS DOUBLE)"};

void RegisterMonthsBetween(ExtensionLoader &loader) {
	auto info = DefaultFunctionGenerator::CreateInternalMacroInfo(MONTHS_BETWEEN_MACRO);
	loader.RegisterFunction(*info);
}

} // namespace duckdb
