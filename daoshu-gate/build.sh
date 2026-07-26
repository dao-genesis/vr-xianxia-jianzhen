#!/bin/bash
# Manual APK build: aapt2 + javac + d8 + zipalign + apksigner (no Gradle needed)
set -e
SDK=${ANDROID_SDK:-$HOME/android-sdk}
BT=$SDK/build-tools/34.0.0
PLATFORM=$SDK/platforms/android-34/android.jar
cd "$(dirname "$0")"
rm -rf build && mkdir -p build/classes

"$BT/aapt2" link -o build/base.apk --manifest AndroidManifest.xml -I "$PLATFORM"

javac -source 8 -target 8 -bootclasspath "$PLATFORM" -d build/classes $(find src -name '*.java')
"$BT/d8" --release --lib "$PLATFORM" --output build $(find build/classes -name '*.class')

cd build && zip -qj base.apk classes.dex && cd ..
"$BT/zipalign" -f 4 build/base.apk build/aligned.apk

KS=build/debug.keystore
[ -f "$KS" ] || keytool -genkeypair -keystore "$KS" -storepass daoshu123 -keypass daoshu123 \
  -alias daoshu -keyalg RSA -keysize 2048 -validity 10000 -dname "CN=daoshu" >/dev/null 2>&1
"$BT/apksigner" sign --ks "$KS" --ks-pass pass:daoshu123 --out build/daoshu-gate.apk build/aligned.apk
ls -la build/daoshu-gate.apk
