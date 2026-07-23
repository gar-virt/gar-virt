ARG BASE_IMAGE=ghcr.io/gar-virt/runner-images:ubuntu-24.04
FROM ${BASE_IMAGE} AS builder

# Enabling Universe is needed for some packages:
#   libboost-json-dev
#   libboost-url-dev
RUN echo "deb http://archive.ubuntu.com/ubuntu noble universe" >> /etc/apt/sources.list

# Supported values: gcc, clang
ARG COMPILER=gcc
ARG GCC_VERSION=14
ARG CLANG_VERSION=20

RUN <<EOF
set -e

# Compiler
if [ "${COMPILER}" = "gcc" ]; then
    echo "CMAKE_C_COMPILER=gcc-${GCC_VERSION}" >> /tmp/compile_env.txt
    echo "CMAKE_CXX_COMPILER=g++-${GCC_VERSION}" >> /tmp/compile_env.txt
elif [ "${COMPILER}" = "clang" ]; then
    echo "CMAKE_C_COMPILER=clang-${CLANG_VERSION}" >> /tmp/compile_env.txt
    echo "CMAKE_CXX_COMPILER=clang-${CLANG_VERSION}" >> /tmp/compile_env.txt
else
    echo "Invalid compiler: ${COMPILER}"
    exit 1
fi

export DEBIAN_FRONTEND=noninteractive
apt-get update

# Development dependencies
apt-get install --yes --no-install-recommends \
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
    uuid-dev
apt-get clean
rm -rf /var/lib/apt/lists/*
EOF

WORKDIR /source
ARG BUILD_TYPE=Release

FROM builder AS compile
ARG BUILD_TYPE
COPY --from=builder /tmp/compile_env.txt /tmp/compile_env.txt
COPY . .

RUN <<EOF
set -e
. /tmp/compile_env.txt
cmake -G "Ninja Multi-Config" -B /build -S .
cmake --build /build --config "${BUILD_TYPE}"
EOF

FROM compile AS test
ARG BUILD_TYPE
COPY . .

RUN <<EOF
set -e
ctest --build-config "${BUILD_TYPE}"
EOF

FROM compile AS dist-prep
COPY --from=compile /build /build
COPY . .

RUN <<EOF
set -e
cmake --install /build --config "${BUILD_TYPE}" --prefix /build/install --strip
EOF

FROM ${BASE_IMAGE} AS dist
ARG USERNAME=gar-virt
ARG UID=1000
ARG GID=1000

RUN <<EOF
set -e
userdel --remove ubuntu
groupadd --gid "${GID}" "${USERNAME}"
useradd \
    --uid "${UID}" \
    --gid "${GID}" \
    --no-log-init \
    --create-home \
    "${USERNAME}"
EOF

RUN <<EOF
set -e
export DEBIAN_FRONTEND=noninteractive
apt-get update
# Runtime libraries
apt-get install --yes --no-install-recommends \
    ca-certificates \
    libcurl4t64 \
    libgrpc++1.51t64 \
    libprotobuf-c1 \
    libuuid1 \
    libyaml-cpp0.8 \
    libvirt0
# SSH support via OpenSSH and libssh
apt-get install --yes --no-install-recommends \
    openssh-client \
    libssh-4
apt-get clean
rm -rf /var/lib/apt/lists/*
EOF

COPY --from=dist-prep /build/install/ /usr/local/

RUN cat <<EOF > /usr/local/bin/docker-entrypoint.sh
#!/usr/bin/env sh
set -e

if [ -d /run/gar-virt-config ]; then
    cp --archive /run/gar-virt-config /etc/gar-virt
    chown -R "${USERNAME}:${USERNAME}" /etc/gar-virt/
fi

exec setpriv --reuid "${USERNAME}" --regid "${USERNAME}" --init-groups /usr/local/bin/gar-virt "\$@"
EOF

RUN chmod 555 /usr/local/bin/docker-entrypoint.sh
ENTRYPOINT ["/usr/local/bin/docker-entrypoint.sh"]
