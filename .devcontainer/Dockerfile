# Devcontainer / Codespace image for building Luthier on Linux.
# Mirrors scripts/setup_linux.sh: the JUCE GUI dev packages plus the toolchain
# (cmake, ninja, clang, ccache) and xvfb for running the headless test runner.
FROM mcr.microsoft.com/devcontainers/base:ubuntu-24.04

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update \
    && apt-get install -y --no-install-recommends \
        cmake \
        ninja-build \
        clang \
        ccache \
        git \
        ca-certificates \
        pkg-config \
        libasound2-dev \
        libxrandr-dev \
        libxcursor-dev \
        libxinerama-dev \
        libxcomposite-dev \
        libfontconfig1-dev \
        libfreetype-dev \
        libcurl4-openssl-dev \
        libgl1-mesa-dev \
        xvfb \
    && apt-get clean \
    && rm -rf /var/lib/apt/lists/*
