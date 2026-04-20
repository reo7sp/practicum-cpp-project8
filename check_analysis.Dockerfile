FROM ubuntu:24.04

RUN apt-get update && apt-get install -y \
    bash \
    build-essential \
    clang \
    cmake \
    git \
    linux-tools-common \
    linux-tools-generic \
    llvm \
    && rm -rf /var/lib/apt/lists/*
