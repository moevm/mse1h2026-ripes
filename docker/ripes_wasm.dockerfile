# syntax=docker/dockerfile:1.6
ARG QT_VERSION=6.6.0
ARG EMSDK_VERSION=3.1.25
ARG BRANCH=master

FROM --platform=linux/amd64 ubuntu:22.04 AS builder

ARG QT_VERSION
ARG EMSDK_VERSION
ARG BRANCH
ARG DEBIAN_FRONTEND=noninteractive

RUN apt-get update -q && apt-get upgrade -y -q \
    && apt-get install -y -q --no-install-recommends \
       automake cmake git wget libfuse2 desktop-file-utils tree \
       build-essential libgl1-mesa-dev libxkbcommon-x11-0 libpulse-dev \
       libxcb-icccm4 libxcb-image0 libxcb-keysyms1 libxcb-render-util0 \
       libxcb-xinerama0 libxcb-composite0 libxcb-cursor0 libxcb-damage0 \
       libxcb-dpms0 libxcb-dri2-0 libxcb-dri3-0 libxcb-ewmh2 libxcb-glx0 \
       libxcb-present0 libxcb-randr0 libxcb-record0 libxcb-render0 libxcb-res0 \
       libxcb-screensaver0 libxcb-shape0 libxcb-shm0 libxcb-sync1 libxcb-util1 \
       libegl1 libegl1-mesa-dev \
       ca-certificates xz-utils python3 python3-pip python3-venv \
    && rm -rf /var/lib/apt/lists/*

RUN python3 -m venv /opt/aqt-venv \
    && /opt/aqt-venv/bin/pip install --no-cache-dir --upgrade pip \
    && /opt/aqt-venv/bin/pip install --no-cache-dir "aqtinstall==3.1.*" "py7zr>=0.20.2"

RUN /opt/aqt-venv/bin/aqt install-qt linux desktop ${QT_VERSION} gcc_64 \
        -m qtcharts \
        -O /Qt

RUN /opt/aqt-venv/bin/aqt install-qt linux desktop ${QT_VERSION} wasm_multithread \
        -m qtcharts \
        -O /Qt

RUN chmod -R +x /Qt/${QT_VERSION}/gcc_64/bin \
    && chmod -R +x /Qt/${QT_VERSION}/wasm_multithread/bin

ENV EMSDK=/opt/emsdk
RUN git clone https://github.com/emscripten-core/emsdk.git /opt/emsdk \
    && cd /opt/emsdk \
    && ./emsdk install ${EMSDK_VERSION} \
    && ./emsdk activate ${EMSDK_VERSION}

RUN git clone --recursive --branch ${BRANCH} \
        https://github.com/moevm/mse1h2026-ripes.git /src/ripes

ENV CC=emcc
ENV CXX=em++

WORKDIR /src/ripes

RUN cd /opt/emsdk \
    && . ./emsdk_env.sh \
    && cd /src/ripes \
    && export QT_HOST_PATH=/Qt/${QT_VERSION}/gcc_64/ \
    && cmake \
        -DCMAKE_BUILD_TYPE=Release \
        -DRIPES_WITH_QPROCESS=OFF \
        -DEMSCRIPTEN=1 \
        -DQT_HOST_PATH="${QT_HOST_PATH}" \
        -DCMAKE_TOOLCHAIN_FILE=/Qt/${QT_VERSION}/wasm_multithread/lib/cmake/Qt6/qt.toolchain.cmake \
        -DCMAKE_PREFIX_PATH=/Qt/${QT_VERSION}/wasm_multithread/ \
        -DEMSCRIPTEN_FORCE_COMPILERS=ON \
        . \
    && make -j "$(nproc)"

RUN mkdir -p /opt/ripes-web \
    && cp -v /src/ripes/Ripes.wasm /opt/ripes-web/ \
    && cp -v /src/ripes/Ripes.js   /opt/ripes-web/ \
    && for f in /src/ripes/Ripes.worker.js /src/ripes/qtloader.js /src/ripes/Ripes.html /src/ripes/qtlogo.svg; do \
         if [ -f "$f" ]; then cp -v "$f" /opt/ripes-web/; fi; \
       done \
    && if [ -f /opt/ripes-web/Ripes.html ]; then cp /opt/ripes-web/Ripes.html /opt/ripes-web/index.html; fi

FROM nginx:alpine AS runtime

COPY --from=builder /opt/ripes-web/ /usr/share/nginx/html/

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
    '    location ~* \.js$ {' \
    '        default_type application/javascript;' \
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
