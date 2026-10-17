# 0001: Snowflake `LISTAGG` / `ARRAY_AGG ... WITHIN GROUP` on DuckDB 2.0

- **Status:** accepted
- **Date:** 2026-10-17
- **DuckDB target:** `v2.0-cyanoptera` @ `f8f3f4dcd9` (`v2.0.0-alpha48730`)

## Summary

duckflake is a C++ DuckDB 2.0 extension. Its purpose is to make DuckDB's parser accept Snowflake SQL, so Snowflake
queries run in DuckDB without changes. The first target is:

```sql
LISTAGG(DISTINCT x [, delim]) WITHIN GROUP (ORDER BY x)
```

DuckDB 2.0 rejects this form in its parser, before any function lookup happens. The only hook at that point is the
C++ `GrammarExtension` API, so the extension is written in C++. DuckDB already returns the right result for the
native form `listagg(DISTINCT x, ',' ORDER BY x)`, so duckflake only has to translate syntax. It does not need a new
aggregate. We will also propose the fix to DuckDB upstream; if it is merged, this part of duckflake can be removed.

## Goal and scope

**Requirement:** the Snowflake syntax must parse unchanged and return Snowflake's results. Rewriting queries by hand
does not meet the goal.

```sql
LISTAGG( [DISTINCT] <expr> [, <delimiter>] ) [WITHIN GROUP (ORDER BY ...)] [OVER (PARTITION BY ...)]
```

Snowflake rules we must match ([LISTAGG docs](https://docs.snowflake.com/en/sql-reference/functions/listagg)):

| Rule | Snowflake | DuckDB 2.0 today |
| --- | --- | --- |
| `DISTINCT` + `WITHIN GROUP` | Must use the same column; different columns raise an error | The binder enforces the same rule |
| Default delimiter | `''` (empty string) | `','`. duckflake must pass `''` itself |
| NULL inputs | Skipped; empty or all-NULL input returns `''` | Skipped; empty or all-NULL input returns `NULL` |
| Input type | A string, or anything castable to a string | Nested types are cast to `VARCHAR` |
| Window form | `OVER (PARTITION BY ...)` only; no `ORDER BY` inside `OVER`, no frames | Already parses (see below) |

**Nested inputs:** `LISTAGG` must accept semi-structured input, such as objects built with
[`OBJECT_AGG`](https://docs.snowflake.com/en/sql-reference/functions/object_agg), not just simple types.

**Out of scope for this step:** implementing `OBJECT_AGG` itself and other Snowflake functions. These come later in
the same extension. Notes for that work:

- DuckDB has no `object_agg`.
- The closest DuckDB function, `json_group_object`, keeps duplicate keys. Snowflake raises
  `Duplicate field key` instead.

## What we found

All results below come from the `v2.0.0-alpha48730` CLI inside the devcontainer.

| Query | Result |
| --- | --- |
| `listagg(x, ',')` | Works; `listagg` is an alias of `string_agg` |
| `listagg(x, ',') WITHIN GROUP (ORDER BY y)` | `Parser Error: Unknown ordered aggregate "listagg"` |
| `listagg(DISTINCT x, ',') WITHIN GROUP (ORDER BY y)` | `Parser Error: DISTINCT is not allowed in combination with WITHIN GROUP` |
| `string_agg(DISTINCT x, ',' ORDER BY y)` | `Binder Error: In a DISTINCT aggregate, ORDER BY expressions must appear in the argument list` |
| `string_agg(DISTINCT x, ',' ORDER BY x)` | Works |
| `listagg(x, ',') WITHIN GROUP (ORDER BY y) OVER (PARTITION BY g)` | Works; rewritten to `listagg(x, ',' ORDER BY y)` |
| `listagg({'k': k, 'v': v}, ',')` and the same with `MAP` or `json_object` inputs | Works; each value is cast to `VARCHAR` first |

**Where it fails:** both parser errors come from `PEGTransformerFactory::TransformFunctionExpression` in
`duckdb/src/parser/peg/transformer/transform_expression.cpp`.

- `DISTINCT` combined with `WITHIN GROUP` is rejected unconditionally.
- Outside a window, `WITHIN GROUP` is accepted only for `percentile_cont`, `percentile_disc` and `mode`.
- In a window (`OVER` clause), the same `WITHIN GROUP` is turned into the aggregate's `ORDER BY`. The window and
  non-window paths are inconsistent, which supports an upstream fix.

**Semantics already match:** Snowflake requires `DISTINCT` and `WITHIN GROUP` to use the same column, and DuckDB's
binder has the same rule. DuckDB therefore needs no new aggregate; the gap is purely syntax.

**Nested-type gap:** the value is cast to `VARCHAR` with DuckDB's text format, for example `{'k': k1, 'v': 10}`.
Snowflake prints objects as JSON, for example `{"k":"k1","v":10}`. Values from `json_object` already print the way
Snowflake does.

## Options considered

| Option | API surface | Pros | Cons |
| --- | --- | --- | --- |
| A. Users write the native DuckDB form | none | Works today | Not the same syntax; fails the goal |
| B. Upstream fix in DuckDB's transformer | none (core change) | Most stable long-term; no extension needed | 2.0 is feature-frozen, so it lands in 2.1 at the earliest |
| C. Rewrite SQL before it reaches DuckDB (e.g. sqlglot 30.23.0) | fully public | No DuckDB internals involved | Only works if every query passes through our code; sqlglot emits `','` as the default delimiter instead of Snowflake's `''` |
| D. C API v2 extension (`duckdb_v2.h`) | stable C ABI | Survives DuckDB upgrades | Has no grammar or dialect hooks, so it cannot change the syntax |
| E. Rust (`extension-template-rs`) | C API | Memory safety | Same limit as D; the template also needs `USE_UNSTABLE_C_API=1`, so it gets no stable ABI anyway |
| F. **C++ `GrammarExtension`** | internal C++ | Keeps the exact syntax; works on 2.0 now | Tied to one DuckDB build; the API is a preview |

## Decision

**We go with F: a C++ `GrammarExtension`. We also file B upstream.**

- Same syntax is a hard requirement. That rules out A and C.
- The syntax check happens in the parser's transformer, before function lookup. No function, from any API, can
  change that, which rules out D, E and Java-based functions.
- DuckDB 2.0 offers grammar changes only through C++ (`GrammarExtension`, `DialectExtension`).
- An extension binary is either C++ or C-API. Future duckflake work is mostly parser-level (Snowflake syntax), so
  the whole extension is C++. Plain functions (`OBJECT_AGG`, `IFF`, ...) are just as easy to write in C++.
- If upstream accepts the transformer fix, we delete the `LISTAGG` grammar rule. The tests stay, as a regression
  check.

## Implementation approach

The implementation needs no new grammar rule. DuckDB's grammar already parses
`FunctionExpression <- FunctionIdentifier FunctionExpressionArguments WithinGroupClause? FilterClause? ExportClause? OverClause?`,
so `DISTINCT` and `WITHIN GROUP` can already be combined. Only the transformer rejects them. duckflake therefore
replaces the transformer of that single rule and leaves the grammar unchanged.

DuckDB's own `duckdb/test/extension/loadable_grammar_extension_demo.cpp` uses the same API. The API differs from the
[PEG parser blog post](https://duckdb.org/2026/08/20/duckdb-20-peg-parser): grammar changes are now a separate
`GrammarExtension` class, not a field on `ParserExtension`.

Code lives in `src/grammar/`. `snowflake_grammar.cpp` registers the extension and dispatches by function name.
`function_call.cpp` holds DuckDB's slot layout. `listagg.cpp` and `array_agg.cpp` hold the rewrites:

1. `SnowflakeGrammarExtension` (name `snowflake`) returns a single change:
   `GrammarChange::SetTransformProcess("FunctionExpression", ...)`.
2. The transform process reuses DuckDB's generated
   `PEGTransformerFactory::InitializeFunctionExpressionTrampoline`, which transforms the child rules on DuckDB's
   explicit transform stack. Our finalize step edits a few child results, then calls DuckDB's generated
   `FinalizeFunctionExpressionTrampoline`. We touch only these slots: 0 (name), 1 (arguments), 2 (`WITHIN GROUP`)
   and 3 (`FILTER`). Clauses DuckDB adds later pass through untouched.
3. Rewrites, applied only to unqualified `listagg`, `array_agg` and `arrayagg`:

   | Change | `LISTAGG` | `ARRAY_AGG` / `ARRAYAGG` |
   | --- | --- | --- |
   | `WITHIN GROUP (ORDER BY ...)` | becomes the aggregate's `ORDER BY` | same |
   | Default delimiter | `''` | n/a |
   | NULL inputs | already skipped by DuckDB | `FILTER (WHERE x IS NOT NULL)`, combined with any user `FILTER` |
   | Empty result | `COALESCE(..., '')` | `COALESCE(..., [])` |
   | Name | unchanged | `arrayagg` renamed to `array_agg` |

   The `COALESCE` wrapper is aliased to the inner expression, so the column name stays `listagg(...)`.
   `EXPORT_STATE` calls are not wrapped. Every other function goes through DuckDB's code unchanged.
4. The binder then applies DuckDB's existing rule that `DISTINCT` and `ORDER BY` must use the same column, which is
   Snowflake's rule too.
5. Activation is per connection: `SET active_grammar_extensions = ['snowflake'];`. The default is off.
6. Tests in `test/sql/grammar/listagg.test` and `test/sql/grammar/array_agg.test` cover:
    - the syntax is rejected before activation and after `RESET`;
    - `DISTINCT` on the same column, and on different columns (error);
    - defaults, and empty, NULL and all-NULL groups;
    - `FILTER`;
    - the window form;
    - nested inputs;
    - unrelated functions (`percentile_cont`, `string_agg`, `list`) are unaffected.

### API exposure

We are **not** using only public APIs.

| Symbol | Status |
| --- | --- |
| `GrammarExtension`, `GrammarChange::SetTransformProcess`, `GrammarExtension::Register` | `DUCKDB_API`; intended for extensions; labelled a preview |
| `GeneratedTransformProcess`, `TransformFrameOps`, `TryGetTransformResult`, `MethodArguments` | internal transformer headers, not `DUCKDB_API` |
| `PEGTransformerFactory::Initialize/FinalizeFunctionExpressionTrampoline` | generated from the grammar, not `DUCKDB_API` |
| The slot numbers 0–3 | implied by the generated grammar layout |

If DuckDB reorders the `FunctionExpression` rule or renames the generated functions, our code breaks. It would
break either at compile time or, for a slot change, in the tests. Delegating to DuckDB's own finalize keeps that
exposure to four slots. The supported long-term route is the upstream fix (option B).

## Development environment

- **Devcontainer** (`.devcontainer/`, layout modelled on `fusion-unit-testing`):
    - base image `debian:trixie-slim`;
    - compilers and build tools: gcc 14, cmake 3.31, ninja, ccache;
    - linting and debugging: clang-format, clang-tidy, clangd, gdb;
    - Python 3.13;
    - vcpkg, pinned to the same commit as DuckDB's extension CI (`cd61e1e`);
    - the DuckDB 2.0 alpha CLI.
- **Docker volumes:** `build/`, the ccache cache and the vcpkg cache live in volumes, not on the macOS bind mount,
  so builds are faster and survive container rebuilds.
- **Submodules:**
    - `duckdb` on `v2.0-cyanoptera`;
    - `extension-ci-tools` on `main`.
- **CI:** `MainDistributionPipeline.yml` points at `v2.0-cyanoptera` and `extension-ci-tools@main`.
- **Template cleanup:** the template's OpenSSL demo dependency is removed. The only function is a
  `duckflake_version()` smoke test.

**Build modes:**

- A full build compiles all of DuckDB, because a C++ extension must match the exact DuckDB build that loads it.
  - The first build takes long, because every file is compiled from an empty cache.
  - Later builds only recompile duckflake's own files and relink.
- `make reldebug` (optimized, with debug symbols) is the everyday loop. Verified: it builds, and
  `build/reldebug/test/unittest "test/sql/*"` passes.
  - The `build/reldebug/duckdb` shell does not include the extension, despite the template README. Load it by path:
    `build/reldebug/duckdb -unsigned -c "LOAD 'build/reldebug/extension/duckflake/duckflake.duckdb_extension'"`.
- `make debug` (assertions and sanitizers) currently **fails to link on arm64**:
  `relocation truncated to fit: R_AARCH64_CALL26`. The debug + sanitizer `libduckdb.so` is too large for a direct
  branch on arm64 (±128 MB). Fixes to try: link with `lld`, or skip building the shared library in debug.
- Building only the extension (`BUILD_EXTENSIONS_ONLY`) and loading it into the pre-built CLI is possible but fragile:
  - the commit must match the CLI exactly;
  - the version string differs: our build reports `v2.0.0-dev88537`, the CLI `v2.0.0-alpha48730`. Confirmed: the CLI
    refuses the extension with `The file was built specifically for DuckDB version 'f8f3f4dcd9'`, even though the
    commit is the same;
  - there are no assertions;
  - there is no `unittest` runner.

## Risks and open questions

- **API churn:** the grammar extension API is labelled a preview that "may change before" 2.0 ships. Expect to fix
  the code when the submodule is bumped.
- **ABI coupling:** every DuckDB release needs a rebuild. That is normal for C++ extensions.
- **Activation default:** today users opt in with `SET active_grammar_extensions = ['snowflake']`. The setting is a
  list per connection, so it replaces any other grammar extensions already set. Should loading duckflake turn the
  grammar on automatically?
- **Nested types:** objects as `LISTAGG` input should print as JSON, Snowflake's text form, rather than DuckDB's
  `STRUCT` text form. Doing that needs a cast step in our transform.
- **Plain `listagg` changes too:** while the grammar is active, `listagg(x)` uses `''` as its default delimiter and
  returns `''` instead of `NULL`. This matches Snowflake, but differs from stock DuckDB.
- **`ARRAY_AGG` column names:** the alias shows the injected filter, for example
  `array_agg(x ORDER BY y) FILTER (WHERE (x IS NOT NULL))`. Users who need a stable name should alias the column.

## Sources

- [Try DuckDB v2.0-dev](https://duckdb.org/2026/09/02/try-duckdb-20-alpha): branch name, alpha builds, `ref_next`
- [DuckDB 2.0 PEG parser](https://duckdb.org/2026/08/20/duckdb-20-peg-parser): grammar extensions
- [A Preview of DuckDB v2.0](https://duckdb.org/2026/08/17/duckdb-20-highlights)
- [DuckDB Java functions](https://duckdb.org/docs/current/clients/java/functions): scalar and table functions only
- [Snowflake LISTAGG](https://docs.snowflake.com/en/sql-reference/functions/listagg)
- [Snowflake OBJECT_AGG](https://docs.snowflake.com/en/sql-reference/functions/object_agg)
- DuckDB source at `f8f3f4dcd9`:
  - `src/parser/peg/transformer/transform_expression.cpp`
  - `src/include/duckdb/parser/grammar_extension.hpp`
  - `src/include/duckdb/parser/grammar_change.hpp`
  - `src/include/duckdb_v2.h`
  - `test/extension/loadable_grammar_extension_demo.cpp`
