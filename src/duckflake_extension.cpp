#define DUCKDB_EXTENSION_MAIN

#include "duckflake_extension.hpp"
#include "snowflake_grammar.hpp"
#include "duckdb.hpp"
#include "duckdb/function/scalar_function.hpp"

namespace duckdb {

// Smoke-test function so we can verify the extension loads; remove once real functionality lands.
inline void DuckflakeVersionFun(DataChunk &args, ExpressionState &state, Vector &result) {
	result.SetVectorType(VectorType::CONSTANT_VECTOR);
	ConstantVector::GetData<string_t>(result)[0] = StringVector::AddString(result, DuckflakeExtension().Version());
}

static void LoadInternal(ExtensionLoader &loader) {
	loader.RegisterFunction(ScalarFunction("duckflake_version", {}, LogicalType::VARCHAR, DuckflakeVersionFun));
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
