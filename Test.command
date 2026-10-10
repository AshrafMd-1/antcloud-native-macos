#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")"
antcloud_root="$PWD"
antcloud_qmake_args=()
python3 tools/test_fetch_sdk.py
if [[ "$(uname -s)" == Darwin ]]; then
  antcloud_qmake="$(brew --prefix qtbase)/bin/qmake"
  [[ -d src/libs/mac ]] || { echo 'Run python3 tools/fetch_sdk.py first.'; exit 1; }
  antcloud_qmake_args=(-after "QMAKE_MACOSX_DEPLOYMENT_TARGET=$(sw_vers -productVersion | cut -d. -f1).0")
else
  antcloud_qmake="$(command -v qmake6)"
fi
antcloud_run_test() {
  if [[ "$(uname -s)" == Darwin ]]; then
    # SDK install names target an app's Frameworks directory. Plain test binaries
    # need a process-local lookup override; do not modify the SDK or shell profile.
    DYLD_LIBRARY_PATH="$antcloud_root/src/libs/mac/lib" "$@"
  else
    "$@"
  fi
}
for antcloud_test in protocol input_stats tests controls seeker bandwidth microphone; do
  antcloud_folder="$antcloud_root/build/test-$antcloud_test"
  mkdir -p "$antcloud_folder"
  (cd "$antcloud_folder" && "$antcloud_qmake" "$antcloud_root/src/tests/antcloud/$antcloud_test.pro" "${antcloud_qmake_args[@]}" && make -j3 && SDL_VIDEODRIVER=dummy antcloud_run_test "./$antcloud_test")
done
if [[ "${1:-}" != --core-only ]]; then
  mkdir -p build/test-ui
  (cd build/test-ui && "$antcloud_qmake" "$antcloud_root/src/tests/antcloud/ui.pro" "${antcloud_qmake_args[@]}" && make -j3 && antcloud_run_test ./ui)
fi
echo 'Native tests passed. These tests do not contact the real AntCloud service or change Keychain.'
