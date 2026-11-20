#!/usr/bin/env bash
source "$(dirname "${BASH_SOURCE[0]}")/common.sh"
"$UE_ROOT/Engine/Build/BatchFiles/Linux/Build.sh" "${PROJECT_NAME}Editor" Linux Development -project="$PROJECT_FILE" -waitmutex "$@"
