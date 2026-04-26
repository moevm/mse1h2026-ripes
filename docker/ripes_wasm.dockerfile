# syntax=docker/dockerfile:1.6
# Multi-stage build of Ripes for WebAssembly (Qt for WASM) + nginx runtime.
#
# Stage 1 (builder):
#   - Reuses the existing host Qt image so we have a matching desktop Qt
#     (required by Qt for WASM as QT_HOST_PATH).
#   - Installs emsdk pinned to the version Qt 6.10 expects (3.1.70).
#   - Installs Qt for WASM (wasm_singlethread) + qtcharts + qtsvg via aqtinstall.
#   - Cross-compiles Ripes using qt-cmake, producing Ripes.{html,js,wasm}.
#
# Stage 2 (runtime):
#   - nginx:alpine serving the produced artifacts on port 80.

FROM ghcr.io/niksbrn/qt-ubuntu:6.10.2 AS builder

ARG DEBIAN_FRONTEND=noninteractive
ARG EMSDK_VERSION=4.0.7
ARG QT_VERSION=6.10.2
ARG BRANCH=master

# Build prerequisites for emsdk + aqtinstall + cloning sources.
RUN apt-get update -q \
    && apt-get install -qy --no-install-recommends \
       build-essential cmake git ca-certificates xz-utils \
       python3 python3-pip python3-venv \
       libglib2.0-0 \
    && rm -rf /var/lib/apt/lists/*

# Unpack the host Qt that ships inside the base image (same recipe as ripes_QT.dockerfile).
RUN mkdir -p /opt/qt/${QT_VERSION} \
    && tar -xzf /tmp/qt-${QT_VERSION}-gcc_64.tar.gz -C /opt/qt/${QT_VERSION} \
    && rm /tmp/qt-${QT_VERSION}-gcc_64.tar.gz

ENV QT_HOST_PATH=/opt/qt/${QT_VERSION}/gcc_64
ENV LD_LIBRARY_PATH=${QT_HOST_PATH}/lib

# Install emsdk pinned to the version Qt for WASM was built with.
# This MUST match the version reported by Qt's QtPublicWasmToolchainHelpers.cmake,
# otherwise qt-cmake aborts with "mismatch of Emscripten versions".
# Qt 6.10.x WASM is built with Emscripten 4.0.7.
RUN git clone https://github.com/emscripten-core/emsdk.git /opt/emsdk \
    && cd /opt/emsdk \
    && ./emsdk install ${EMSDK_VERSION} \
    && ./emsdk activate ${EMSDK_VERSION}

# Install aqtinstall inside an isolated venv to stay compatible with both
# old (no --break-system-packages) and new (PEP 668) pip versions.
# This is a separate layer so a failed Qt download does not force pip to
# reinstall on the next build.
RUN python3 -m venv /opt/aqt-venv \
    && /opt/aqt-venv/bin/pip install --no-cache-dir --upgrade pip \
    && /opt/aqt-venv/bin/pip install --no-cache-dir aqtinstall

# Install Qt for WebAssembly (single-threaded) module by module.
# Since Qt 6.7 the WASM artifacts live under host=all_os / target=wasm.
# Files land under /opt/qt/<version>/wasm_singlethread.
#
# Each module is its own RUN so Docker can cache the successful ones:
# if (for example) qtbase download times out, the next build will reuse
# the layers for qtsvg/qttools/... that already finished.
# --archives <name> restricts the download to that single archive, so the
# follow-up module installs do not re-download qtbase every time.

# 1) Base Qt for WASM (largest archive, most prone to timeouts).
RUN /opt/aqt-venv/bin/aqt install-qt all_os wasm ${QT_VERSION} wasm_singlethread \
        -O /opt/qt

# 2) Extra modules required by Ripes, one per layer.
RUN /opt/aqt-venv/bin/aqt install-qt all_os wasm ${QT_VERSION} wasm_singlethread \
        -m qtcharts --archives qtcharts \
        -O /opt/qt

# 3) Make the WASM Qt tools executable (qt-cmake, qmake, ...).
RUN chmod -R +x /opt/qt/${QT_VERSION}/wasm_singlethread/bin

ENV QT_WASM_PATH=/opt/qt/${QT_VERSION}/wasm_singlethread

# Clone Ripes and cross-compile it.
RUN git clone --recursive --branch ${BRANCH} \
        https://github.com/moevm/mse1h2026-ripes.git /tmp/ripes

# Threads shim for single-threaded WebAssembly.
# Emscripten 4.x's CMake FindThreads does not create the Threads::Threads
# imported target unless the project is compiled with -pthread, but Qt for
# WASM single-threaded must NOT use -pthread. Several CMakeLists in the
# Ripes tree (VSRTL, libelfin/dwarf++, ripes_lib, CLI_lib, the Ripes target
# itself) link against Threads::Threads unconditionally.
#
# On the browser there are no real threads anyway, so we provide an empty
# INTERFACE imported target via a wrapper toolchain that loads Qt's own
# emscripten toolchain first and then injects the shim. We cannot use
# CMAKE_PROJECT_TOP_LEVEL_INCLUDES because Ripes pins cmake_minimum_required
# to 3.13 (that variable was introduced in CMake 3.24 and is silently
# ignored on older policies).
RUN QT_TOOLCHAIN=$(find /opt/qt/${QT_VERSION}/wasm_singlethread/lib/cmake \
        -name 'qt.toolchain.cmake' | head -n1) \
    && printf '%s\n' \
        "include(\"${QT_TOOLCHAIN}\")" \
        '# Toolchain files are loaded before project()/enable_language(),' \
        '# so we cannot call find_package(Threads) here. Just declare the' \
        '# imported target unconditionally - on single-threaded WASM there' \
        '# is no real pthread runtime to link against anyway.' \
        'if(NOT TARGET Threads::Threads)' \
        '  add_library(Threads::Threads INTERFACE IMPORTED)' \
        'endif()' \
        '# The host Qt rcc is built with zstd support and emits calls to' \
        '# qResourceFeatureZstd() in the generated qrc_*.cpp files, but the' \
        '# WASM build of Qt6Core does not export that symbol -> wasm-ld fails' \
        '# with "undefined symbol: qResourceFeatureZstd()". Disable zstd' \
        '# compression in rcc globally so the generated code does not' \
        '# reference the missing symbol.' \
        'set(CMAKE_AUTORCC_OPTIONS "--no-zstd" CACHE STRING "" FORCE)' \
        > /opt/ripes-wasm-toolchain.cmake

# qt-cmake from the WASM kit auto-injects the emscripten toolchain file.
# We override CMAKE_TOOLCHAIN_FILE with our wrapper that re-includes Qt's
# toolchain and then defines the missing Threads::Threads target.
# QProcess and QTest are unavailable in the browser sandbox -> disabled.
# emsdk_env.sh uses $BASH_SOURCE to locate itself - must be sourced from its own directory.
RUN cd /opt/emsdk && . ./emsdk_env.sh \
    && ${QT_WASM_PATH}/bin/qt-cmake \
        -S /tmp/ripes \
        -B /tmp/ripes/build \
        -Wno-dev \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_TOOLCHAIN_FILE=/opt/ripes-wasm-toolchain.cmake \
        -DQT_HOST_PATH=${QT_HOST_PATH} \
        -DRIPES_WITH_QPROCESS=OFF \
        -DRIPES_BUILD_TESTS=OFF \
        -DVSRTL_BUILD_TESTS=OFF \
    && cmake --build /tmp/ripes/build --parallel "$(nproc)"

# Collect the web artifacts in a flat folder for the runtime stage.
RUN mkdir -p /opt/ripes-web \
    && cd /tmp/ripes/build \
    && cp -v Ripes.html Ripes.js Ripes.wasm /opt/ripes-web/ \
    # qtloader.js / qtlogo.svg are produced next to the executable by Qt for WASM.
    && for f in qtloader.js qtlogo.svg; do \
         if [ -f "$f" ]; then cp -v "$f" /opt/ripes-web/; fi; \
       done \
    # Provide a default landing page.
    && cp /opt/ripes-web/Ripes.html /opt/ripes-web/index.html


FROM nginx:alpine AS runtime

# nginx:alpine already maps application/wasm in /etc/nginx/mime.types.
# We add a tiny conf to set sane caching headers and gzip for .js/.wasm.
COPY --from=builder /opt/ripes-web/ /usr/share/nginx/html/

RUN printf '%s\n' \
    'gzip on;' \
    'gzip_types application/javascript application/wasm text/html image/svg+xml;' \
    'types { application/wasm wasm; }' \
    > /etc/nginx/conf.d/wasm.conf

EXPOSE 80
