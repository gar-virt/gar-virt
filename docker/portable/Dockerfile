FROM ghcr.io/steffenl/cxx-builder:1.3 AS builder

RUN <<EOF
#!/usr/bin/env sh
set -e
export DEBIAN_FRONTEND=noninteractive
apt-get update
apt-get install --yes --no-install-recommends p7zip-full ca-certificates curl patch setpriv xz-utils
apt-get install --yes --no-install-recommends \
    libvirt-dev
apt-get clean
rm -rf /var/lib/apt/lists/*
EOF

ARG USERNAME=builder
ARG UID=1000
ARG GID=1000

RUN <<EOF
#!/usr/bin/env sh
set -e
groupadd --gid "${GID}" "${USERNAME}"
useradd \
    --uid "${UID}" \
    --gid "${GID}" \
    --no-log-init \
    --create-home \
    "${USERNAME}"
tee /tmp/env.txt <<ENV_EOF
export USERNAME="${USERNAME}"
ENV_EOF
EOF

RUN tee /usr/local/bin/docker-entrypoint.sh <<'EOF'
#!/usr/bin/env sh
set -e
. /tmp/env.txt
uid=${HOST_UID:-$(id -u "${USERNAME}")}
gid=${HOST_GID:-$(id -g "${USERNAME}")}
# Avoid error: bash: /root/.bashrc: Permission denied
export HOME="/home/${USERNAME}"
exec setpriv --reuid "${uid}" --regid "${gid}" --init-groups "${@}"
EOF

WORKDIR /source
RUN chmod 555 /usr/local/bin/docker-entrypoint.sh
ENTRYPOINT ["/usr/local/bin/docker-entrypoint.sh"]
