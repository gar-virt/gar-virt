ARG BASE_IMAGE=alpine:3.24
FROM ${BASE_IMAGE} AS builder

RUN <<EOF
set -e
# Toolchain
apk add --no-cache \
    cmake \
    ninja-build \
    ninja-is-really-ninja \
    gcc \
    g++
# Development dependencies
apk add --no-cache \
    boost1.84-dev \
    boost1.84-static \
    curl-dev \
    grpc-dev \
    grpc-plugins \
    libvirt-dev \
    protobuf-dev \
    yaml-cpp-dev  \
    util-linux-dev
EOF

WORKDIR /source
ARG BUILD_TYPE=Release

FROM builder AS compile
COPY . .

RUN <<EOF
set -e
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
addgroup --gid "${GID}" "${USERNAME}"
adduser \
    --uid "${UID}" \
    --ingroup "${USERNAME}" \
    --disabled-password \
    "${USERNAME}"
EOF

RUN <<EOF
set -e
# Runtime libraries
apk add --no-cache \
    boost1.84 \
    libcurl \
    libgrpc \
    libvirt \
    libprotobuf \
    yaml-cpp  \
    util-linux
# SSH support via OpenSSH and libssh
apk add --no-cache \
    openssh-client-default \
    libssh
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
