#include "functions/functions.hpp"

namespace duckdb {

void RegisterFunctions(ExtensionLoader &loader) {
	RegisterDuckflakeVersion(loader);
	RegisterBoolorAgg(loader);
	RegisterDateadd(loader);
	RegisterDayOfWeekIso(loader);
	RegisterIff(loader);
	RegisterMonthsBetween(loader);
	RegisterToArray(loader);
}

} // namespace duckdb
