FROM debian:trixie-slim

# Matches the vcpkg commit used by duckdb/extension-ci-tools (Release 2026.06.24)
ARG VCPKG_COMMIT=cd61e1e26a038e82d6550a3ebbe0fbbfe7da78e3
# DuckDB CLI release, for poking at built extensions
ARG DUCKDB_VERSION=2.0.0

RUN apt-get update && apt-get install -y --no-install-recommends \
    autoconf \
    automake \
    build-essential \
    ca-certificates \
    ccache \
    clang-format \
    clang-tidy \
    clangd \
    cmake \
    curl \
    gdb \
    git \
    jq \
    libtool \
    ninja-build \
    perl \
    pkg-config \
    python3 \
    python3-pip \
    python3-venv \
    tar \
    unzip \
    zip \
    && rm -rf /var/lib/apt/lists/*

# vcpkg (extension dependencies). System cmake/ninja are required on arm64 Linux.
ENV VCPKG_ROOT=/opt/vcpkg \
    VCPKG_FORCE_SYSTEM_BINARIES=1
RUN git init ${VCPKG_ROOT} \
    && git -C ${VCPKG_ROOT} remote add origin https://github.com/microsoft/vcpkg.git \
    && git -C ${VCPKG_ROOT} fetch --depth 1 origin ${VCPKG_COMMIT} \
    && git -C ${VCPKG_ROOT} checkout FETCH_HEAD \
    && ${VCPKG_ROOT}/bootstrap-vcpkg.sh -disableMetrics
ENV VCPKG_TOOLCHAIN_PATH=${VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake \
    PATH="${VCPKG_ROOT}:${PATH}"

# DuckDB CLI for poking at built extensions
RUN curl -fsSL https://install.duckdb.org | DUCKDB_VERSION=${DUCKDB_VERSION} bash
ENV PATH="/root/.duckdb/cli/latest:${PATH}"

# Same build conventions as the extension template / CI
ENV GEN=ninja \
    CCACHE_DIR=/ccache \
    CCACHE_COMPRESS=true \
    CCACHE_MAXSIZE=5G \
    CMAKE_C_COMPILER_LAUNCHER=ccache \
    CMAKE_CXX_COMPILER_LAUNCHER=ccache

RUN git config --global --add safe.directory '*'

WORKDIR /workspace
