FROM ghcr.io/niksbrn/qt-ubuntu:6.10.2 AS builder

ARG DEBIAN_FRONTEND=noninteractive

# Install build dependencies
RUN apt-get update -q \
    && apt-get install -qy --no-install-recommends \
       build-essential cmake gcc-riscv64-unknown-elf git ca-certificates \
       libpthread-stubs0-dev python3 python3-pip python3-dev \
       libegl1-mesa-dev libgl1-mesa-dev libglib2.0-dev \
       libxkbcommon-x11-0 libpulse-dev libfontconfig1-dev libfreetype6-dev \
       libxcb-icccm4 libxcb-image0 libxcb-keysyms1 libxcb-render-util0 \
       libxcb-xinerama0 libxcb-composite0 libxcb-cursor0 libxcb-damage0 \
       libxcb-dpms0 libxcb-dri2-0 libxcb-dri3-0 libxcb-ewmh2 libxcb-glx0 \
       libxcb-present0 libxcb-randr0 libxcb-record0 libxcb-render0 libxcb-res0 \
       libxcb-screensaver0 libxcb-shape0 libxcb-shm0 libxcb-sync1 libxcb-util1 \
    && rm -rf /var/lib/apt/lists/*

# Install Qt 6.10.2
RUN mkdir -p /opt/qt/6.10.2 \
    && tar -xzf /tmp/qt-6.10.2-gcc_64.tar.gz -C /opt/qt/6.10.2 \
    && rm /tmp/qt-6.10.2-gcc_64.tar.gz

ARG GIT_SSL_NO_VERIFY=true
ENV LC_ALL=C.UTF-8 \
    SHELL=/bin/bash \
    LD_LIBRARY_PATH="/opt/qt/6.10.2/gcc_64/lib"

# Clone and build Ripes
ARG BRANCH=master
ARG RUN_TESTS=true

RUN git clone --recursive --branch ${BRANCH} \
        https://github.com/moevm/mse1h2026-ripes.git /tmp/ripes \
    && cmake -S /tmp/ripes \
             -B /tmp/ripes/build \
             -Wno-dev \
             -DRIPES_BUILD_TESTS=ON \
             -DVSRTL_BUILD_TESTS=ON \
             -DCMAKE_BUILD_TYPE=Release \
             -DCMAKE_PREFIX_PATH=/opt/qt/6.10.2/gcc_64 \
             -DCMAKE_INSTALL_PREFIX=/opt/ripes \
    && cmake --build /tmp/ripes/build --parallel "$(nproc)"

# Run tests
RUN if [ "${RUN_TESTS}" = "true" ]; then \
        cd /tmp/ripes/build/test \
        && ./tst_assembler \
        && ./tst_expreval \
        && ./tst_riscv ; \
    fi

# Install Ripes and strip binaries
RUN cmake --install /tmp/ripes/build \
    && rm -rf /tmp/ripes \
    && find /opt/ripes -type f -executable -exec strip --strip-unneeded {} + 2>/dev/null || true

# Prepare a minimal Qt runtime
RUN set -e && mkdir -p /opt/qt-runtime/lib /opt/qt-runtime/plugins \
    # Core Qt libraries and ICU
    && cp -a /opt/qt/6.10.2/gcc_64/lib/libQt6*.so*  /opt/qt-runtime/lib/ \
    && cp -a /opt/qt/6.10.2/gcc_64/lib/libicu*.so*   /opt/qt-runtime/lib/ 2>/dev/null || true \
    && for d in platforms imageformats platformthemes xcbglintegrations; do \
         [ -d "/opt/qt/6.10.2/gcc_64/plugins/$d" ] \
           && cp -rL "/opt/qt/6.10.2/gcc_64/plugins/$d" /opt/qt-runtime/plugins/ ; \
       done \
    && find /opt/qt-runtime -name '*.debug' -delete 2>/dev/null || true


FROM ghcr.io/niksbrn/riscv-ubuntu:64 AS runtime

ARG DEBIAN_FRONTEND=noninteractive

# Install only necessary runtime libraries
RUN apt-get update -q \
    && apt-get install -qy --no-install-recommends \
       # OpenGL and EGL (Qt)
       libegl1 \
       libgl1 \
       libglx-mesa0 \
       libglx0 \
       libopengl0 \
       # GLib and D-Bus
       libglib2.0-0 \
       libdbus-1-3 \
       # XKB
       libxkbcommon-x11-0 \
       libxkbcommon0 \
       # Fonts
       libfontconfig1 \
       libfreetype6 \
       fontconfig \
       fonts-dejavu-core \
       # Audio (optional, but often needed)
       libpulse0 \
       # XCB libraries (required by Qt's xcb platform plugin)
       libxcb-icccm4 \
       libxcb-image0 \
       libxcb-keysyms1 \
       libxcb-render-util0 \
       libxcb-xinerama0 \
       libxcb-composite0 \
       libxcb-cursor0 \
       libxcb-damage0 \
       libxcb-dpms0 \
       libxcb-dri2-0 \
       libxcb-dri3-0 \
       libxcb-ewmh2 \
       libxcb-glx0 \
       libxcb-present0 \
       libxcb-randr0 \
       libxcb-record0 \
       libxcb-render0 \
       libxcb-res0 \
       libxcb-screensaver0 \
       libxcb-shape0 \
       libxcb-shm0 \
       libxcb-sync1 \
       libxcb-util1 \
       libxcb1 \
       libx11-xcb1 \
       libx11-6 \
       # Utilities for extracting toolchain
       tar \
       xz-utils \
    && rm -rf /var/lib/apt/lists/*

# Install RISC-V toolchain
RUN tar xzf /tmp/riscv64-unknown-elf-gcc-8.3.0-2020.04.1-x86_64-linux-ubuntu14.tar.gz -C /opt \
    && rm /tmp/riscv64-unknown-elf-gcc-8.3.0-2020.04.1-x86_64-linux-ubuntu14.tar.gz

# Copy Ripes and Qt runtime from builder
COPY --from=builder /opt/ripes              /opt/ripes
COPY --from=builder /opt/qt-runtime/lib     /opt/qt/lib
COPY --from=builder /opt/qt-runtime/plugins /opt/qt/plugins

# Set environment variables
ENV LD_LIBRARY_PATH="/opt/qt/lib" \
    QT_PLUGIN_PATH="/opt/qt/plugins" \
    QT_QPA_PLATFORM="xcb" \
    PATH="/opt/ripes/bin:/opt/riscv64-unknown-elf-gcc-8.3.0-2020.04.1-x86_64-linux-ubuntu14/bin:${PATH}" \
    LC_ALL=C.UTF-8

USER root
ENV DEBIAN_FRONTEND=noninteractive
ENV TZ=Europe/Moscow

RUN apt-get update && apt-get install -y \
    tzdata \
    x11vnc \
    xvfb \
    fluxbox \
    websockify \
    novnc \
    procps \
    sudo \
    && rm -rf /var/lib/apt/lists/*

RUN useradd -m -s /bin/bash developer || true && \
    echo "developer:developer" | chpasswd && \
    usermod -aG audio,video developer && \
    echo "developer ALL=(ALL) NOPASSWD:ALL" >> /etc/sudoers

RUN mkdir -p /home/developer/projects && \
    chown developer:developer /home/developer/projects

COPY entrypoint-vnc.sh /entrypoint.sh
RUN chmod +x /entrypoint.sh

ENV DISPLAY=:0 \
    QT_QPA_PLATFORM=xcb

USER developer
WORKDIR /home/developer

ENTRYPOINT ["/entrypoint.sh"]