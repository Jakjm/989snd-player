# 989snd Player

## Build & Run (Linux)

### Packages (Arch)

```sh
sudo pacman -S --needed base-devel cmake ninja mesa \
  libx11 libxext libxrandr libxcursor libxi libxfixes libxss libxtst \
  wayland wayland-protocols libxkbcommon alsa-lib libpulse dbus
```

### Packages (Ubuntu)

```sh
sudo apt install build-essential cmake ninja-build pkg-config \
  libgl1-mesa-dev libx11-dev libxext-dev libxrandr-dev libxcursor-dev \
  libxi-dev libxfixes-dev libxss-dev libxtst-dev libwayland-dev \
  wayland-protocols libxkbcommon-dev libasound2-dev libpulse-dev \
  libdbus-1-dev libudev-dev
```

```sh
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build --target sndplayer
./build/sndplayer
```

## Build & Run (Windows)

```sh
cmake -B build
cmake --build build --config Release --target sndplayer
build\Release\sndplayer.exe
```

## Build & Run (Android)

```sh
cd android
./gradlew assembleDebug
# APK: android/app/build/outputs/apk/debug/app-debug.apk
```
