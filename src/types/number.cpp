// Snowflake: NUMBER [ ( precision [, scale] ) ]
//   NUMBER        -> DECIMAL(38, 0)
//   NUMBER(p)     -> DECIMAL(p, 0)
//   NUMBER(p, s)  -> DECIMAL(p, s)    precision 1-38, scale 0-precision
// DuckDB's own DECIMAL / NUMERIC default to (18, 3), so only the Snowflake name gets Snowflake's default.
// https://docs.snowflake.com/en/sql-reference/data-types-numeric#number

#include "duckdb/common/types/decimal.hpp"
#include "duckdb/function/type_constructor.hpp"
#include "types/types.hpp"

namespace duckdb {

static constexpr uint8_t DEFAULT_PRECISION = 38;
static constexpr uint8_t DEFAULT_SCALE = 0;

static LogicalType BindDefaultNumber(BindLogicalTypeInput &input) {
	return LogicalType::DECIMAL(DEFAULT_PRECISION, DEFAULT_SCALE);
}

static LogicalType BindNumber(BindLogicalTypeInput &input) {
	auto precision = input.modifiers[0].GetValue().GetValue<uint8_t>();
	auto scale = input.modifiers[1].GetValue().GetValue<uint8_t>();
	if (precision < 1 || precision > Decimal::MAX_WIDTH_DECIMAL) {
		throw BinderException(input.GetLocation(0), "NUMBER precision must be between 1 and %d",
		                      Decimal::MAX_WIDTH_DECIMAL);
	}
	if (scale > precision) {
		throw BinderException(input.GetLocation(1), "NUMBER scale cannot be greater than its precision");
	}
	return LogicalType::DECIMAL(precision, scale);
}

void RegisterNumberType(ExtensionLoader &loader) {
	TypeConstructorSet constructors("number");
	constructors.AddFunction(TypeConstructor(TypeConstructor::Signature(), BindDefaultNumber));

	auto signature = TypeConstructor::Signature();
	signature.AddParameter("precision", LogicalType::UTINYINT);
	signature.AddParameter("scale", LogicalType::UTINYINT, Value::UTINYINT(DEFAULT_SCALE));
	constructors.AddFunction(TypeConstructor(std::move(signature), BindNumber));

	loader.RegisterType("number", LogicalType::DECIMAL(DEFAULT_PRECISION, DEFAULT_SCALE), std::move(constructors));
}

} // namespace duckdb
