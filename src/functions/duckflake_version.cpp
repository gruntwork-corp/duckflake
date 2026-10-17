// duckflake_version(): the extension's version, to check which build is loaded.

#include "duckdb/function/scalar_function.hpp"
#include "duckflake_extension.hpp"
#include "functions/functions.hpp"

namespace duckdb {

static void DuckflakeVersionFun(DataChunk &args, ExpressionState &state, Vector &result) {
	result.SetVectorType(VectorType::CONSTANT_VECTOR);
	ConstantVector::GetData<string_t>(result)[0] = StringVector::AddString(result, DuckflakeExtension().Version());
}

void RegisterDuckflakeVersion(ExtensionLoader &loader) {
	loader.RegisterFunction(ScalarFunction("duckflake_version", {}, LogicalType::VARCHAR, DuckflakeVersionFun));
}

} // namespace duckdb
