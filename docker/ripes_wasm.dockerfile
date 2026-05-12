# syntax=docker/dockerfile:1.6
#
# WASM build of Ripes that mirrors the upstream wasm-release.yml workflow:
#   - Qt 6.6.0 (host gcc_64 + wasm_multithread, with qtcharts)
#   - emsdk 3.1.25
#   - plain cmake with the Qt toolchain file, EMSCRIPTEN=1,
#     EMSCRIPTEN_FORCE_COMPILERS=ON, RIPES_WITH_QPROCESS=OFF
#   - artifacts: Ripes.html / Ripes.js / Ripes.wasm / Ripes.worker.js / qtloader.js
#
# The multithread WASM build requires SharedArrayBuffer, so nginx must serve
# the page with cross-origin isolation headers (COOP + COEP).

ARG QT_VERSION=6.6.0
ARG EMSDK_VERSION=3.1.25
ARG BRANCH=master


FROM --platform=linux/amd64 ubuntu:22.04 AS builder

ARG QT_VERSION
ARG EMSDK_VERSION
ARG BRANCH
ARG DEBIAN_FRONTEND=noninteractive

ENV CC=emcc
ENV CXX=em++

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

# ---- Emscripten (must match the version Qt for WASM was built with) ----------
RUN git clone https://github.com/emscripten-core/emsdk.git /opt/emsdk \
    && cd /opt/emsdk \
    && ./emsdk install ${EMSDK_VERSION} \
    && ./emsdk activate ${EMSDK_VERSION}

# ---- Sources -----------------------------------------------------------------
RUN git clone --recursive --branch ${BRANCH} \
        https://github.com/moevm/mse1h2026-ripes.git /tmp/ripes

# ---- Configure & build (mirrors upstream wasm-release.yml) -------------------
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

# Multithread Qt-for-WASM uses pthreads, which require SharedArrayBuffer,
# which the browser only enables on cross-origin-isolated pages.
RUN printf '%s\n' \
    'server {' \
    '    listen 80;' \
    '    server_name _;' \
    '    root /usr/share/nginx/html;' \
    '    index index.html;' \
    '' \
    '    gzip on;' \
    '    gzip_types application/javascript application/wasm text/html image/svg+xml;' \
    '' \
    '    types {' \
    '        application/wasm wasm;' \
    '        application/javascript js;' \
    '        text/html html;' \
    '        image/svg+xml svg;' \
    '    }' \
    '' \
    '    add_header Cross-Origin-Opener-Policy   "same-origin"   always;' \
    '    add_header Cross-Origin-Embedder-Policy "require-corp"  always;' \
    '    add_header Cross-Origin-Resource-Policy "same-origin"   always;' \
    '' \
    '    location / {' \
    '        try_files $uri $uri/ =404;' \
    '    }' \
    '}' \
    > /etc/nginx/conf.d/default.conf

EXPOSE 80
