FROM ubuntu:24.04

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    git \
    gdb \
    ninja-build \
    pkg-config \
    # Core Dependencies
    libjsoncpp-dev \
    uuid-dev \
    libssl-dev \
    zlib1g-dev \
    postgresql-server-dev-all \
    libhiredis-dev \
    libsqlite3-dev \
    libargon2-dev \
    && apt-get clean && rm -rf /var/lib/apt/lists/*

RUN git clone https://github.com/Thalhammer/jwt-cpp.git \
    && cd jwt-cpp \
    && git checkout v0.7.0 \
    && mkdir build && cd build \
    && cmake -DJWT_BUILD_EXAMPLES=OFF -DJWT_BUILD_TESTS=OFF -DJWT_SSL_LIBRARY=OpenSSL .. \
    && make install \
    && cd ../.. && rm -rf jwt-cpp

RUN git clone https://github.com/drogonframework/drogon.git \
    && cd drogon \
    && git checkout v1.9.8 \
    && git submodule update --init \
    && mkdir build && cd build \
    && cmake -DCMAKE_BUILD_TYPE=Release \
             -DBUILD_MYSQL=OFF \
             -DBUILD_POSTGRESQL=ON \
             -DBUILD_REDIS=ON \
             .. \
    && make -j$(nproc) \
    && make install \
    && cd ../.. && rm -rf drogon

RUN gcc --version