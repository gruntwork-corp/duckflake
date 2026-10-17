# duckflake

A DuckDB 2.0 extension that lets DuckDB run Snowflake SQL unchanged.

Currently supported:

```sql
LOAD duckflake;
SET active_grammar_extensions = ['snowflake'];

SELECT LISTAGG(DISTINCT x, ', ') WITHIN GROUP (ORDER BY x) FROM t;
SELECT ARRAY_AGG(x) WITHIN GROUP (ORDER BY y) FROM t;
```

When active, `LISTAGG` and `ARRAY_AGG` / `ARRAYAGG` follow Snowflake's behaviour:

- `''` is the default delimiter;
- `ARRAY_AGG` omits NULLs;
- empty input returns `''` / `[]` instead of `NULL`.

See [docs/design/0001-listagg-within-group.md](docs/design/0001-listagg-within-group.md) for the design and its
trade-offs.

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
