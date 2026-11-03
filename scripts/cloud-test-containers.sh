#!/bin/sh
# SPDX-FileCopyrightText: 2026 Dennis Burgermeister
# SPDX-License-Identifier: GPL-3.0-or-later
#
# The cloud:test:containers task (docs/releasing.md):
#
#   cloud-test-containers.sh <service>
#
# Builds every role from deploy/ (cloud:build or cloud:release:deploy
# first) and starts it as locked down as on d0b: read-only, no
# capabilities, no new privileges, UID 1000; volumes become throwaway
# tmpfs, the environment comes from Environment= only, never real secrets.
# Then the quadlet's HealthCmd must pass and the process must run as 1000.
# Runs from the app's root; needs Podman >= 5.

set -eu
[ $# -eq 1 ] || { echo "usage: $0 <service>" >&2; exit 2; }
service=$1
[ -f deploy/MANIFEST ] || { echo "no deploy/ - run cloud:build or cloud:release:deploy first" >&2; exit 1; }

set --
if [ -s containers/build.env ]; then
    while IFS= read -r l; do case $l in ''|'#'*) ;; *) set -- "$@" --build-arg "$l" ;; esac; done <containers/build.env
fi
rc=0
for q in containers/quadlets/"$service"-*.container; do
    r=${q##*/"$service"-}; r=${r%.container}
    name="t-$service-$r"
    podman build -q --pull=always "$@" -f "deploy/containers/$r.Containerfile" -t "localhost/$service-$r:test" deploy >/dev/null
    opts="--read-only --cap-drop=all --security-opt=no-new-privileges --userns=keep-id:uid=1000,gid=1000 --tmpfs /tmp"
    for v in $(sed -n 's/^Volume=//p' "$q"); do opts="$opts --tmpfs $(echo "$v" | cut -d: -f2):mode=1777"; done
    grep -qx 'RunInit=true' "$q" && opts="$opts --init"
    for e in $(sed -n 's/^Environment=//p' "$q"); do opts="$opts --env $e"; done
    podman rm -f "$name" >/dev/null 2>&1 || true
    # shellcheck disable=SC2086
    podman run -d --name "$name" $opts "localhost/$service-$r:test" >/dev/null
    hc=$(sed -n 's/^HealthCmd=//p' "$q")
    # Up to 60 s for the health check, like a slow first start on d0b.
    ok=
    for _ in $(seq 1 30); do
        # shellcheck disable=SC2086
        if podman exec "$name" $hc >/dev/null 2>&1; then ok=1; break; fi
        sleep 2
    done
    if [ -n "$ok" ] && [ "$(podman exec "$name" id -u)" = 1000 ]; then
        echo "ok     $r"
    else
        echo "FAILED $r"; podman logs --tail 20 "$name"; rc=1
    fi
    podman rm -f "$name" >/dev/null
done
exit $rc
