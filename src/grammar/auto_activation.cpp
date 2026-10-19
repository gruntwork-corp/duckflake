// Turns the snowflake grammar on for every connection, so users don't need
// SET active_grammar_extensions = ['snowflake'];
//   - connections opened after LOAD: when they open
//   - connections already open: when the query running LOAD ends, or when an idle connection's next query begins
// Each connection is changed on its own thread, never from another connection's.
// Opt out with SET GLOBAL duckflake_auto_enable_grammar = false (before new connections open), or turn it off on one
// connection with RESET active_grammar_extensions. Other active grammar extensions are kept.

#include "duckdb/common/string_util.hpp"
#include "duckdb/main/client_config.hpp"
#include "duckdb/main/client_context.hpp"
#include "duckdb/main/client_context_state.hpp"
#include "duckdb/main/config.hpp"
#include "duckdb/main/connection_manager.hpp"
#include "duckdb/main/settings.hpp"
#include "duckdb/planner/extension_callback.hpp"
#include "grammar/snowflake_grammar.hpp"

namespace duckdb {

static constexpr const char *AUTO_ENABLE_SETTING = "duckflake_auto_enable_grammar";
static constexpr const char *PENDING_STATE_KEY = "duckflake_pending_grammar_activation";

static bool AutoEnableIsOn(ClientContext &context) {
	Value value;
	if (context.TryGetCurrentSetting(Identifier(AUTO_ENABLE_SETTING), value) && !value.IsNull()) {
		return value.GetValue<bool>();
	}
	return true;
}

static void ActivateSnowflakeGrammar(ClientContext &context) {
	if (!AutoEnableIsOn(context)) {
		return;
	}
	vector<Value> names;
	for (auto &name : ClientConfig::GetConfig(context).active_grammar_extensions) {
		if (StringUtil::CIEquals(name, SnowflakeGrammarExtension::NAME)) {
			return;
		}
		names.emplace_back(name);
	}
	names.emplace_back(SnowflakeGrammarExtension::NAME);
	ActiveGrammarExtensionsSetting::SetLocal(context, Value::LIST(LogicalType::VARCHAR, std::move(names)));
}

//! Activates the grammar once on a connection that was already open when the extension loaded:
//!   - mid-query (the LOAD itself): when that query ends, so the next query is parsed with the grammar
//!   - idle: when its next query begins, before it executes, so a RESET or SET in that query still wins
//! The first event tells the two apart: an idle connection sees QueryBegin first.
class PendingGrammarActivation final : public ClientContextState {
public:
	void QueryBegin(ClientContext &context) override {
		Activate(context);
	}
	void QueryEnd(ClientContext &context) override {
		Activate(context);
	}

private:
	void Activate(ClientContext &context) {
		if (done) {
			return;
		}
		done = true;
		ActivateSnowflakeGrammar(context);
	}

private:
	bool done = false;
};

class GrammarActivationCallback final : public ExtensionCallback {
public:
	void OnConnectionOpened(ClientContext &context) override {
		ActivateSnowflakeGrammar(context);
	}
};

void RegisterGrammarAutoActivation(DatabaseInstance &db) {
	auto &config = DBConfig::GetConfig(db);
	config.AddExtensionOption(Identifier(AUTO_ENABLE_SETTING),
	                          "Turn the snowflake grammar on for every connection (duckflake)", LogicalType::BOOLEAN,
	                          Value::BOOLEAN(true), nullptr, SetScope::GLOBAL);
	ExtensionCallback::Register(config, make_shared_ptr<GrammarActivationCallback>());
	for (auto &context : ConnectionManager::Get(db).GetConnectionList()) {
		context->registered_state->GetOrCreate<PendingGrammarActivation>(PENDING_STATE_KEY);
	}
}

} // namespace duckdb
