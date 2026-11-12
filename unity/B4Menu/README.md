# B4 Native VR Menu - Unity 6 package (v1.0.2)

A wrist menu for Quest, drawn and driven by the native `b4-native` C++ library (`libb4menu.so`).
This package is the Unity side: bindings, rendering, input, haptics and sound.

## Requirements
* Unity 6000.0 or newer
* Android, IL2CPP, ARM64 (Quest 2 / 3 / 3S / Pro)
* OpenXR (or another XR plugin that exposes `UnityEngine.XR.InputDevices`)
* `libb4menu.so` (arm64-v8a). The zip on the GitHub Releases page already contains it.
* The menu does not run in the Unity editor (the library is Android-only). Test on the headset.

## Install
1. Download `B4Menu-unity6-<version>.zip` from Releases and unzip it.
2. Copy the `B4Menu` folder into your project's `Packages/` folder (or `Assets/`).
3. Player Settings > Android: Scripting Backend = IL2CPP, Target Architectures = ARM64.
4. Create an empty GameObject and add **B4 > Menu Host**.
5. Assign `Left Hand` and `Right Hand` to your controller transforms
   (XR Origin controllers, or OVRCameraRig `LeftHandAnchor` / `RightHandAnchor`).
6. Build and run. Unity packs `libb4menu.so` into the APK at `lib/arm64-v8a/`.

If you installed from the repo source instead of a release, put `libb4menu.so` (from the Actions artifact)
in `Runtime/Plugins/Android/arm64-v8a/`. It is set to Android / ARM64 automatically.

## Using the menu in VR
* Press **Y** on the left controller to open or close it (change with `Toggle Button`).
* The menu floats above your **left** controller. Poke buttons with the red pointer sphere on your **right** controller.
* Home page: Settings, Favorites, Game Mods, Credits, Soundboard, Movement.
* Toggle entries show an LED; action entries fire once. Tap the star on an entry to favorite it.
* Use `<` / `>` at the bottom to change pages, **BACK** to return home, **CLOSE** on the home page to hide it.
* Settings (saved to `b4menu.cfg` in the app's persistent data folder): haptics, UI sound, animations, pointer,
  scale, tip size, tilt, touch speed. RESET restores defaults, CLEAR FAVS removes favorites.

## Adding your own entries
Do this from any script, in `Start()` (the host initialises earlier in `Awake()`).
```csharp
using B4;

var menu = B4MenuHost.Instance;
menu.RegisterToggle(1, "My toggle", B4Category.Game, false, on => Debug.Log(on));
menu.RegisterAction(2, "My action", B4Category.Movement, () => Debug.Log("fired"));
menu.RegisterAction(3, "My sound",  B4Category.Sound,    () => { /* play a clip */ });
```
* Ids are yours to choose and must be unique. Re-registering an id updates its name.
* Categories decide the page: `Game`, `Sound` (Soundboard), `Movement`.
* Names are upper-cased and cut to 31 characters. Maximum 64 entries, 4 per page.
* Entry state is not saved between launches, only settings and favorites are.

## API (`B4MenuHost`)
| Member | Purpose |
|---|---|
| `Instance`, `IsReady`, `IsVisible` | Access and state. `IsReady` is false in the editor or if the library failed to load. |
| `Show()`, `Hide()`, `Toggle()` | Open / close the menu. |
| `GoToPage(B4Page)` | Jump to a page. |
| `RegisterToggle`, `RegisterAction` | Add entries (see above). |
| `GetFeatureState(id)`, `SetFeatureState(id, on)` | Read / set a toggle's state. Setting does not fire the callback. |
| `GetSetting(B4Setting)`, `SetSetting(B4Setting, int)` | Menu settings. |
| Events | `VisibilityChanged`, `PageChanged`, `SettingChanged`, `HapticRequested`, `UiSoundRequested`. |

Inspector options: menu / pointer anchors, toggle button, XR tracking and haptics switches, shader override,
draw layer, volume and custom sounds (order: Open, Close, Select, Back, On, Off, Tick).

## Troubleshooting
* **`libb4menu.so could not be loaded` / DllNotFoundException**: the file is missing or not arm64. Make sure the
  player is ARM64 + IL2CPP. The CI build links the C++ runtime statically, so no extra `.so` is needed.
* **Menu never appears**: check the controller transforms are assigned, controllers are tracked, and look at the
  Y button mapping. Call `B4MenuHost.Instance.Show()` from code to test.
* **Pointer is off or hidden in the controller**: change `Pointer Anchor Position` (try a few cm on Z).
* **Menu is misplaced or tilted**: change `Menu Anchor Position / Euler`, or the in-menu TILT setting.
* **Pink or black menu**: the `B4/Menu` shader failed to compile; check the console. URP and Built-in are supported.
* **No haptics**: your XR setup may not expose `InputDevices` haptics. Turn off `Use Built In Haptics` and
  handle `HapticRequested` yourself.

## How it works
Each `LateUpdate` the host sends controller poses to the library, calls `B4_Update`, drains native events (haptics,
sounds, toggles, actions), calls `B4_BuildPacket`, uploads the returned vertices and indices to one dynamic `Mesh`
and draws it twice (menu and pointer) with the `B4/Menu` shader.

`B4Packet` in `Runtime/B4Native.cs` hard-codes byte offsets into `b4::RenderPacket` (checked with `offsetof`,
size 2,880,300). If you change `kMaxVerts`, `kMaxIndices` or the struct, update them.
