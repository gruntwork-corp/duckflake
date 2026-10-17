#define DUCKDB_EXTENSION_MAIN

#include "duckflake_extension.hpp"

#include "duckdb.hpp"
#include "functions/functions.hpp"
#include "grammar/snowflake_grammar.hpp"

namespace duckdb {

static void LoadInternal(ExtensionLoader &loader) {
	RegisterFunctions(loader);
	GrammarExtension::Register(loader.GetDatabaseInstance(), make_shared_ptr<SnowflakeGrammarExtension>());
}

void DuckflakeExtension::Load(ExtensionLoader &loader) {
	LoadInternal(loader);
}
std::string DuckflakeExtension::Name() {
	return "duckflake";
}

std::string DuckflakeExtension::Version() const {
#ifdef EXT_VERSION_DUCKFLAKE
	return EXT_VERSION_DUCKFLAKE;
#else
	return "";
#endif
}

} // namespace duckdb

extern "C" {

DUCKDB_CPP_EXTENSION_ENTRY(duckflake, loader) {
	duckdb::LoadInternal(loader);
}
}
