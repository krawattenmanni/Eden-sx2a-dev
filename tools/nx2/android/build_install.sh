#!/bin/bash
# Builds the RelWithDebInfo APK and installs it for the main user only.
set -e
. "$(dirname "$0")/env.sh"
cd "$REPO"
export ANDROID_HOME=$HOME/Library/Android/sdk JAVA_HOME=/Library/Java/JavaVirtualMachines/temurin-17.jdk/Contents/Home
sh .ci/android/build.sh -t standard -b RelWithDebInfo > build-android.log 2>&1 || { grep -m5 "error:" build-android.log; exit 1; }
$ADB install --user 0 -r artifacts/app-mainline-relWithDebInfo.apk
