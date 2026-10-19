# duckflake

A DuckDB 2.0 extension that lets DuckDB run Snowflake SQL unchanged.

Currently supported:

```sql
LOAD duckflake;
SET active_grammar_extensions = ['snowflake'];

SELECT LISTAGG(DISTINCT x, ', ') WITHIN GROUP (ORDER BY x) FROM t;
SELECT ARRAY_AGG(x) WITHIN GROUP (ORDER BY y) FROM t;
SELECT DATEADD(day, 1, d) FROM t;                         -- bare date part needs the SET; DATEADD('day', 1, d) works without
```

Functions, available as soon as the extension is loaded:

| Function | Notes |
| --- | --- |
| `IFF(cond, a, b)` | NULL condition picks `b`; only the chosen branch is evaluated |
| `TO_ARRAY(x)` | lists unchanged, NULL stays NULL, anything else becomes `[x]` |
| `DATEADD` / `TIMEADD` / `TIMESTAMPADD(part, n, x)` | Snowflake part names; `DATE` stays `DATE` for day-or-larger parts |
| `BOOLOR_AGG(x)` | numbers are TRUE when non-zero; works with `OVER` |
| `MONTHS_BETWEEN(d1, d2)` | Snowflake's 31-day fraction and last-day-of-month rules |
| `DAYOFWEEKISO(d)` | 1 = Monday ... 7 = Sunday |

Types, available as soon as the extension is loaded: `NUMBER` is `DECIMAL(38,0)`, `NUMBER(p)` is `DECIMAL(p,0)` and
`NUMBER(p,s)` is `DECIMAL(p,s)`. DuckDB's own `DECIMAL` / `NUMERIC` keep their `(18,3)` default.

When active, `LISTAGG` and `ARRAY_AGG` / `ARRAYAGG` follow Snowflake's behaviour:

- `''` is the default delimiter;
- `ARRAY_AGG` omits NULLs;
- empty input returns `''` / `[]` instead of `NULL`.

See [docs/design/0001-listagg-within-group.md](docs/design/0001-listagg-within-group.md) for the design and its
trade-offs.

## Layout

| Path | What goes there |
| --- | --- |
| `src/functions/` | Plain Snowflake functions (`IFF`, ...): one file per function; always available after `LOAD` |
| `src/types/` | Snowflake type names (`NUMBER`, ...): one file per type; always available after `LOAD` |
| `src/grammar/` | Syntax DuckDB rejects (`LISTAGG ... WITHIN GROUP`, ...): one file per rewrite; active after the `SET` |
| `test/sql/functions/`, `test/sql/types/`, `test/sql/grammar/` | One sqllogictest file per function or rewrite |

**Adding a function:**

1. Write `src/functions/<name>.cpp` with a `Register<Name>(ExtensionLoader &)` function. Prefer a macro (see
   `iff.cpp`) when the function can be written in SQL.
2. Declare it in `src/include/functions/functions.hpp`.
3. Call it from `RegisterFunctions()` in `src/functions/functions.cpp`.
4. Add `test/sql/functions/<name>.test`.

**Adding a syntax rewrite for a function call:**

1. Write `src/grammar/<name>.cpp` returning a `FunctionCallRewrite`. It names the functions it applies to, how to
   change the call (`FunctionCall`) and, optionally, how to wrap the result. See `listagg.cpp`.
2. Declare it in `src/include/grammar/function_call_rewrite.hpp`.
3. List it in `GetFunctionCallRewrites()` in `src/grammar/snowflake_grammar.cpp`.
4. Add `test/sql/grammar/<name>.test`.

`src/grammar/function_call.cpp` is the only file that knows DuckDB's internal parser layout. CMake picks up every
`.cpp` under `src/` automatically.

## Development

Open the repo in the devcontainer (`.devcontainer/`). It builds against the `duckdb` submodule on the
`v2.0-cyanoptera` branch.

```sh
git submodule update --init --recursive
make reldebug
./build/reldebug/test/unittest "test/sql/*"
./build/reldebug/duckdb -unsigned -c "LOAD 'build/reldebug/extension/duckflake/duckflake.duckdb_extension'"
```

- `make debug` currently fails to link on arm64 (`R_AARCH64_CALL26`), so use `reldebug` there.
- A C++ extension only loads into a DuckDB binary built from the exact same DuckDB build. The pre-built 2.0 alpha
  CLI rejects this one.
