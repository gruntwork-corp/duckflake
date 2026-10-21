#!/usr/bin/env bash
# Builds duckflake against one DuckDB release and runs the test suite. Run through docker-compose.test.yml, which
# mounts per-version volumes over duckdb/ and extension-ci-tools/, so the host's submodule checkouts are untouched.
set -euo pipefail

: "${DUCKDB_VERSION:?set DUCKDB_VERSION, e.g. v2.0.0}"
: "${CI_TOOLS_VERSION:?set CI_TOOLS_VERSION, e.g. v2.0-cyanoptera}"

# Clone a ref (tag or branch) into a directory once; an existing checkout must already be on that ref
checkout() {
	local repo=$1 ref=$2 dir=$3
	if [ -d "${dir}/.git" ]; then
		local current
		current=$(cat "${dir}/.duckflake-ref" 2>/dev/null || true)
		if [ "${current}" != "${ref}" ]; then
			echo "${dir} holds ${current:-an unknown ref}, not ${ref}; remove its volume to switch" >&2
			exit 1
		fi
		return
	fi
	find "${dir}" -mindepth 1 -delete
	git clone --quiet --depth 1 --branch "${ref}" "https://github.com/duckdb/${repo}" "${dir}"
	echo "${ref}" >"${dir}/.duckflake-ref"
}

checkout duckdb "${DUCKDB_VERSION}" /workspace/duckdb
checkout extension-ci-tools "${CI_TOOLS_VERSION}" /workspace/extension-ci-tools

echo "Building duckflake against DuckDB ${DUCKDB_VERSION} (extension-ci-tools ${CI_TOOLS_VERSION})"
make reldebug
./build/reldebug/test/unittest "test/sql/*"
