# Steffen's Experimental Gitea Runner

An experimental Gitea Runner made from scratch in C++.

## Goals

- Use Gitea and Gitea Actions instead of GitHub and GitHub Actions.
- Use my own self-hosted runner.
- Spawn a new environment for every workflow task:
  - Build in Linux Docker containers.
  - Build in Windows virtual machines using QEMU/KVM.
  - Explore building in Windows containers.
  - Maybe build in macOS virtual machines using QEMU/KVM (note: Hackintosh is on its last breath with Tahoe 26).

## Is it usable yet?

The more I work on this, the more I realize how far I am from the finish line.

Some simple workflows may work, but don't expect much right now.

What does work:

- Simple workflow expressions.
- Functions usable in expressions:
  - `contains()`
  - `startsWith()`
  - `endsWith()`
- Conditional steps.
- Spawning Docker containers.
- Contexts (`gitea`/`github`, `runner`, `secrets`, `vars`) are mostly available but not everything within.
- `GITHUB_*` environment variables are mostly available but some are missing.
- Reports command output back to Gitea.
- So far the `actions/checkout` action works for cloning the repository.

Known to not work yet:
- `stderr` redirection seems to have a problem at the moment and may not be reported back to Gitea.
- Overriding working directory in workflow yaml.
- Defaults in workflow yaml.
- Job status in context and environment variable.
- Cleanup after executing action such as `actions/checkout`.
- Commands printed to `stdout`.
- Setting environment variables via `GITHUB_ENV`.
- Setting and retrieving outputs (`needs`).
- Action inputs (`INPUT_*` environment variables).
- Action state (`STATE_*` environment variables).
- No handling of `ephemeral` setting yet so keep it at `false` for now.
- `jobs.<job_id>.container`.
- Probably many more things.

## Building

Supported platform: Linux

See `Dockerfile` for detailed build instructions.

```
sudo docker build -t ga-runner .
```

Note: Root access is currently needed when running this in a Docker container because it needs to manage Docker.

## Usage

See CLI usage:

```
sudo docker run --rm -it ga-runner --help
```

Register runner:
```
sudo docker run \
  --volume "${PWD}/.config/runner.config.json:/etc/ga_runner/runner.config.json" \
  --volume "${PWD}/.config/runner.state.json:/var/run/ga_runner/runner.state.json" \
  --rm -it ga-runner \
  --config-file /etc/ga_runner/runner.config.json \
  --state-file /var/run/ga_runner/runner.state.json \
  register
```

Run daemon:
```
sudo docker run \
  --volume "${PWD}/.config/runner.config.json:/etc/ga_runner/runner.config.json" \
  --volume "${PWD}/.config/runner.state.json:/var/run/ga_runner/runner.state.json" \
  --volume /var/run/docker.sock:/var/run/docker.sock \
  --privileged --rm -it ga-runner \
  --config-file /etc/ga_runner/runner.config.json \
  --state-file /var/run/ga_runner/runner.state.json \
  daemon
```

Note: Providing a usable Docker image isn't currently a priority. It's mainly there to document the build process and runtime dependencies. Limited testing has however shown that it works for running builds in Docker containers.

## Sample config

This format is a work-in-progress and may change. `qemu` isn't implemented yet.

```json
{
  "instance_url": "[gitea instance url]",
  "name": "[runner name]",
  "token": "[token]",
  "ephemeral": false,
  "environments": {
    "docker": {
      "labels": [
        "ubuntu-latest",
        "ubuntu-24.04",
        "linux"
      ],
      "os": "Linux",
      "arch": "X64",
      "temp_dir": "/tmp",
      "workspaces_dir": "/w",
      "details": {
        "image": "ga-runner-ubuntu",
        "tag": "24.04"
      }
    },
    "qemu": {
      "labels": [
        "windows-latest",
        "windows-2025",
        "windows"
      ],
      "os": "Windows",
      "arch": "X64",
      "temp_dir": "C:\\tmp",
      "workspaces_dir": "C:\\w",
      "details": {
        "image": "path/to/image.qcow2",
        "cpu": "host",
        "memory": 4294967296
      }
    }
  }
}
```

## History

I didn't want to continue paying for a few more GitHub Actions build minutes that would evaporate within a few days due to slow hosted runners in combination with high running costs.

I've used AppVeyor successfully in the past to run builds in Windows virtual machines, but I wanted something with less strings attached this time, so I tried some popular self-hosted Git hosting and CI/CD solutions such as Drone, Forgejo, Gitea, GitLab and Woodpecker.

GitLab could have been a good solution with a bit of work, but it would have needed a hardware upgrade. One of the main points of my effort was and continues to be to plug the little holes that drain my money.

After some testing and back-and-forth leading to an increasing amount of disappointment, I settled for Gitea because of its seamless integration with Gitea Actions.

To be able to build on all of the major platforms (Linux, macOS, Windows) isn't an unreasonable requirement, but neither Gitea's act_runner nor ACT supported building my projects in Windows containers or in fresh virtual machines running on my own hardware.

To attempt solving my problem the "easy" way, I built a "controller" that received webhooks from Gitea, and based on labels it would notify a matching "agent" to spin up a fresh environment. For example, the agent could spin up a new Windows virtual machine where act_runner would do the rest of the work in "host" mode, but in isolation.

For these quick projects I wanted to try Deno instead of Node.js, using Express.js, websockets and TypeScript. Deno was surprisingly pleasant to work with compared to Node.js and NPM, although some libraries weren't fully compatible. While this solution worked quite well in the "happy path", I felt that Gitea and act_runner stumbled here and there in "unhappy paths", and I wanted more control over the situation.

At this point, the smart thing to do might have been to extend act_runner or find some readily available libraries to help me build a custom runner for Gitea. All the juicy material was however written in Go, and I have some history with fighting against Go/cgo tooling which has left me somewhat bitter.

Instead I decided to try writing a runner in C++, and try a few new and different things while at it, such as CMake 4.x, C++23, change up my usual code style, etc.

I just wanted to move away from GitHub and GitHub Actions, so I wonder what I'm even doing with this project.

That's how I started my adventure writing this thing in C++, and now I'm deep into this rabbit hole while finding out how much more work there's left to do.
