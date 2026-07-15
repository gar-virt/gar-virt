# gar-virt - Gitea Actions Runner with Libvirt Autoscaling

A simple Gitea Actions runner controller/orchestrator for self-hosted runner autoscaling with local and remote [libvirt][libvirt]/QEMU support.

## Project Status

This project is in early/active development, but all of the documented features are known to work end-to-end locally.

## How It Works

* gar-virt starts by retrieving the runner registration token via the Gitea's REST API.
* One or more pools will be created for pre-warming and scaling machines.
* It registers itself as one or more global ephemeral runners via Gitea's gRPC service where it'll be assigned tasks.
* Once a task is assigned, the ephemeral runner registration details and assigned task are delegated to the Gitea Runner executable that'll execute the workflow in a pre-warmed virtual machine.
* For Linux environments, the Gitea Runner will typically run workflows in a Docker container inside the virtual machine.
* When the workflow execution completes, the virtual machine is destroyed.

## Scaling

Upscaling of machines is currently quite basic. Runners only register and listen for tasks when a machine is ready to take a task. Without any prediction or other clever calculations, and without having the ability to inspect the task queue depth, it may lag behind a long queue for a bit until fully ramped-up.

Gitea's admin API has an endpoint for querying the list of workflow runs, but without a way to filter by labels (as of Gitea 1.26), gar-virt can't know whether any of its runners are capable of taking those queued tasks.

As for downscaling, it occurs naturally as every machine is ephemeral and reaches its end of life as soon as it completes its task.

### Libvirt

gar-virt uses the libvirt C API and its QEMU support to manage virtual machines created from base disk images and XML documents that you supply.

One can easily create virtual machine templates and experiment with domain XML using [Virtual Machine Manager][virt-manager]. Documentation for automating base image creation and custom runner images for Docker have been published in a [separate repository][runner-images].

### Gitea Runner

gar-virt uses the [official Gitea Runner][gitea-runner], but [with some patches][gitea-runner-patches] to allow it to run a single given task.

This project originally had a simple runner implementation in C++ with workflow execution, but it was scrapped as I couldn't easily see where the project's scope would end. Offloading the workflow execution to the official Gitea Runner keeps the project scope to a maintainable level.

## Requirements

* Linux host (tested: Ubuntu 24.04, Alpine 3.24)
* Libvirt (tested: 10.0)
* QEMU (tested: 8.2)
* Development: C++23 compiler (GCC >= 14, Clang >= 19), CMake >= 3.28, Ninja >= 1.11
* Refer to `docker/<variant>/Dockerfile` for the full list of dependencies.

## Building

```sh
cmake -G Ninja -B build -S . -D CMAKE_BUILD_TYPE=Release
cmake --build build
cmake --install build --prefix build/install --strip
```

## Quick Start

Copy sample configuration files:

```sh
cp -r sample_config .config
```

You need to edit the `.config/config.yaml` file with your own Gitea instance URL, Gitea API token, runner name, hypervisor URI among a few other things. See the inline comments for more documentation.

Take a moment to inspect the sample domain XML files in the `templates/` directory. You can edit these or use your own custom files. You must insert `${DOMAIN_NAME}` in the right places in the XML documents so that gar-virt can substitute those with unique identifiers and avoid name collisions. See the sample XML files for where to insert this keyword.

Run it:

```sh
build/install/bin/gar-virt --config-file .config/config.yaml --verbose
```

The `--verbose` flag can be specified to increase the log level. Logs are printed to stdout.

### Remote Connection via SSH

You can connect to libvirt via SSH by specifying a [connection URI][libvirt-uri] such as the following:

```
qemu+libssh://[user]@[host]/system?keyfile=/etc/gar-virt/libvirt.key&sshauth=privkey&known_hosts=/etc/gar-virt/known_hosts
```

`ssh` uses different parameters so make sure to check the [connection URI][libvirt-uri] documentation.

Transport/Distro combinations confirmed to work with libvirt:

| Transport | Ubuntu 24.04 | Alpine 3.24 |
| --------- |:------------:|:-----------:|
| `libssh`  | ✓            | ✗           |
| `libssh2` | ✗            | ✓           |
| `ssh`     | ✓            | ✓           |

✓ works — ✗ doesn't work

Keep in mind that libvirt may have been compiled for your distro with or without certain support.

> [!NOTE]
> Adding `known_hosts_verify=ignore` (`libssh*`) or `no_verify=1` (`ssh`) to the connection URI disables host key verification and can be useful during troubleshooting. Do however consider keeping it enabled for security reasons.

Generate files needed for the SSH setup. Replace `[host]` with the same name used in your connection URI.

```sh
# Generate an SSH key
ssh-keygen -t ed25519 -f .config/libvirt.key -N '' -C 'gar-virt'
# Add the libvirt host's public keys to a new known_hosts file for host key verification
ssh-keyscan [host] > .config/known_hosts
# Add the public key to the authorized_keys file on the libvirt host
cat .config/libvirt.key.pub >> ~/.ssh/authorized_keys
```

## Docker Image

Use the `ubuntu` tag to build an image based on Ubuntu, or pick another variant such as `alpine`.

```sh
docker build --tag "gar-virt:ubuntu" --target dist --file "docker/ubuntu/Dockerfile" .
docker build --tag "gar-virt:alpine" --target dist --file "docker/alpine/Dockerfile" .
```

To avoid libvirt socket permission issues, container-to-host connection issues and not to mention reduce security risks, a secure remote connection is recommended.

When using SSH connections, you can avoid SSH key file permission issues by mounting your config directory at `/run/gar-virt-config` in the container. Doing so will copy the files to `/etc/gar-virt` and replace ownership of the copied files.

```sh
docker run --rm \
    --volume "$(pwd)/.config:/run/gar-virt-config:ro" \
    gar-virt:ubuntu --config-file /etc/gar-virt/config.yaml --verbose
```

### Docker Security Notes

The entrypoint of the Docker image will initially have root access in order to have permission to copy and change ownership of the config files from `/run/gar-virt-config`.

Root permissions are then dropped so that `gar-virt` itself runs as a non-root user (rootless).

[gitea-runner]: https://gitea.com/gitea/runner
[gitea-runner-patches]: https://github.com/gar-virt/gitea-runner
[libvirt]: https://libvirt.org
[libvirt-uri]: https://libvirt.org/uri.html
[runner-images]: https://github.com/gar-virt/runner-images
[virt-manager]: https://virt-manager.org
