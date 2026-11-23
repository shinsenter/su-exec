#!/bin/sh
BASE_DIR="$(git rev-parse --show-toplevel)"
TEST_DIR="$BASE_DIR/tests"
RESULTS="$BASE_DIR/test-results.txt"
trap 'kill 0' SIGINT

platforms=(
	linux/amd64
	linux/arm/v7
	linux/arm64/v8
)
joined="$(IFS=, ; echo "${platforms[*]}")"
source="shinsenter/su-exec:latest"

# keep only the platforms that the given image actually provides
supported () {
	local out p list=""
	out="$(docker buildx imagetools inspect "$1" 2>/dev/null)" || { echo "$joined"; return; }
	for p in "${platforms[@]}"; do
		if printf '%s\n' "$out" | grep -qE "^[[:space:]]*Platform:[[:space:]]+$p\$"; then
			list="${list:+$list,}$p"
		fi
	done
	echo "${list:-$joined}"
}

template () {
	local distro="$1"
	cat <<Dockerfile
FROM $distro AS test
COPY --from=$source --chown=root:root --chmod=0755 /su-exec /usr/sbin/su-exec
COPY --from=cases --chmod=0755 /test-cases.sh /test-cases.sh
RUN  if [ "\$(su-exec nobody whoami)" != "nobody" ]; then exit 1; fi
RUN  mkdir /output && su-exec nobody sh -c "whoami && id" >/output/result.txt
RUN  /test-cases.sh >/output/cases.txt 2>&1 || { cat /output/cases.txt; exit 1; }
FROM scratch
COPY --from=test /output/ /
Dockerfile
}

set -e
rm -rf "$TEST_DIR"
date >"$RESULTS"
while read distro; do
	mkdir -p "$TEST_DIR/$distro"
	tag="testing/su-exec-$distro"
	targets="$(supported "$distro")"
	template "$distro" | docker buildx build \
		-t "$tag" --platform "$targets" \
		--build-context cases="$BASE_DIR" \
		--output "$TEST_DIR/$distro" \
		-
	if [ "$?" -eq "0" ] ; then
		echo $distro OK | tee -a "$RESULTS"
	else
		echo $distro NG | tee -a "$RESULTS"
		exit 1
	fi
done << EOF
almalinux:latest
alpine:3.9
alpine:3.10
alpine:3.11
alpine:3.12
alpine:3.13
alpine:3.14
alpine:3.15
alpine:3.16
alpine:3.17
alpine:3.18
alpine:3.19
alpine:3.20
alpine:3.21
alpine:3.22
alpine:3.23
alpine:3.24
alpine:3
alpine:latest
alpine:edge
busybox:latest
debian:jessie
debian:stretch
debian:buster
debian:bullseye
debian:bookworm
debian:trixie
debian:forky
debian:sid
debian:latest
fedora:latest
rockylinux:latest
ubuntu:14.04
ubuntu:16.04
ubuntu:17.04
ubuntu:17.10
ubuntu:18.04
ubuntu:18.10
ubuntu:19.04
ubuntu:19.10
ubuntu:20.04
ubuntu:20.10
ubuntu:21.04
ubuntu:21.10
ubuntu:22.04
ubuntu:22.10
ubuntu:23.04
ubuntu:23.10
ubuntu:24.04
ubuntu:24.10
ubuntu:25.04
ubuntu:25.10
ubuntu:26.04
ubuntu:26.10
ubuntu:latest
ubuntu:rolling
EOF
