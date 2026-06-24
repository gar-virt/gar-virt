FROM ubuntu:24.04 AS build
# Required for CMake installation
ENV LANG=C.UTF-8
# Set up toolchain
RUN apt-get update \
    && apt-get upgrade -y \
    && apt-get install -y --no-install-recommends \
        7zip \
        ca-certificates \
        git \
        gcc-14 g++-14 \
        unzip
RUN update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-14 140 \
    && update-alternatives --install /usr/bin/g++ g++ /usr/bin/g++-14 140
WORKDIR /source
# Make any already-downloaded external tools available
COPY ./dependencies ./dependencies/
RUN dependencies/tools/download.sh
RUN dependencies/tools/downloaded/cmake-4.1.2.sh --skip-license --exclude-subdir --prefix=/usr/local
RUN unzip -d /usr/local/bin dependencies/tools/downloaded/ninja-1.13.1.zip
# Install development tools and libraries
RUN apt-get install -y --no-install-recommends \
    libcurl4-openssl-dev \
    libgrpc++-dev \
    libprotobuf-dev \
    libyaml-cpp-dev \
    protobuf-compiler \
    protobuf-compiler-grpc \
    uuid-dev
# Make any already-downloaded external libraries available
RUN dependencies/libraries/download.sh
RUN 7z x -obuild/_deps/ dependencies/libraries/downloaded/boost-1.89.0.7z \
    && mv build/_deps/boost-1.89.0 build/_deps/boost-src
# Build project
COPY . .
RUN cmake -G Ninja -B build -S . -D CMAKE_BULID_TYPE=Release -D FETCHCONTENT_FULLY_DISCONNECTED=TRUE \
    && cmake --build build \
    && cmake --install build --prefix install --strip

FROM ubuntu:24.04 AS final
# Install Docker
RUN apt-get update \
    && apt-get upgrade -y \
    && apt-get install -y --no-install-recommends \
        ca-certificates \
        curl \
    && install -m 0755 -d /etc/apt/keyrings \
    && curl -fsSL https://download.docker.com/linux/ubuntu/gpg -o /etc/apt/keyrings/docker.asc \
    && chmod a+r /etc/apt/keyrings/docker.asc \
    && echo "deb [arch=$(dpkg --print-architecture) signed-by=/etc/apt/keyrings/docker.asc] https://download.docker.com/linux/ubuntu $(. /etc/os-release && echo "${UBUNTU_CODENAME:-$VERSION_CODENAME}") stable" \
        | tee /etc/apt/sources.list.d/docker.list > /dev/null \
    && apt-get update \
    && apt-get install -y --no-install-recommends \
        docker-ce \
        docker-ce-cli \
        containerd.io \
    && apt-get clean \
    && rm -rf /var/lib/apt/lists/*
# Install runtime libraries
RUN apt-get update \
    && apt-get upgrade -y \
    && apt-get install -y --no-install-recommends \
        ca-certificates \
        libcurl4t64 \
        libgrpc++1.51t64 \
        libprotobuf-c1 \
        libuuid1 \
        libyaml-cpp0.8 \
    && apt-get clean \
    && rm -rf /var/lib/apt/lists/*
# Install project
COPY --from=build /source/install/ /usr/local/
ENTRYPOINT ["ga_runner"]
