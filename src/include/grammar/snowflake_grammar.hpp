#pragma once

#include "duckdb/parser/grammar_extension.hpp"

namespace duckdb {

//! Snowflake syntax that DuckDB's parser rejects. Turned on for every connection automatically; manage it per
//! connection with SET active_grammar_extensions = ['snowflake'] / RESET active_grammar_extensions.
class SnowflakeGrammarExtension final : public GrammarExtension {
public:
	static constexpr const char *NAME = "snowflake";

	SnowflakeGrammarExtension();

	vector<GrammarChange> GetChanges() const override;
};

//! Turns the snowflake grammar on for every connection (src/grammar/auto_activation.cpp)
void RegisterGrammarAutoActivation(DatabaseInstance &db);

//! Snowflake time and timestamp type spellings (src/grammar/timestamp_types.cpp)
vector<GrammarChange> TimestampTypeGrammarChanges();

} // namespace duckdb
