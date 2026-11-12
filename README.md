# b4-native

Native C++ VR wrist menu for Quest (Android arm64), with a Unity 6 package that draws and drives it.

## What it is
`libb4menu.so` holds all the menu logic: pages, buttons, touch detection, settings, favorites and geometry.
It has no renderer of its own. A host (the Unity package) feeds it controller poses each frame, then draws the mesh
it builds. The C API is in `src/b4_api.cpp`.

## Get it
Download `B4Menu-unity6-<version>.zip` from the [Releases](../../releases) page. It is the Unity package with
`libb4menu.so` already inside. The raw `libb4menu.so` is attached to each release too.

## Use in Unity 6
Unzip, copy the `B4Menu` folder into your project's `Packages/`, add **B4 > Menu Host** to a GameObject and assign
your two controller transforms. Press Y on the left controller to open the menu.
See `unity/B4Menu/README.md` for the full guide, the C# API and troubleshooting.

## Build and release (GitHub Actions)
The **Build and release** workflow compiles the library (arm64-v8a, Android API 29, static STL, 16 KB pages),
checks that it is arm64 with all 18 `B4_` exports, bundles it into the Unity package and publishes a release.
* Actions tab > Build and release > Run workflow, enter the version (e.g. `1.0.2`), keep "Publish a GitHub release" on.
* Or push a tag: `git tag v1.0.2 && git push origin v1.0.2`.

Local build:
```
cmake -B build -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE=$ANDROID_NDK/build/cmake/android.toolchain.cmake \
  -DANDROID_ABI=arm64-v8a -DANDROID_PLATFORM=android-29 \
  -DANDROID_STL=c++_static -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

## Layout
* `src/b4_menu.*` menu logic and geometry
* `src/b4_api.cpp` C exports for P/Invoke
* `src/b4_mesh.h`, `b4_font.*`, `b4_math.h` mesh builder, bitmap font, math helpers
* `unity/B4Menu` the Unity 6 package source
