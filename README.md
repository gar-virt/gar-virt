# gar-virt - Gitea Actions Runner with Libvirt Autoscaling

A simple Gitea Actions runner controller/orchestrator for self-hosted runner autoscaling with remote and local libvirt support.

No complicated runner registration, no webhooks to set up, and no web UI or API endpoints. Just bring your own libvirt host, virtual machine templates. Seamless management of native Gitea Actions ephemeral runners.

## Sample Config

See `sample_config/` for sample configuration files. This format is a work-in-progress and may change.

Make sure to customize these files if you use them.

## Building

Refer to `docker/<flavor>/Dockerfile` for library dependencies and packages to install.

```sh
cmake -G Ninja -B build -S . -D CMAKE_BUILD_TYPE=Release
cmake --build build
cmake --install build --prefix build/install --strip
```

Run it:

```sh
build/install/bin/gar-virt --verbose --config-file .config/config.yaml
```

## Docker Image

See `docker/<flavor>/Dockerfile` for details or just build your flavor as instructed below.

```sh
for flavor in alpine ubuntu; do
    docker build --tag "gar-virt:${flavor}" --target dist --file "docker/${flavor}/Dockerfile" . || break
done
```

Run it:

```sh
docker run --rm -it --init \
    --volume "$(pwd)/.config:/etc/gar-virt:ro" \
    gar-virt:ubuntu --verbose --config-file /etc/gar-virt/config.yaml
```
