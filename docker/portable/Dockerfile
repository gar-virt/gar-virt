FROM ghcr.io/steffenl/cxx-builder:1.3 AS builder

RUN <<EOF
#!/usr/bin/env sh
set -eu
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
set -eu
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
set -eu
. /tmp/env.txt
current_uid=$(id -u "${USERNAME}")
current_gid=$(id -g "${USERNAME}")
uid=${HOST_UID:-${current_uid}}
gid=${HOST_GID:-${current_gid}}
if [ "${uid}" != "${current_gid}" ]; then
    usermod --uid "${uid}" "${USERNAME}"
fi
if [ "${gid}" != "${current_gid}" ]; then
    group_name=$(id --group --name "${USERNAME}")
    groupmod --gid "${gid}" "${group_name}"
fi
# Avoid error: bash: /root/.bashrc: Permission denied
export HOME=$(getent passwd "${USERNAME}" | cut -d: -f6)
exec setpriv --reuid "${uid}" --regid "${gid}" --init-groups "${@}"
EOF

WORKDIR /source
RUN chmod 555 /usr/local/bin/docker-entrypoint.sh
ENTRYPOINT ["/usr/local/bin/docker-entrypoint.sh"]
