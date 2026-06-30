FROM alpine:3.24 AS builder
RUN apk add --no-cache \
    # Toolchain
    cmake \
    ninja-build \
    ninja-is-really-ninja \
    gcc \
    g++ \
    # Development dependencies
    boost1.84-dev \
    boost1.84-static \
    curl-dev \
    grpc-dev \
    grpc-plugins \
    libvirt-dev \
    protobuf-dev \
    yaml-cpp-dev  \
    util-linux-dev
WORKDIR /source

FROM builder AS compile
COPY . .
RUN cmake -G Ninja -B /build -S . -D CMAKE_BUILD_TYPE=Release \
    && cmake --build /build \
    && cmake --install /build --prefix /build/install --strip

FROM alpine:3.24 AS dist
ARG USERNAME=gar-virt
ARG UID=1000
ARG GID=1000
RUN addgroup --gid "${GID}" "${USERNAME}" \
  && adduser \
    --uid "${UID}" \
    --ingroup "${USERNAME}" \
    --no-create-home \
    --disabled-password \
    "${USERNAME}"
RUN apk add --no-cache \
    # Runtime libraries
    boost1.84 \
    libcurl \
    libgrpc \
    libvirt \
    libprotobuf \
    yaml-cpp  \
    util-linux
COPY --from=compile /build/install/ /usr/local/
USER ${USERNAME}
ENTRYPOINT ["gar-virt"]
