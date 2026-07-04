FROM ubuntu:24.04 AS builder
# Enabling Universe is needed for some packages:
#   libboost-json-dev
#   libboost-url-dev
RUN echo "deb http://archive.ubuntu.com/ubuntu noble universe" >> /etc/apt/sources.list

RUN <<EOF
set -e
apt-get update
# Toolchain
apt-get install --yes --no-install-recommends \
    cmake \
    git \
    gcc-14 \
    g++-14 \
    ninja-build
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

RUN <<EOF
set -e
update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-14 140
update-alternatives --install /usr/bin/g++ g++ /usr/bin/g++-14 140
EOF

WORKDIR /source

FROM builder AS compile
COPY . .

RUN <<EOF
set -e
cmake -G Ninja -B /build -S . -D CMAKE_BUILD_TYPE=Release
cmake --build /build
cmake --install /build --prefix /build/install --strip
EOF

FROM ubuntu:24.04 AS dist
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

COPY --from=compile /build/install/ /usr/local/

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
