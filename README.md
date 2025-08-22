# su-exec <!-- omit from toc -->

Switch user and group, then exec.

## Table of contents <!-- omit from toc -->
- [Introduction](#introduction)
- [Installation Using Docker](#installation-using-docker)
- [Usage](#usage)
- [Supported platforms](#supported-platforms)
- [Linux distribution compatibility](#linux-distribution-compatibility)


## Introduction

`su-exec` is a minimal re-write of
[`gosu`](https://github.com/tianon/gosu) in C, resulting in a smaller binary.

This simple tool executes a program with different privileges.
Unlike `su` and `sudo`, it runs the program directly, avoiding TTY and signal issues.

**Important:** `su-exec` requires root privileges to run.
Non-root users do not have permission to change uid/gid.

> ### Credits <!-- omit from toc -->
> This project is based on the inactive project [ncopa/su-exec](https://github.com/ncopa/su-exec)
and improvements from [songdongsheng/su-exec](https://github.com/songdongsheng/su-exec).
> Special thanks to them.

## Installation Using Docker
```Dockerfile
FROM <base-image>
COPY --from=shinsenter/su-exec:latest \
     --chown=root:root --chmod=4755 \
     /su-exec /usr/sbin/su-exec
```

## Usage
```shell
su-exec user-spec command [arguments...]
```

The `user-spec` can be either a username (e.g., `nobody`)
or a username and group name separated by a colon (e.g., `www-data:www-data`).
You can also use numeric uid/gid values.

### Examples: <!-- omit from toc -->
```shell
su-exec tianon bash
su-exec nobody:root bash -c 'whoami && id'
su-exec 1000:1 id
```

## Supported platforms
- linux/386
- linux/amd64
- linux/arm/v6
- linux/arm/v7
- linux/arm64/v8
- linux/ppc64le
- linux/riscv64
- linux/s390x

## Linux distribution compatibility
The `su-exec` in this repository has been tested on various Linux distributions.
See [test-results.txt](https://github.com/shinsenter/su-exec/blob/master/test-results.txt) for details.
