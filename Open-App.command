#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")"
[[ -d dist/AntCloudNative.app ]] || { echo 'Run bash Build-Mac.command first.'; exit 1; }
open "$PWD/dist/AntCloudNative.app"
