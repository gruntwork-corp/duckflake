// Snowflake timestamp types, with an optional precision p (0-9 fractional digits):
//
//   TIMESTAMP_NTZ, TIMESTAMPNTZ   wall-clock time
//     -> TIMESTAMP_S (p = 0), TIMESTAMP_MS (1-3), TIMESTAMP (4-6), TIMESTAMP_NS (7-9)
//   TIMESTAMP_LTZ, TIMESTAMPLTZ   UTC instant
//     -> TIMESTAMPTZ (0-6), TIMESTAMPTZ_NS (7-9)
//   TIMESTAMP_TZ                  instant + offset
//     -> TIMESTAMPTZ (0-6), TIMESTAMPTZ_NS (7-9)
//
// Without a precision they default to microseconds, so TIMESTAMP_NTZ is the same type as DuckDB's TIMESTAMP
// (Snowflake's default is 9). DuckDB has no type that keeps a per-value offset, so TIMESTAMP_TZ keeps the instant
// but shows it in the session time zone. DuckDB's TIMESTAMPTZ only comes in microseconds and nanoseconds, so LTZ / TZ
// precisions below 6 are not truncated.
// https://docs.snowflake.com/en/sql-reference/data-types-datetime

#include "duckdb/function/type_constructor.hpp"
#include "types/types.hpp"

namespace duckdb {

static constexpr uint8_t MAX_PRECISION = 9;

static uint8_t GetPrecision(BindLogicalTypeInput &input) {
	auto precision = input.modifiers[0].GetValue().GetValue<uint8_t>();
	if (precision > MAX_PRECISION) {
		throw BinderException(input.GetLocation(0), "Timestamp precision must be between 0 and %d", MAX_PRECISION);
	}
	return precision;
}

LogicalType TimestampNtzForPrecision(uint8_t precision) {
	if (precision == 0) {
		return LogicalType::TIMESTAMP_S;
	}
	if (precision <= 3) {
		return LogicalType::TIMESTAMP_MS;
	}
	if (precision <= 6) {
		return LogicalType::TIMESTAMP;
	}
	return LogicalType::TIMESTAMP_NS;
}

LogicalType TimestampTzForPrecision(uint8_t precision) {
	return precision <= 6 ? LogicalType::TIMESTAMP_TZ : LogicalType::TIMESTAMP_TZ_NS;
}

static LogicalType BindDefaultNtz(BindLogicalTypeInput &input) {
	return LogicalType::TIMESTAMP;
}

static LogicalType BindNtz(BindLogicalTypeInput &input) {
	return TimestampNtzForPrecision(GetPrecision(input));
}

static LogicalType BindDefaultTz(BindLogicalTypeInput &input) {
	return LogicalType::TIMESTAMP_TZ;
}

static LogicalType BindTz(BindLogicalTypeInput &input) {
	return TimestampTzForPrecision(GetPrecision(input));
}

static void RegisterTimestampType(ExtensionLoader &loader, const char *name, const LogicalType &default_type,
                                  bind_logical_type_function_t bind_default, bind_logical_type_function_t bind) {
	TypeConstructorSet constructors(name);
	constructors.AddFunction(TypeConstructor(TypeConstructor::Signature(), bind_default));
	auto signature = TypeConstructor::Signature();
	signature.AddParameter("precision", LogicalType::UTINYINT);
	constructors.AddFunction(TypeConstructor(std::move(signature), bind));
	loader.RegisterType(name, default_type, std::move(constructors));
}

void RegisterTimestampTypes(ExtensionLoader &loader) {
	for (auto name : {"timestamp_ntz", "timestampntz"}) {
		RegisterTimestampType(loader, name, LogicalType::TIMESTAMP, BindDefaultNtz, BindNtz);
	}
	for (auto name : {"timestamp_ltz", "timestampltz", "timestamp_tz"}) {
		RegisterTimestampType(loader, name, LogicalType::TIMESTAMP_TZ, BindDefaultTz, BindTz);
	}
}

} // namespace duckdb
