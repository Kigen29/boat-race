#!/usr/bin/env bash
set -euo pipefail
REPO="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
git -C "$REPO" rev-parse --show-toplevel
echo "Use the existing monorepo; do not initialise a repository inside a game."
