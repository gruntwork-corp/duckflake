// Snowflake time and timestamp spellings that DuckDB's parser or catalog rejects:
//   TIMESTAMPTZ(p)                   -> TIMESTAMP_TZ(p) (src/types/timestamp.cpp)
//   TIME(p)                          -> TIME (0-6) / TIME_NS (7-9)
//   TIMESTAMP WITH LOCAL TIME ZONE   -> TIMESTAMPTZ (same as TIMESTAMP_LTZ)
// Like function_call.cpp, this file knows the slot layout of DuckDB's generated transformers:
//   QualifiedSimpleType <- QualifiedTypeName TypeModifiers?
//   TimeType <- TimeOrTimestamp TypeModifiers? TimeZone?
// https://docs.snowflake.com/en/sql-reference/data-types-datetime

#include "duckdb/common/enum_util.hpp"
#include "duckdb/common/string_util.hpp"
#include "duckdb/parser/expression/constant_expression.hpp"
#include "duckdb/parser/expression/type_expression.hpp"
#include "duckdb/parser/peg/transformer/peg_transformer.hpp"
#include "grammar/snowflake_grammar.hpp"

namespace duckdb {

static constexpr idx_t SLOT_TYPE_NAME = 0;
static constexpr idx_t SLOT_TIME_OR_TIMESTAMP = 0;
static constexpr idx_t SLOT_TYPE_MODIFIERS = 1;
static constexpr idx_t SLOT_TIME_ZONE = 2;

// TIMESTAMPTZ is a DuckDB type without modifiers; with a precision, use duckflake's TIMESTAMP_TZ instead
static arena_ptr<TransformResultValue> FinalizeQualifiedSimpleType(PEGTransformer &transformer,
                                                                   GeneratedTransformProcess &process) {
	auto &name = process.GetResult<QualifiedName>(SLOT_TYPE_NAME);
	if (process.child_results[SLOT_TYPE_MODIFIERS] && name.Path().size() == 1 &&
	    StringUtil::CIEquals(name.Name().GetIdentifierName(), "timestamptz")) {
		name = name.WithName(Identifier("timestamp_tz"));
	}
	return PEGTransformerFactory::FinalizeQualifiedSimpleTypeTrampoline(transformer, process);
}

static uint8_t GetTimePrecision(const vector<unique_ptr<ParsedExpression>> &modifiers) {
	if (modifiers.size() != 1 || modifiers[0]->GetExpressionClass() != ExpressionClass::CONSTANT) {
		throw ParserException("TIME takes a single constant precision");
	}
	int64_t precision;
	if (!modifiers[0]->Cast<ConstantExpression>().GetLiteral().TryGetInt64(precision) || precision < 0 ||
	    precision > 9) {
		throw ParserException("TIME precision must be between 0 and 9");
	}
	return static_cast<uint8_t>(precision);
}

// DuckDB's TIME takes no precision; Snowflake's TIME(p) does
static arena_ptr<TransformResultValue> FinalizeTimeType(PEGTransformer &transformer,
                                                        GeneratedTransformProcess &process) {
	bool is_time = process.GetResult<LogicalTypeId>(SLOT_TIME_OR_TIMESTAMP) == LogicalTypeId::TIME;
	if (!is_time || !process.child_results[SLOT_TYPE_MODIFIERS] || process.child_results[SLOT_TIME_ZONE]) {
		return PEGTransformerFactory::FinalizeTimeTypeTrampoline(transformer, process);
	}
	auto modifiers = process.TakeResult<vector<unique_ptr<ParsedExpression>>>(SLOT_TYPE_MODIFIERS);
	auto type = GetTimePrecision(modifiers) <= 6 ? LogicalType::TIME : LogicalType::TIME_NS;
	unique_ptr<ParsedExpression> result =
	    make_uniq<TypeExpression>(Identifier(EnumUtil::ToString(type)), vector<unique_ptr<ParsedExpression>> {});
	return transformer.MakeResult<unique_ptr<ParsedExpression>>(std::move(result));
}

// WITH LOCAL TIME ZONE means the same as WITH TIME ZONE here: both map to TIMESTAMPTZ
static arena_ptr<TransformResultValue> FinalizeWithLocal(PEGTransformer &transformer, ParseResult &parse_result) {
	return transformer.MakeResult<bool>(true);
}

static arena_ptr<TransformProcess> StartWithLocalTransform(PEGTransformer &transformer, ParseResult &parse_result) {
	return transformer.MakeProcess<FinalizeTransformProcess>(transformer, parse_result, FinalizeWithLocal);
}

static const TransformFrameOps QUALIFIED_SIMPLE_TYPE_OPS = {
    "QualifiedSimpleType", &PEGTransformerFactory::InitializeQualifiedSimpleTypeTrampoline,
    &FinalizeQualifiedSimpleType};
static const TransformFrameOps TIME_TYPE_OPS = {"TimeType", &PEGTransformerFactory::InitializeTimeTypeTrampoline,
                                                &FinalizeTimeType};

static grammar_transform_process_function_t StartGenerated(const TransformFrameOps &ops) {
	return [&ops](PEGTransformer &transformer, ParseResult &parse_result) -> arena_ptr<TransformProcess> {
		return transformer.MakeProcess<GeneratedTransformProcess>(transformer, TransformInput {parse_result}, ops);
	};
}

vector<GrammarChange> TimestampTypeGrammarChanges() {
	vector<GrammarChange> changes;
	changes.push_back(
	    GrammarChange::SetTransformProcess("QualifiedSimpleType", StartGenerated(QUALIFIED_SIMPLE_TYPE_OPS)));
	changes.push_back(GrammarChange::SetTransformProcess("TimeType", StartGenerated(TIME_TYPE_OPS)));
	changes.push_back(GrammarChange::AddRule("WithLocalRule <- 'WITH' 'LOCAL'", StartWithLocalTransform));
	changes.push_back(GrammarChange::PrependChoice("WithOrWithout", "WithLocalRule"));
	return changes;
}

} // namespace duckdb
