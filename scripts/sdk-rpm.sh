#!/bin/sh
# SPDX-FileCopyrightText: 2026 Dennis Burgermeister
# SPDX-License-Identifier: GPL-3.0-or-later
#
# An app's RPM against one SDK target, the same way for dev:build,
# phone:build, check:harbour and release:deploy (docs/mise-tasks.md):
#
#   sdk-rpm.sh build <target>         RPMS/*.rpm, unit tests included via %check
#   sdk-rpm.sh check <target> <rpm>   Jolla's Harbour intake validation
#
# Runs from the app's root through mise: on the host with
# SAILFISH_SDK_PATH (bin/sfdk), in the development VM
# (IN_SAILFISH_SANDBOX=true) through the app's scripts/engine-exec.sh.

set -u

usage() { echo "usage: $0 build <target> | check <target> <rpm>" >&2; exit 2; }
[ $# -ge 2 ] || usage
command=$1
target=$2

sdk() {
    if [ "${IN_SAILFISH_SANDBOX:-}" = "true" ]; then
        [ -x ./scripts/engine-exec.sh ] || { echo "sdk-rpm.sh: run from the app root; IN_SAILFISH_SANDBOX needs scripts/engine-exec.sh" >&2; exit 2; }
        ./scripts/engine-exec.sh mb2 --target "$target" "$@"
    else
        "$SAILFISH_SDK_PATH/bin/sfdk" -c target="$target" "$@"
    fi
}

# sfdk configures in-source; these are its leftovers.
clean() {
    rm -rf CMakeCache.txt CMakeFiles Makefile cmake_install.cmake QtCreatorDeployment.txt
}

case $command in
    build)
        [ $# -eq 2 ] || usage
        clean
        sdk build
        status=$?
        # Cleaned up again, not just before the next run: the in-source
        # QtCreatorDeployment.txt names a binary in the project root that only
        # exists inside the build engine, and Qt Creator picks that file up for
        # its own Deploy/Run, which then fails with exit code 127.
        clean
        exit $status
        ;;
    check)
        [ $# -eq 3 ] || usage
        sdk check -s harbour "$3"
        ;;
    *) usage ;;
esac
