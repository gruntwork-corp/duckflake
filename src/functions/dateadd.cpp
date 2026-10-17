// Snowflake: DATEADD( part, value, date_or_time_expr )   (aliases TIMEADD, TIMESTAMPADD)
// Adds value units of part. Return type follows Snowflake:
//   DATE + day-or-larger part -> DATE        DATE + smaller part -> TIMESTAMP (from midnight)
//   TIMESTAMP -> TIMESTAMP                   TIME + hour-or-smaller part -> TIME (wraps around midnight)
// Adding months clamps to the end of a shorter month (Jan 31 + 1 month = Feb 28).
// The part must be a constant. With the snowflake grammar active it may be written unquoted: DATEADD(day, 1, d).
// https://docs.snowflake.com/en/sql-reference/functions/dateadd

#include "duckdb/common/case_insensitive_map.hpp"
#include "duckdb/common/operator/multiply.hpp"
#include "duckdb/common/types/interval.hpp"
#include "duckdb/common/types/timestamp.hpp"
#include "duckdb/common/vector_operations/binary_executor.hpp"
#include "duckdb/function/scalar_function.hpp"
#include "duckdb/planner/expression/bound_function_expression.hpp"
#include "functions/functions.hpp"

namespace duckdb {

enum class DateaddPart : uint8_t { YEAR, QUARTER, MONTH, WEEK, DAY, HOUR, MINUTE, SECOND, MILLISECOND, MICROSECOND };

// Snowflake's names and abbreviations: https://docs.snowflake.com/en/sql-reference/functions-date-time
static DateaddPart ParsePart(const string &name) {
	static const case_insensitive_map_t<DateaddPart> PARTS = {
	    {"year", DateaddPart::YEAR},
	    {"y", DateaddPart::YEAR},
	    {"yy", DateaddPart::YEAR},
	    {"yyy", DateaddPart::YEAR},
	    {"yyyy", DateaddPart::YEAR},
	    {"yr", DateaddPart::YEAR},
	    {"years", DateaddPart::YEAR},
	    {"yrs", DateaddPart::YEAR},
	    {"quarter", DateaddPart::QUARTER},
	    {"q", DateaddPart::QUARTER},
	    {"qtr", DateaddPart::QUARTER},
	    {"qtrs", DateaddPart::QUARTER},
	    {"quarters", DateaddPart::QUARTER},
	    {"month", DateaddPart::MONTH},
	    {"mm", DateaddPart::MONTH},
	    {"mon", DateaddPart::MONTH},
	    {"mons", DateaddPart::MONTH},
	    {"months", DateaddPart::MONTH},
	    {"week", DateaddPart::WEEK},
	    {"w", DateaddPart::WEEK},
	    {"wk", DateaddPart::WEEK},
	    {"weekofyear", DateaddPart::WEEK},
	    {"woy", DateaddPart::WEEK},
	    {"wy", DateaddPart::WEEK},
	    {"weeks", DateaddPart::WEEK},
	    {"day", DateaddPart::DAY},
	    {"d", DateaddPart::DAY},
	    {"dd", DateaddPart::DAY},
	    {"days", DateaddPart::DAY},
	    {"dayofmonth", DateaddPart::DAY},
	    {"hour", DateaddPart::HOUR},
	    {"h", DateaddPart::HOUR},
	    {"hh", DateaddPart::HOUR},
	    {"hr", DateaddPart::HOUR},
	    {"hours", DateaddPart::HOUR},
	    {"hrs", DateaddPart::HOUR},
	    {"minute", DateaddPart::MINUTE},
	    {"m", DateaddPart::MINUTE},
	    {"mi", DateaddPart::MINUTE},
	    {"min", DateaddPart::MINUTE},
	    {"minutes", DateaddPart::MINUTE},
	    {"mins", DateaddPart::MINUTE},
	    {"second", DateaddPart::SECOND},
	    {"s", DateaddPart::SECOND},
	    {"sec", DateaddPart::SECOND},
	    {"seconds", DateaddPart::SECOND},
	    {"secs", DateaddPart::SECOND},
	    {"millisecond", DateaddPart::MILLISECOND},
	    {"ms", DateaddPart::MILLISECOND},
	    {"msec", DateaddPart::MILLISECOND},
	    {"milliseconds", DateaddPart::MILLISECOND},
	    {"microsecond", DateaddPart::MICROSECOND},
	    {"us", DateaddPart::MICROSECOND},
	    {"usec", DateaddPart::MICROSECOND},
	    {"microseconds", DateaddPart::MICROSECOND},
	};
	auto entry = PARTS.find(name);
	if (entry == PARTS.end()) {
		throw InvalidInputException("DATEADD: unsupported date or time part '%s'", name);
	}
	return entry->second;
}

static bool IsDayOrLarger(DateaddPart part) {
	return part <= DateaddPart::DAY;
}

static int64_t Multiply(int64_t value, int64_t factor) {
	int64_t result;
	if (!TryMultiplyOperator::Operation(value, factor, result)) {
		throw OutOfRangeException("DATEADD: value %lld is out of range", value);
	}
	return result;
}

static int32_t ToInt32(int64_t value) {
	if (value < NumericLimits<int32_t>::Minimum() || value > NumericLimits<int32_t>::Maximum()) {
		throw OutOfRangeException("DATEADD: value %lld is out of range", value);
	}
	return static_cast<int32_t>(value);
}

static interval_t ToInterval(DateaddPart part, int64_t value) {
	interval_t result {0, 0, 0};
	switch (part) {
	case DateaddPart::YEAR:
		result.months = ToInt32(Multiply(value, Interval::MONTHS_PER_YEAR));
		break;
	case DateaddPart::QUARTER:
		result.months = ToInt32(Multiply(value, 3));
		break;
	case DateaddPart::MONTH:
		result.months = ToInt32(value);
		break;
	case DateaddPart::WEEK:
		result.days = ToInt32(Multiply(value, Interval::DAYS_PER_WEEK));
		break;
	case DateaddPart::DAY:
		result.days = ToInt32(value);
		break;
	case DateaddPart::HOUR:
		result.micros = Multiply(value, Interval::MICROS_PER_HOUR);
		break;
	case DateaddPart::MINUTE:
		result.micros = Multiply(value, Interval::MICROS_PER_MINUTE);
		break;
	case DateaddPart::SECOND:
		result.micros = Multiply(value, Interval::MICROS_PER_SEC);
		break;
	case DateaddPart::MILLISECOND:
		result.micros = Multiply(value, Interval::MICROS_PER_MSEC);
		break;
	case DateaddPart::MICROSECOND:
		result.micros = value;
		break;
	}
	return result;
}

struct DateaddBindData : public FunctionData {
	explicit DateaddBindData(DateaddPart part_p) : part(part_p) {
	}

	DateaddPart part;

	unique_ptr<FunctionData> Copy() const override {
		return make_uniq<DateaddBindData>(part);
	}
	bool Equals(const FunctionData &other) const override {
		return part == other.Cast<DateaddBindData>().part;
	}
};

static unique_ptr<FunctionData> DateaddBind(BindScalarFunctionInput &input) {
	auto part_value = input.TryGetConstant(0);
	if (!part_value) {
		throw BinderException("DATEADD: the date or time part must be a constant, such as day or 'day'");
	}
	if (part_value->IsNull()) {
		throw BinderException("DATEADD: the date or time part cannot be NULL");
	}
	auto part = ParsePart(part_value->ToString());

	auto &bound_function = input.GetBoundFunction();
	switch (bound_function.GetArguments()[2].id()) {
	case LogicalTypeId::DATE:
		bound_function.SetReturnType(IsDayOrLarger(part) ? LogicalType::DATE : LogicalType::TIMESTAMP);
		break;
	case LogicalTypeId::TIME:
		if (IsDayOrLarger(part)) {
			throw BinderException("DATEADD: TIME values only accept hour or smaller parts, not '%s'",
			                      part_value->ToString());
		}
		break;
	default:
		break;
	}
	return make_uniq<DateaddBindData>(part);
}

static DateaddPart GetPart(ExpressionState &state) {
	return state.expr.Cast<BoundFunctionExpression>().BindInfo()->Cast<DateaddBindData>().part;
}

static void DateaddDate(DataChunk &args, ExpressionState &state, Vector &result) {
	auto part = GetPart(state);
	if (result.GetType().id() == LogicalTypeId::DATE) {
		BinaryExecutor::Execute<int64_t, date_t, date_t>(
		    args.data[1], args.data[2], result,
		    [&](int64_t value, date_t date) { return Interval::Add(date, ToInterval(part, value)); });
		return;
	}
	BinaryExecutor::Execute<int64_t, date_t, timestamp_t>(
	    args.data[1], args.data[2], result, [&](int64_t value, date_t date) {
		    return Interval::Add(Timestamp::FromDatetime(date, dtime_t(0)), ToInterval(part, value));
	    });
}

static void DateaddTimestamp(DataChunk &args, ExpressionState &state, Vector &result) {
	auto part = GetPart(state);
	BinaryExecutor::Execute<int64_t, timestamp_t, timestamp_t>(
	    args.data[1], args.data[2], result,
	    [&](int64_t value, timestamp_t timestamp) { return Interval::Add(timestamp, ToInterval(part, value)); });
}

static void DateaddTime(DataChunk &args, ExpressionState &state, Vector &result) {
	auto part = GetPart(state);
	BinaryExecutor::Execute<int64_t, dtime_t, dtime_t>(
	    args.data[1], args.data[2], result, [&](int64_t value, dtime_t time) {
		    date_t ignored_date(0);
		    return Interval::Add(time, ToInterval(part, value), ignored_date);
	    });
}

void RegisterDateadd(ExtensionLoader &loader) {
	for (auto name : {"dateadd", "timeadd", "timestampadd"}) {
		ScalarFunctionSet set(name);
		set.AddFunction(ScalarFunction({LogicalType::VARCHAR, LogicalType::BIGINT, LogicalType::DATE},
		                               LogicalType::DATE, DateaddDate, DateaddBind));
		set.AddFunction(ScalarFunction({LogicalType::VARCHAR, LogicalType::BIGINT, LogicalType::TIMESTAMP},
		                               LogicalType::TIMESTAMP, DateaddTimestamp, DateaddBind));
		set.AddFunction(ScalarFunction({LogicalType::VARCHAR, LogicalType::BIGINT, LogicalType::TIME},
		                               LogicalType::TIME, DateaddTime, DateaddBind));
		// Invalid parts and out-of-range values throw at runtime
		set.SetFallible();
		loader.RegisterFunction(set);
	}
}

} // namespace duckdb
