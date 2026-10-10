#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")"
[[ "$(uname -s)" == Darwin ]] || { echo 'Build this on your Mac.'; exit 1; }
xcrun --find clang >/dev/null 2>&1 || { echo 'Run xcode-select --install, finish installation, then retry.'; exit 1; }
command -v brew >/dev/null 2>&1 || { echo 'Homebrew is required for build tools: https://brew.sh/'; exit 1; }
command -v python3 >/dev/null 2>&1 || { echo 'Python 3.10+ is required for the build only.'; exit 1; }
python3 -c 'import sys; sys.exit(0 if sys.version_info >= (3,10) else "Python 3.10+ is required.")'
# Reuse installed modules. Do not start an upgrade on every app build.
for antcloud_module in qtbase qtdeclarative qtsvg qttools qtwebsockets; do
  antcloud_installed="$(brew list --versions --formula "$antcloud_module" 2>/dev/null || true)"
  if [[ -n "$antcloud_installed" ]]; then
    echo "Reusing $antcloud_installed"
  else
    HOMEBREW_NO_AUTO_UPDATE=1 HOMEBREW_NO_INSTALL_UPGRADE=1 brew install --formula "$antcloud_module"
  fi
done
antcloud_qmake="$(brew --prefix qtbase)/bin/qmake"
antcloud_version="$(cat src/app/antcloud/version.txt)"
# Target the build host's major version because Homebrew Qt bottles may require it.
antcloud_macos_target="$(sw_vers -productVersion | cut -d. -f1).0"
antcloud_deploy=''
for antcloud_module in qttools qtbase; do
  antcloud_candidate="$(brew --prefix "$antcloud_module")/bin/macdeployqt"
  if [[ -x "$antcloud_candidate" ]]; then antcloud_deploy="$antcloud_candidate"; break; fi
done
[[ -x "$antcloud_qmake" && -x "$antcloud_deploy" ]] || { echo 'Qt qmake/macdeployqt is missing.'; exit 1; }
python3 - "$antcloud_qmake" <<'PY'
import subprocess,sys
v=subprocess.check_output([sys.argv[1],'-query','QT_VERSION'],text=True).strip()
parts=tuple(map(int,v.split('.')[:2]))
if parts[0]!=6 or parts<(6,11):
    raise SystemExit('Qt 6.11+ is required. Upgrade qtbase qtdeclarative qtsvg qttools qtwebsockets together.')
print('Building with Qt',v)
PY
python3 tools/fetch_sdk.py
bash Test.command --core-only
antcloud_build="$PWD/build/mac-$(uname -m)"
mkdir -p "$antcloud_build" dist
(
  cd "$antcloud_build"
  "$antcloud_qmake" "$PWD/../../src/moonlight-qt.pro" QMAKE_APPLE_DEVICE_ARCHS="$(uname -m)" -after QMAKE_MACOSX_DEPLOYMENT_TARGET="$antcloud_macos_target"
  make qmake_all
  # qmake's bundle plist rule has no template prerequisite on incremental builds.
  rm -f app/Moonlight.app/Contents/Info.plist
  make -j4 release
)
"$antcloud_deploy" "$antcloud_build/app/Moonlight.app" -qmldir="$PWD/src/app/gui" -no-codesign
antcloud_staged="$PWD/dist/AntCloudNative-new.app"
[[ ! -e "$antcloud_staged" && ! -L "$antcloud_staged" ]] || { echo 'Rename the unfinished dist/AntCloudNative-new.app, then retry.'; exit 1; }
for antcloud_owned in dist/AntCloudNative.app dist/AntCloudNative-previous.app; do
  if [[ -L "$antcloud_owned" || ( -e "$antcloud_owned" && ! -d "$antcloud_owned" ) ]]; then
    echo "Rename $antcloud_owned: generated app destinations must be real directories."
    exit 1
  fi
done
ditto "$antcloud_build/app/Moonlight.app" "$antcloud_staged"
iconutil -c icns assets/AntCloudNative.iconset -o "$antcloud_build/AntCloudNative.icns"
cp "$antcloud_build/AntCloudNative.icns" "$antcloud_staged/Contents/Resources/AntCloudNative.icns"
for antcloud_key in CFBundleName CFBundleDisplayName CFBundleIconFile; do
  /usr/libexec/PlistBuddy -c "Set :$antcloud_key AntCloudNative" "$antcloud_staged/Contents/Info.plist" 2>/dev/null || /usr/libexec/PlistBuddy -c "Add :$antcloud_key string AntCloudNative" "$antcloud_staged/Contents/Info.plist"
done
/usr/libexec/PlistBuddy -c "Set :CFBundleShortVersionString $antcloud_version" "$antcloud_staged/Contents/Info.plist"
/usr/libexec/PlistBuddy -c "Set :CFBundleVersion $antcloud_version" "$antcloud_staged/Contents/Info.plist"
/usr/libexec/PlistBuddy -c "Set :LSMinimumSystemVersion $antcloud_macos_target" "$antcloud_staged/Contents/Info.plist"
python3 - "$antcloud_staged/Contents/Info.plist" "$PWD/src/app/Info.plist" <<'PY'
import plistlib, sys
with open(sys.argv[1], 'rb') as file:
    bundled = plistlib.load(file)
with open(sys.argv[2], 'rb') as file:
    source = plistlib.load(file)
key = 'NSMicrophoneUsageDescription'
if not source.get(key) or bundled.get(key) != source[key]:
    raise SystemExit('Packaged microphone usage description is missing or stale; the staged app was not installed.')
PY
codesign --force --deep --sign - "$antcloud_staged"
codesign --verify --deep --strict --verbose=2 "$antcloud_staged"
"$antcloud_staged/Contents/MacOS/Moonlight" --help >/dev/null
antcloud_built_version="$("$antcloud_staged/Contents/MacOS/Moonlight" --version)"
[[ "$antcloud_built_version" == "AntCloudNative $antcloud_version" ]] || { echo 'The built app version does not match the native release version. The installed app was not replaced.'; exit 1; }
# Keep one previous generated app; never touch the installed app in Applications.
if [[ -e dist/AntCloudNative.app ]]; then
  if [[ -d dist/AntCloudNative-previous.app && ! -L dist/AntCloudNative-previous.app ]]; then
    rm -rf dist/AntCloudNative-previous.app
  fi
  mv dist/AntCloudNative.app dist/AntCloudNative-previous.app
fi
mv "$antcloud_staged" dist/AntCloudNative.app
echo 'Built dist/AntCloudNative.app. Open it or drag it into Applications.'
echo 'Use the app for sign-in and streaming. Terminal can now be closed.'
