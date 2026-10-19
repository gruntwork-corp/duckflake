#include "types/types.hpp"

namespace duckdb {

void RegisterTypes(ExtensionLoader &loader) {
	RegisterNumberType(loader);
}

} // namespace duckdb
