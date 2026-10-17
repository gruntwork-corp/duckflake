// Snowflake: TO_ARRAY( expr )
// A list is returned unchanged, NULL stays NULL, and any other value becomes a single-element list.
// https://docs.snowflake.com/en/sql-reference/functions/to_array

#include "duckdb/common/vector/list_vector.hpp"
#include "duckdb/common/vector/vector_iterator.hpp"
#include "duckdb/common/vector/vector_writer.hpp"
#include "duckdb/function/scalar_function.hpp"
#include "functions/functions.hpp"

namespace duckdb {

static unique_ptr<FunctionData> ToArrayBind(BindScalarFunctionInput &input) {
	auto &input_type = input.GetArguments()[0]->GetReturnType();
	auto &bound_function = input.GetBoundFunction();
	if (input_type.id() == LogicalTypeId::LIST) {
		bound_function.SetReturnType(input_type);
	} else if (input_type.id() == LogicalTypeId::SQLNULL) {
		bound_function.SetReturnType(LogicalType::LIST(LogicalType::INTEGER));
	} else {
		bound_function.SetReturnType(LogicalType::LIST(input_type));
	}
	return nullptr;
}

static void ToArrayFunction(DataChunk &args, ExpressionState &state, Vector &result) {
	auto &input = args.data[0];
	if (input.GetType().id() == LogicalTypeId::LIST) {
		result.Reference(input);
		return;
	}
	auto count = args.size();
	auto offset = ListVector::GetListSize(result);
	if (input.GetType().id() != LogicalTypeId::SQLNULL) {
		ListVector::Append(result, input, count);
	}
	auto validity = input.Validity();
	auto writer = FlatVector::Writer<list_entry_t>(result, count);
	for (idx_t row = 0; row < count; row++) {
		if (!validity.IsValid(row)) {
			writer.WriteNull();
			continue;
		}
		writer.WriteValue(list_entry_t(offset + row, 1));
	}
}

void RegisterToArray(ExtensionLoader &loader) {
	loader.RegisterFunction(
	    ScalarFunction("to_array", {LogicalType::ANY}, LogicalType::ANY, ToArrayFunction, ToArrayBind));
}

} // namespace duckdb
