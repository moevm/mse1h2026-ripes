# syntax=docker/dockerfile:1.6
#
# WASM build of Ripes (Qt 6.6.0 wasm_multithread + emsdk 3.1.37).

ARG QT_VERSION=6.6.0
# emsdk version MUST match the one Qt-for-WASM was built with.
# Qt 6.6.x -> emsdk 3.1.37, Qt 6.5.x -> 3.1.25, Qt 6.7.x -> 3.1.50
ARG EMSDK_VERSION=3.1.37
ARG BRANCH=master

FROM --platform=linux/amd64 ubuntu:22.04 AS builder

ARG QT_VERSION
ARG EMSDK_VERSION
ARG BRANCH
ARG DEBIAN_FRONTEND=noninteractive

RUN apt-get update -q \
    && apt-get install -qy --no-install-recommends \
       build-essential cmake git ca-certificates xz-utils wget \
       python3 python3-pip python3-venv \
       libglib2.0-0 \
       libgl1-mesa-dev libxkbcommon-x11-0 libegl1 libegl1-mesa-dev \
    && rm -rf /var/lib/apt/lists/*

# ---- Qt: host (gcc_64) + wasm_multithread, both with qtcharts ----------------
RUN python3 -m venv /opt/aqt-venv \
    && /opt/aqt-venv/bin/pip install --no-cache-dir --upgrade pip \
    && /opt/aqt-venv/bin/pip install --no-cache-dir "aqtinstall>=3.1,<4"

RUN /opt/aqt-venv/bin/aqt install-qt linux desktop ${QT_VERSION} gcc_64 \
        -m qtcharts \
        -O /opt/qt

RUN /opt/aqt-venv/bin/aqt install-qt linux desktop ${QT_VERSION} wasm_multithread \
        -m qtcharts \
        -O /opt/qt

RUN chmod -R +x /opt/qt/${QT_VERSION}/gcc_64/bin \
    && chmod -R +x /opt/qt/${QT_VERSION}/wasm_multithread/bin

ENV QT_HOST_PATH=/opt/qt/${QT_VERSION}/gcc_64
ENV QT_WASM_PATH=/opt/qt/${QT_VERSION}/wasm_multithread
ENV LD_LIBRARY_PATH=${QT_HOST_PATH}/lib

# ---- Emscripten (must match Qt-for-WASM's version) ---------------------------
RUN git clone https://github.com/emscripten-core/emsdk.git /opt/emsdk \
    && cd /opt/emsdk \
    && ./emsdk install ${EMSDK_VERSION} \
    && ./emsdk activate ${EMSDK_VERSION}

# ---- Sources -----------------------------------------------------------------
RUN git clone --recursive --branch ${BRANCH} \
        https://github.com/moevm/mse1h2026-ripes.git /tmp/ripes

# ---- Configure & build -------------------------------------------------------
# Exception model:
#   Qt 6.6 wasm_multithread from aqtinstall is built with JS-based exceptions
#   (-fexceptions), so the app MUST be compiled and linked with -fexceptions too.
#   This flag MUST be passed via CMAKE_CXX_FLAGS, CMAKE_C_FLAGS AND
#   CMAKE_EXE_LINKER_FLAGS, otherwise unwind tables won't be generated and
#   you'll get:
#     "Exception thrown, but exception catching is not enabled"
#
# Debug flags like -sASSERTIONS=2 are intentionally NOT used here: they enable
# an internal Emscripten check in Asyncify that wrongly aborts Qt's event loop
# with "Cannot have multiple async operations in flight at once" when two
# DOM callbacks (e.g. resize + mouse) arrive close together. In production
# (ASSERTIONS=0) this code path is simply skipped.
RUN cd /opt/emsdk && . ./emsdk_env.sh \
    && cd /tmp/ripes \
    && cmake \
        -S /tmp/ripes \
        -B /tmp/ripes/build \
        -Wno-dev \
        -DCMAKE_BUILD_TYPE=Release \
        -DRIPES_WITH_QPROCESS=OFF \
        -DRIPES_BUILD_TESTS=OFF \
        -DVSRTL_BUILD_TESTS=OFF \
        -DEMSCRIPTEN=1 \
        -DEMSCRIPTEN_FORCE_COMPILERS=ON \
        -DQT_HOST_PATH=${QT_HOST_PATH} \
        -DCMAKE_TOOLCHAIN_FILE=${QT_WASM_PATH}/lib/cmake/Qt6/qt.toolchain.cmake \
        -DCMAKE_PREFIX_PATH=${QT_WASM_PATH} \
        -DCMAKE_C_FLAGS="-fexceptions" \
        -DCMAKE_CXX_FLAGS="-fexceptions" \
        -DCMAKE_C_FLAGS_RELEASE="-O3 -DNDEBUG -fexceptions" \
        -DCMAKE_CXX_FLAGS_RELEASE="-O3 -DNDEBUG -fexceptions" \
        -DCMAKE_EXE_LINKER_FLAGS="-fexceptions" \
    && cmake --build /tmp/ripes/build --parallel "$(nproc)"

# ---- Stage artifacts ---------------------------------------------------------
RUN mkdir -p /opt/ripes-web \
    && cd /tmp/ripes/build \
    && cp -v Ripes.html Ripes.js Ripes.wasm /opt/ripes-web/ \
    && for f in Ripes.worker.js qtloader.js qtlogo.svg; do \
         if [ -f "$f" ]; then cp -v "$f" /opt/ripes-web/; fi; \
       done \
    && cp /opt/ripes-web/Ripes.html /opt/ripes-web/index.html

# =============================================================================
FROM nginx:alpine AS runtime

COPY --from=builder /opt/ripes-web/ /usr/share/nginx/html/

# Multithread Qt-for-WASM needs SharedArrayBuffer, which the browser only
# enables on cross-origin-isolated pages (COOP + COEP + CORP).
#
# IMPORTANT: we do NOT redefine a local `types {}` block (that would wipe out
# nginx's default mime.types). Instead we just override the type for .wasm.
# We also exclude application/wasm from gzip_types, because gzipped wasm can
# break WebAssembly.instantiateStreaming under some COEP setups.
RUN printf '%s\n' \
    'server {' \
    '    listen 80;' \
    '    server_name _;' \
    '    root /usr/share/nginx/html;' \
    '    index index.html;' \
    '' \
    '    gzip on;' \
    '    gzip_types application/javascript text/html image/svg+xml text/css application/json;' \
    '' \
    '    add_header Cross-Origin-Opener-Policy   "same-origin"  always;' \
    '    add_header Cross-Origin-Embedder-Policy "require-corp" always;' \
    '    add_header Cross-Origin-Resource-Policy "same-origin"  always;' \
    '' \
    '    location ~* \.wasm$ {' \
    '        default_type application/wasm;' \
    '        add_header Cross-Origin-Opener-Policy   "same-origin"  always;' \
    '        add_header Cross-Origin-Embedder-Policy "require-corp" always;' \
    '        add_header Cross-Origin-Resource-Policy "same-origin"  always;' \
    '    }' \
    '' \
    '    location / {' \
    '        try_files $uri $uri/ =404;' \
    '    }' \
    '}' \
    > /etc/nginx/conf.d/default.conf

EXPOSE 80
