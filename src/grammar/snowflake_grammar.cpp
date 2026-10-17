#include "grammar/snowflake_grammar.hpp"

#include "duckdb/common/string_util.hpp"
#include "duckdb/parser/expression/function_expression.hpp"
#include "duckdb/parser/expression/operator_expression.hpp"
#include "grammar/function_call_rewrite.hpp"

namespace duckdb {

const vector<FunctionCallRewrite> &GetFunctionCallRewrites() {
	static const vector<FunctionCallRewrite> REWRITES = {
	    ListaggRewrite(),
	    ArrayAggRewrite(),
	};
	return REWRITES;
}

unique_ptr<ParsedExpression> CoalesceTo(unique_ptr<ParsedExpression> expr, unique_ptr<ParsedExpression> fallback) {
	auto alias = expr->ToString();
	auto coalesce =
	    make_uniq<OperatorExpression>(ExpressionType::OPERATOR_COALESCE, std::move(expr), std::move(fallback));
	coalesce->SetAlias(Identifier(std::move(alias)));
	return std::move(coalesce);
}

static optional_ptr<const FunctionCallRewrite> FindRewrite(const QualifiedName &name) {
	if (name.Path().size() != 1) {
		return nullptr;
	}
	auto &function_name = name.Name().GetIdentifierName();
	for (auto &rewrite : GetFunctionCallRewrites()) {
		for (auto &rewrite_name : rewrite.names) {
			if (StringUtil::CIEquals(function_name, rewrite_name)) {
				return &rewrite;
			}
		}
	}
	return nullptr;
}

static bool IsExportState(ParsedExpression &expr) {
	return expr.GetExpressionClass() == ExpressionClass::FUNCTION &&
	       expr.Cast<FunctionExpression>().ExportStateMutable();
}

// Applies the matching rewrite to the transformed children, then defers to DuckDB's own finalize
static arena_ptr<TransformResultValue> FinalizeFunctionExpression(PEGTransformer &transformer,
                                                                  GeneratedTransformProcess &process) {
	FunctionCall call(transformer, process);
	auto rewrite = FindRewrite(call.Name());
	if (!rewrite) {
		return PEGTransformerFactory::FinalizeFunctionExpressionTrampoline(transformer, process);
	}

	rewrite->rewrite_call(call);
	auto result = PEGTransformerFactory::FinalizeFunctionExpressionTrampoline(transformer, process);
	auto &expr = *TryGetTransformResult<unique_ptr<ParsedExpression>>(*result);
	if (rewrite->rewrite_result && !IsExportState(*expr)) {
		expr = rewrite->rewrite_result(std::move(expr));
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
