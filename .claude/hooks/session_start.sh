#!/bin/sh
# Installs the LÖVE toolchain in cloud sessions only: a developer's own
# machine is never touched by apt-get.

set -eu

[ "${CLAUDE_CODE_REMOTE:-}" = true ] || exit 0
exec sh "$CLAUDE_PROJECT_DIR/tools/love_install.sh"
