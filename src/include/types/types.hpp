#pragma once

#include "duckdb/main/extension/extension_loader.hpp"

namespace duckdb {

//! Registers every Snowflake type name. Add a new type by writing a file in src/types/ that defines a
//! Register<Name>Type function, declaring it here, and calling it from RegisterTypes().
void RegisterTypes(ExtensionLoader &loader);

void RegisterNumberType(ExtensionLoader &loader);

} // namespace duckdb
