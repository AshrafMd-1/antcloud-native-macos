# Publish AntCloud Native on GitHub

## Repository details

Recommended repository name: `antcloud-native-macos`

About / description:

> Unofficial native macOS client for AntCloud Windows VMs, built on Moonlight.

Suggested topics: `macos`, `moonlight`, `remote-desktop`, `cloud-gaming`, `antcloud`.

Keep the repository public only if you are comfortable publishing the source and its history. This client uses AntCloud's observed provider workflow; it is not an official AntCloud integration. Preserve `LICENSE`, `NOTICE.md`, `src/LICENSE` and upstream notices.

## Create the repository

1. Download and unzip the source package into a fresh `AntCloudNative` folder. The package excludes the local `.git` history, build output, SDK binaries and credentials.
2. On GitHub, create a repository named `antcloud-native-macos`. Do not initialize it with a README, license or `.gitignore`; those files are already in this source folder.
3. In Terminal, run these commands from the extracted folder, replacing `YOUR-USERNAME` with your GitHub username:

```bash
cd ~/Downloads/AntCloudNative
git init
git add .
git status --short
git commit -m "Prepare AntCloud Native 1.0.3 source release"
git branch -M main
git remote add origin https://github.com/YOUR-USERNAME/antcloud-native-macos.git
git push -u origin main
```

Review `git status --short` before committing. It should not list `build/`, `dist/`, `src/libs/`, `.env` files, credentials, diagnostic JSONL/HAR files, or your local account data.

## Publish one v1.0.3 release

First build the app on your Mac and verify Stop → reconnect and Shutdown → reconnect in the same app. The source package's automated tests and Linux build cannot verify the live Mac/provider behavior.

Create the version tag and push it:

```bash
git tag -a v1.0.3 -m "AntCloud Native 1.0.3"
git push origin v1.0.3
```

Then on GitHub:

1. Open the repository's **Releases** page and select **Draft a new release**.
2. Choose the existing `v1.0.3` tag. Set the title to `AntCloud Native 1.0.3`.
3. Copy the notes from [github-release-1.0.3.md](github-release-1.0.3.md) into the release description.
4. Optionally attach `AntCloudNative-source.zip` for people who prefer a ZIP download. GitHub also generates source ZIP and tarball links from the tag automatically.
5. Publish the release once. Do not reuse the `v1.0.3` tag for later code changes; create a new patch version for a follow-up release.

This is a source release, not a signed/notarized Mac binary. Users build the app on their Mac with `Build-Mac.command`.
