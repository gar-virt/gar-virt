FROM ubuntu:24.04 AS builder
# Enabling Universe is needed for some packages:
#   libboost-json-dev
#   libboost-url-dev
RUN echo "deb http://archive.ubuntu.com/ubuntu noble universe" >> /etc/apt/sources.list
RUN apt-get update \
    && apt-get install --yes --no-install-recommends \
        # Toolchain
        cmake \
        git \
        gcc-14 \
        g++-14 \
        ninja-build \
        # Development dependencies
        libboost-json-dev \
        libboost-program-options-dev \
        libboost-url-dev \
        libcurl4-openssl-dev \
        libgrpc++-dev \
        libprotobuf-dev \
        libyaml-cpp-dev \
        libvirt-dev \
        protobuf-compiler \
        protobuf-compiler-grpc \
        uuid-dev \
    && apt-get clean \
    && rm -rf /var/lib/apt/lists/*
RUN update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-14 140 \
    && update-alternatives --install /usr/bin/g++ g++ /usr/bin/g++-14 140
WORKDIR /source

FROM builder AS compile
COPY . .
RUN cmake -G Ninja -B /build -S . -D CMAKE_BUILD_TYPE=Release \
    && cmake --build /build \
    && cmake --install /build --prefix /build/install --strip

FROM ubuntu:24.04 AS dist
ARG USERNAME=gar-virt
ARG UID=1000
ARG GID=1000
RUN userdel --remove ubuntu \
    && groupadd --gid "${GID}" "${USERNAME}" \
    && useradd \
        --uid "${UID}" \
        --gid "${GID}" \
        --no-log-init \
        --no-create-home \
        "${USERNAME}"
RUN apt-get update \
    && apt-get install --yes --no-install-recommends \
        # Runtime libraries
        ca-certificates \
        libcurl4t64 \
        libgrpc++1.51t64 \
        libprotobuf-c1 \
        libuuid1 \
        libyaml-cpp0.8 \
        libvirt0 \
    && apt-get clean \
    && rm -rf /var/lib/apt/lists/*
COPY --from=compile /build/install/ /usr/local/
USER ${USERNAME}
ENTRYPOINT ["gar-virt"]
