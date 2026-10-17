#pragma once

#include "duckdb/parser/grammar_extension.hpp"

namespace duckdb {

//! Snowflake syntax that DuckDB's parser rejects. Enable per connection with
//! SET active_grammar_extensions = ['snowflake'];
class SnowflakeGrammarExtension final : public GrammarExtension {
public:
	static constexpr const char *NAME = "snowflake";

	SnowflakeGrammarExtension();

	vector<GrammarChange> GetChanges() const override;
};

} // namespace duckdb
