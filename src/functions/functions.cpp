#include "functions/functions.hpp"

namespace duckdb {

void RegisterFunctions(ExtensionLoader &loader) {
	RegisterDuckflakeVersion(loader);
	RegisterIff(loader);
}

} // namespace duckdb
