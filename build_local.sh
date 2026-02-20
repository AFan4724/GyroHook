#!/data/data/com.termux/files/usr/bin/bash
# GyroHook 本地构建环境安装 + 编译脚本
# 请在普通 Termux 环境下运行（非 root / 非 tsu）

set -e

echo "=== [1/4] 安装 JDK 17 ==="
pkg install openjdk-17 -y

echo "=== [2/4] 安装 Android SDK cmdline-tools ==="
mkdir -p ~/android-sdk/cmdline-tools
cd /tmp
curl -LO https://dl.google.com/android/repository/commandlinetools-linux-11076708_latest.zip
unzip -o commandlinetools-linux-11076708_latest.zip -d ~/android-sdk/cmdline-tools
mv ~/android-sdk/cmdline-tools/cmdline-tools ~/android-sdk/cmdline-tools/latest 2>/dev/null || true
rm -f commandlinetools-linux-11076708_latest.zip

export ANDROID_HOME=~/android-sdk
export PATH=$ANDROID_HOME/cmdline-tools/latest/bin:$ANDROID_HOME/platform-tools:$PATH

echo "=== [3/4] 安装 SDK 组件 ==="
yes | sdkmanager --licenses 2>/dev/null || true
sdkmanager "platforms;android-35" "build-tools;35.0.0" "platform-tools"

# 写入环境变量
grep -q "ANDROID_HOME" ~/.bashrc 2>/dev/null || cat >> ~/.bashrc << 'ENVEOF'

# Android SDK
export ANDROID_HOME=~/android-sdk
export PATH=$ANDROID_HOME/cmdline-tools/latest/bin:$ANDROID_HOME/platform-tools:$PATH
ENVEOF

echo "=== [4/4] 构建 GyroHook ==="
cd ~/GyroHook
chmod +x gradlew
./gradlew assembleRelease

echo ""
echo "=== 构建完成 ==="
ls -lh ~/GyroHook/app/build/outputs/apk/release/*.apk 2>/dev/null
