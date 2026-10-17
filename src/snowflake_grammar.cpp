#include "snowflake_grammar.hpp"

#include "duckdb/common/string_util.hpp"
#include "duckdb/parser/expression/conjunction_expression.hpp"
#include "duckdb/parser/expression/constant_expression.hpp"
#include "duckdb/parser/expression/function_expression.hpp"
#include "duckdb/parser/expression/operator_expression.hpp"
#include "duckdb/parser/peg/ast/method_arguments.hpp"
#include "duckdb/parser/peg/transformer/peg_transformer.hpp"

namespace duckdb {

// Child slots filled by PEGTransformerFactory::InitializeFunctionExpressionTrampoline for
// FunctionExpression <- FunctionIdentifier FunctionExpressionArguments WithinGroupClause? FilterClause? ...
static constexpr idx_t SLOT_NAME = 0;
static constexpr idx_t SLOT_ARGUMENTS = 1;
static constexpr idx_t SLOT_WITHIN_GROUP = 2;
static constexpr idx_t SLOT_FILTER = 3;

enum class SnowflakeAggregate : uint8_t { NONE, LISTAGG, ARRAY_AGG };

static SnowflakeAggregate ClassifyFunction(const QualifiedName &name) {
	if (name.Path().size() != 1) {
		return SnowflakeAggregate::NONE;
	}
	auto &function_name = name.Name().GetIdentifierName();
	if (StringUtil::CIEquals(function_name, "listagg")) {
		return SnowflakeAggregate::LISTAGG;
	}
	if (StringUtil::CIEquals(function_name, "array_agg") || StringUtil::CIEquals(function_name, "arrayagg")) {
		return SnowflakeAggregate::ARRAY_AGG;
	}
	return SnowflakeAggregate::NONE;
}

static unique_ptr<ParsedExpression> EmptyStringLiteral() {
	return make_uniq<ConstantExpression>(Literal::String(""));
}

static unique_ptr<ParsedExpression> EmptyList() {
	return make_uniq<FunctionExpression>(Identifier("list_value"), vector<FunctionArgument>());
}

// WITHIN GROUP (ORDER BY ...) is the aggregate's own ORDER BY
static void MoveWithinGroupToOrderBy(GeneratedTransformProcess &process, MethodArguments &arguments) {
	if (!process.child_results[SLOT_WITHIN_GROUP]) {
		return;
	}
	if (!arguments.order_bys.empty()) {
		throw ParserException("Cannot combine ORDER BY inside the function arguments with WITHIN GROUP");
	}
	arguments.order_bys = process.TakeResult<vector<OrderByNode>>(SLOT_WITHIN_GROUP);
}

// Snowflake's ARRAY_AGG omits NULLs; DuckDB's keeps them
static void FilterOutNulls(PEGTransformer &transformer, GeneratedTransformProcess &process,
                           const MethodArguments &arguments) {
	if (arguments.arguments.size() != 1) {
		return;
	}
	unique_ptr<ParsedExpression> filter = make_uniq<OperatorExpression>(ExpressionType::OPERATOR_IS_NOT_NULL,
	                                                                    arguments.arguments[0].GetExpression().Copy());
	if (process.child_results[SLOT_FILTER]) {
		filter = make_uniq<ConjunctionExpression>(ExpressionType::CONJUNCTION_AND,
		                                          process.TakeResult<unique_ptr<ParsedExpression>>(SLOT_FILTER),
		                                          std::move(filter));
	}
	process.SetChildResult(SLOT_FILTER, transformer.MakeResult<unique_ptr<ParsedExpression>>(std::move(filter)));
}

// Snowflake returns '' / [] rather than NULL for empty input
static unique_ptr<ParsedExpression> CoalesceTo(unique_ptr<ParsedExpression> expr,
                                               unique_ptr<ParsedExpression> fallback) {
	auto alias = expr->ToString();
	auto coalesce =
	    make_uniq<OperatorExpression>(ExpressionType::OPERATOR_COALESCE, std::move(expr), std::move(fallback));
	coalesce->SetAlias(Identifier(std::move(alias)));
	return std::move(coalesce);
}

static bool IsExportState(ParsedExpression &expr) {
	return expr.GetExpressionClass() == ExpressionClass::FUNCTION &&
	       expr.Cast<FunctionExpression>().ExportStateMutable();
}

// Rewrites the Snowflake aggregates in the transformed children, then defers to DuckDB's own finalize
static arena_ptr<TransformResultValue> FinalizeFunctionExpression(PEGTransformer &transformer,
                                                                  GeneratedTransformProcess &process) {
	auto &name = process.GetResult<QualifiedName>(SLOT_NAME);
	auto kind = ClassifyFunction(name);
	if (kind == SnowflakeAggregate::NONE) {
		return PEGTransformerFactory::FinalizeFunctionExpressionTrampoline(transformer, process);
	}

	auto &arguments = process.GetResult<MethodArguments>(SLOT_ARGUMENTS);
	MoveWithinGroupToOrderBy(process, arguments);
	if (kind == SnowflakeAggregate::LISTAGG) {
		// Snowflake's default delimiter is the empty string; DuckDB's is ','
		if (arguments.arguments.size() == 1) {
			arguments.arguments.emplace_back(EmptyStringLiteral());
		}
	} else {
		name = name.WithName(Identifier("array_agg"));
		FilterOutNulls(transformer, process, arguments);
	}

	auto result = PEGTransformerFactory::FinalizeFunctionExpressionTrampoline(transformer, process);
	auto &expr = *TryGetTransformResult<unique_ptr<ParsedExpression>>(*result);
	if (!IsExportState(*expr)) {
		expr = CoalesceTo(std::move(expr), kind == SnowflakeAggregate::LISTAGG ? EmptyStringLiteral() : EmptyList());
	}
	return result;
}

static const TransformFrameOps FUNCTION_EXPRESSION_OPS = {
    "FunctionExpression", &PEGTransformerFactory::InitializeFunctionExpressionTrampoline, &FinalizeFunctionExpression};

static arena_ptr<TransformProcess> StartFunctionExpressionTransform(PEGTransformer &transformer,
                                                                    ParseResult &parse_result) {
	return transformer.MakeProcess<GeneratedTransformProcess>(transformer, TransformInput {parse_result},
	                                                          FUNCTION_EXPRESSION_OPS);
}

SnowflakeGrammarExtension::SnowflakeGrammarExtension()
    : GrammarExtension(NAME, "Snowflake syntax: LISTAGG / ARRAY_AGG ... WITHIN GROUP (ORDER BY ...)") {
}

vector<GrammarChange> SnowflakeGrammarExtension::GetChanges() const {
	vector<GrammarChange> changes;
	changes.push_back(GrammarChange::SetTransformProcess("FunctionExpression", StartFunctionExpressionTransform));
	return changes;
}

} // namespace duckdb
