#include "grammar/function_call.hpp"

#include "duckdb/parser/expression/conjunction_expression.hpp"

namespace duckdb {

// Child slots filled by PEGTransformerFactory::InitializeFunctionExpressionTrampoline for
// FunctionExpression <- FunctionIdentifier FunctionExpressionArguments WithinGroupClause? FilterClause? ...
static constexpr idx_t SLOT_NAME = 0;
static constexpr idx_t SLOT_ARGUMENTS = 1;
static constexpr idx_t SLOT_WITHIN_GROUP = 2;
static constexpr idx_t SLOT_FILTER = 3;

FunctionCall::FunctionCall(PEGTransformer &transformer_p, GeneratedTransformProcess &process_p)
    : transformer(transformer_p), process(process_p) {
}

const QualifiedName &FunctionCall::Name() {
	return process.GetResult<QualifiedName>(SLOT_NAME);
}

void FunctionCall::Rename(const string &name) {
	auto &qualified_name = process.GetResult<QualifiedName>(SLOT_NAME);
	qualified_name = qualified_name.WithName(Identifier(name));
}

MethodArguments &FunctionCall::Arguments() {
	return process.GetResult<MethodArguments>(SLOT_ARGUMENTS);
}

void FunctionCall::AddArgument(unique_ptr<ParsedExpression> argument) {
	Arguments().arguments.emplace_back(std::move(argument));
}

void FunctionCall::MoveWithinGroupToOrderBy() {
	if (!process.child_results[SLOT_WITHIN_GROUP]) {
		return;
	}
	auto &arguments = Arguments();
	if (!arguments.order_bys.empty()) {
		throw ParserException("Cannot combine ORDER BY inside the function arguments with WITHIN GROUP");
	}
	arguments.order_bys = process.TakeResult<vector<OrderByNode>>(SLOT_WITHIN_GROUP);
}

void FunctionCall::AddFilter(unique_ptr<ParsedExpression> condition) {
	if (process.child_results[SLOT_FILTER]) {
		condition = make_uniq<ConjunctionExpression>(ExpressionType::CONJUNCTION_AND,
		                                             process.TakeResult<unique_ptr<ParsedExpression>>(SLOT_FILTER),
		                                             std::move(condition));
	}
	process.SetChildResult(SLOT_FILTER, transformer.MakeResult<unique_ptr<ParsedExpression>>(std::move(condition)));
}

} // namespace duckdb
