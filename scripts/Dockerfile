FROM ubuntu:22.04
RUN apt-get update && apt-get install -y --no-install-recommends gcc-aarch64-linux-gnu libc6-dev-arm64-cross make python3 file && rm -rf /var/lib/apt/lists/*
ENV CROSS_PREFIX=aarch64-linux-gnu-
WORKDIR /build
