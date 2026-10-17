#pragma once

#include "duckdb/main/extension/extension_loader.hpp"

namespace duckdb {

//! Registers every plain Snowflake function. Add a new function by writing a file in src/functions/ that defines
//! a Register<Name> function, declaring it here, and calling it from RegisterFunctions().
void RegisterFunctions(ExtensionLoader &loader);

void RegisterDuckflakeVersion(ExtensionLoader &loader);
void RegisterIff(ExtensionLoader &loader);

} // namespace duckdb
