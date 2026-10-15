#!/bin/sh
# Prepares an ARM64 (aarch64) Linux host to build Android artifacts.
#
# What it does:
#   1. installs native build tools (clang, cmake, ninja, openjdk)
#   2. downloads the Android SDK command line tools and installs
#      platform-tools, platform 35, build-tools 35 and the NDK
#   3. makes the x86_64-only AAPT2 packaging tool runnable on ARM64
#      through QEMU plus a minimal glibc runtime
#
# AAPT2 is a *host* packaging tool only. Target code is always compiled by the
# native ARM64 clang into arm64-v8a; nothing of the APK's own code is emulated.
#
# Usage: tools/setup_host_arm64.sh [sdk_dir]
set -eu

SDK=${1:-/var/minis/workspace/android-sdk}
GLIBC=/tmp/glibc
TOOLS=/var/minis/workspace/android-tools/bin
BT_VERSION=35.0.0

apk add --no-cache openjdk17-jdk gradle cmake ninja-build clang lld llvm zip unzip qemu-x86_64 >/dev/null

mkdir -p "$SDK/cmdline-tools"
if [ ! -x "$SDK/cmdline-tools/latest/bin/sdkmanager" ]; then
  cd "$SDK"
  wget -q -O cmdline-tools.zip \
    https://dl.google.com/android/repository/commandlinetools-linux-11076708_latest.zip
  rm -rf cmdline-tools/latest
  unzip -q -o cmdline-tools.zip -d cmdline-tools
  mv cmdline-tools/cmdline-tools cmdline-tools/latest
  rm -f cmdline-tools.zip
fi

unset HTTP_PROXY HTTPS_PROXY ALL_PROXY http_proxy https_proxy all_proxy
export ANDROID_HOME="$SDK" ANDROID_SDK_ROOT="$SDK"
yes | "$SDK/cmdline-tools/latest/bin/sdkmanager" --licenses >/dev/null 2>&1 || true
"$SDK/cmdline-tools/latest/bin/sdkmanager" \
  "platform-tools" "platforms;android-35" "build-tools;$BT_VERSION" \
  "ndk;27.0.12077973"

# Minimal x86_64 glibc runtime for the one x86-only host tool.
mkdir -p "$GLIBC/lib/x86_64-linux-gnu" "$GLIBC/lib64"
INDEX=https://deb.debian.org/debian/dists/bookworm/main/binary-amd64/Packages.gz
fetch_glibc_pkg() {
  path=$(curl -4 -fsSL --max-time 120 "$INDEX" | gzip -dc | python3 - "$1" <<'PY'
import re, sys
name = sys.argv[1]
s = sys.stdin.read()
m = re.search(r"Package: " + re.escape(name) + r"\n.*?Filename: (.*?amd64\.deb)", s, re.S)
print(m.group(1) if m else "")
PY
)
  [ -n "$path" ] || return 0
  out="$GLIBC/$1.deb"
  curl -4 -fsSL --max-time 300 -C - "https://deb.debian.org/debian/$path" -o "$out"
  ar p "$out" data.tar.xz | tar -xJf - -C "$GLIBC"
}
fetch_glibc_pkg libc6
fetch_glibc_pkg libgcc-s1
fetch_glibc_pkg zlib1g
fetch_glibc_pkg libstdc++6
ln -sf ../lib/x86_64-linux-gnu/ld-linux-x86-64.so.2 "$GLIBC/lib64/ld-linux-x86-64.so.2"

mkdir -p "$TOOLS"
cp "$SDK/build-tools/$BT_VERSION/aapt2" "$TOOLS/aapt2.x86_64" 2>/dev/null || true
cat > "$TOOLS/aapt2" <<EOF
#!/bin/sh
exec /usr/bin/qemu-x86_64 -L $GLIBC $TOOLS/aapt2.x86_64 "\$@"
EOF
chmod 755 "$TOOLS/aapt2" "$TOOLS/aapt2.x86_64"
"$TOOLS/aapt2" version

echo
echo "host ready; build with:"
echo "  ANDROID_SDK_ROOT=$SDK tools/build_arm64.sh"
