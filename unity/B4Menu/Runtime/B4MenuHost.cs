using System;
using System.Collections.Generic;
using Unity.Collections;
using Unity.Collections.LowLevel.Unsafe;
using UnityEngine;
using UnityEngine.Rendering;
using UnityEngine.XR;

namespace B4
{
    public enum B4ToggleButton { None, LeftSecondary_Y, LeftPrimary_X, LeftMenu }

    /// <summary>
    /// Drives libb4menu: feeds controller poses, pumps native events (haptics, sounds, feature callbacks)
    /// and draws the menu mesh each frame. Add one to the scene and assign the two controller transforms.
    /// </summary>
    [DefaultExecutionOrder(-1000)]
    [AddComponentMenu("B4/Menu Host")]
    public sealed unsafe class B4MenuHost : MonoBehaviour
    {
        public static B4MenuHost Instance { get; private set; }

        [Header("Controllers (world-space transforms, e.g. XR Origin / OVRCameraRig hand anchors)")]
        public Transform leftHand;   // carries the menu
        public Transform rightHand;  // pokes the buttons

        [Header("Placement (local to the controller)")]
        public Vector3 menuAnchorPosition = new Vector3(0f, 0.05f, 0.10f);
        public Vector3 menuAnchorEuler = Vector3.zero;
        public Vector3 pointerAnchorPosition = Vector3.zero;

        [Header("Input")]
        public B4ToggleButton toggleButton = B4ToggleButton.LeftSecondary_Y;
        [Tooltip("Use UnityEngine.XR.InputDevices for tracking state. Off = tracked while the transform is active.")]
        public bool useXRInputTracking = true;
        public bool useBuiltInHaptics = true;

        [Header("Rendering")]
        public Shader shaderOverride;
        public int drawLayer = 0;

        [Header("Audio")]
        [Range(0f, 1f)] public float volume = 0.6f;
        [Tooltip("Optional replacements, indexed by B4Sound (Open, Close, Select, Back, On, Off, Tick).")]
        public AudioClip[] customSounds = new AudioClip[7];

        public event Action<bool> VisibilityChanged;
        public event Action<B4Page, int> PageChanged;
        public event Action<B4Setting, int> SettingChanged;
        /// <summary>hand (0 left, 1 right), amplitude 0-1, duration in seconds.</summary>
        public event Action<int, float, float> HapticRequested;
        public event Action<B4Sound> UiSoundRequested;

        public bool IsReady => _ready;
        public bool IsVisible => _ready && B4Native.B4_IsVisible() != 0;

        bool _ready;
        bool _prevToggleDown;
        bool _warnedShader;
        Mesh _mesh;
        Material _material;
        AudioSource _audio;
        AudioClip[] _clips;
        readonly float[] _vp = new float[32];
        readonly Dictionary<int, Action<bool>> _toggles = new Dictionary<int, Action<bool>>();
        readonly Dictionary<int, Action> _actions = new Dictionary<int, Action>();

        // ------------------------------------------------------------------ lifecycle
        void Awake()
        {
            if (Instance != null && Instance != this) { Destroy(this); return; }
            Instance = this;
            if (transform.parent == null) DontDestroyOnLoad(gameObject);

#if UNITY_ANDROID && !UNITY_EDITOR
            try
            {
                if (B4Native.B4_Init(Application.persistentDataPath) == 0) { Debug.LogError("[B4] B4_Init failed"); return; }
            }
            catch (Exception e) when (e is DllNotFoundException || e is EntryPointNotFoundException)
            {
                Debug.LogError("[B4] libb4menu.so could not be loaded (" + e.Message + "). Is it in Runtime/Plugins/Android/arm64-v8a, " +
                               "built with a static STL (or with libc++_shared.so bundled), and is the player ARM64 + IL2CPP?");
                return;
            }
            B4Native.B4_SetBackendName("UNITY " + SystemInfo.graphicsDeviceType.ToString().ToUpperInvariant());
            B4Native.B4_SetLinearColor(QualitySettings.activeColorSpace == ColorSpace.Linear ? 1 : 0);
            _ready = true;
#else
            Debug.LogWarning("[B4] libb4menu is Android arm64 only - menu is inactive in the editor / this platform.");
            return;
#endif
            BuildMesh();
            BuildAudio();
        }

        void OnDestroy()
        {
            if (Instance != this) return;
            if (_ready) B4Native.B4_Shutdown();
            _ready = false;
            Instance = null;
            if (_mesh != null) Destroy(_mesh);
            if (_material != null) Destroy(_material);
        }

        void LateUpdate()
        {
            if (!_ready) return;
            PollToggleButton();
            B4Native.B4_SetMenuAnchor(menuAnchorPosition.x, menuAnchorPosition.y, menuAnchorPosition.z,
                                      menuAnchorEuler.x, menuAnchorEuler.y, menuAnchorEuler.z);
            B4Native.B4_SetPointerAnchor(pointerAnchorPosition.x, pointerAnchorPosition.y, pointerAnchorPosition.z);
            PushPose(0, leftHand, XRNode.LeftHand);
            PushPose(1, rightHand, XRNode.RightHand);
            B4Native.B4_Update(Time.unscaledDeltaTime);
            PumpEvents();

            IntPtr pkt = B4Native.B4_BuildPacket(_vp, Screen.width, Screen.height,
                                                 Mathf.Max(1, QualitySettings.antiAliasing),
                                                 SystemInfo.usesReversedZBuffer ? 1 : 0);
            if (pkt != IntPtr.Zero) Draw(pkt);
        }

        // ------------------------------------------------------------------ public API
        public void Show() { if (_ready) B4Native.B4_SetVisible(1); }
        public void Hide() { if (_ready) B4Native.B4_SetVisible(0); }
        public void Toggle() { if (_ready) B4Native.B4_SetVisible(IsVisible ? 0 : 1); }
        public void GoToPage(B4Page page) { if (_ready) B4Native.B4_GoToPage((int)page); }

        /// <summary>Adds (or updates) an on/off entry. The callback fires when the user toggles it in the menu.</summary>
        public bool RegisterToggle(int id, string name, B4Category category, bool defaultState, Action<bool> onChanged)
        {
            if (!_ready) return false;
            if (onChanged != null) _toggles[id] = onChanged; else _toggles.Remove(id);
            return B4Native.B4_RegisterFeature(id, Label(name), (int)category, 0, defaultState ? 1 : 0) != 0;
        }

        /// <summary>Adds (or updates) a one-shot entry. The callback fires when the user presses it.</summary>
        public bool RegisterAction(int id, string name, B4Category category, Action onTriggered)
        {
            if (!_ready) return false;
            if (onTriggered != null) _actions[id] = onTriggered; else _actions.Remove(id);
            return B4Native.B4_RegisterFeature(id, Label(name), (int)category, 1, 0) != 0;
        }

        /// <summary>-1 if the id is unknown. Does not raise the toggle callback.</summary>
        public int GetFeatureState(int id) => _ready ? B4Native.B4_GetFeatureState(id) : -1;
        /// <summary>Updates the menu LED without raising the toggle callback.</summary>
        public void SetFeatureState(int id, bool on) { if (_ready) B4Native.B4_SetFeatureState(id, on ? 1 : 0); }
        public int GetSetting(B4Setting s) => _ready ? B4Native.B4_GetSetting((int)s) : 0;
        public void SetSetting(B4Setting s, int value) { if (_ready) B4Native.B4_SetSetting((int)s, value); }

        // The built-in font is upper-case only and labels are limited to 31 characters.
        static string Label(string name)
        {
            name = (name ?? string.Empty).ToUpperInvariant();
            return name.Length > 31 ? name.Substring(0, 31) : name;
        }

        // ------------------------------------------------------------------ input
        void PushPose(int hand, Transform t, XRNode node)
        {
            if (t == null) { B4Native.B4_SetControllerPose(hand, 0, 0, 0, 0, 0, 0, 0, 1); return; }
            bool tracked = t.gameObject.activeInHierarchy;
            if (tracked && useXRInputTracking)
            {
                var d = InputDevices.GetDeviceAtXRNode(node);
                tracked = d.isValid && d.TryGetFeatureValue(CommonUsages.isTracked, out bool tr) && tr;
            }
            Vector3 p = t.position;
            Quaternion q = t.rotation;
            B4Native.B4_SetControllerPose(hand, tracked ? 1 : 0, p.x, p.y, p.z, q.x, q.y, q.z, q.w);
        }

        void PollToggleButton()
        {
            if (toggleButton == B4ToggleButton.None) return;
            var d = InputDevices.GetDeviceAtXRNode(XRNode.LeftHand);
            if (!d.isValid) return;
            InputFeatureUsage<bool> usage;
            switch (toggleButton)
            {
                case B4ToggleButton.LeftPrimary_X: usage = CommonUsages.primaryButton; break;
                case B4ToggleButton.LeftMenu: usage = CommonUsages.menuButton; break;
                default: usage = CommonUsages.secondaryButton; break;
            }
            d.TryGetFeatureValue(usage, out bool down);
            if (down && !_prevToggleDown) Toggle();
            _prevToggleDown = down;
        }

        // ------------------------------------------------------------------ events
        void PumpEvents()
        {
            for (int guard = 0; guard < 128 && B4Native.B4_PollEvent(out int type, out int a, out int b, out float f) != 0; ++guard)
            {
                switch (type)
                {
                    case B4Native.EvtMenuVisibility: VisibilityChanged?.Invoke(a != 0); break;
                    case B4Native.EvtPageChanged: PageChanged?.Invoke((B4Page)a, b); break;
                    case B4Native.EvtSettingChanged: SettingChanged?.Invoke((B4Setting)a, b); break;
                    case B4Native.EvtHaptic: DoHaptic(a, f, b * 0.001f); break;
                    case B4Native.EvtUiSound: PlaySound(a); break;
                    case B4Native.EvtFeatureToggled:
                        if (_toggles.TryGetValue(a, out var onT)) onT(b != 0);
                        break;
                    case B4Native.EvtFeatureTriggered:
                        if (_actions.TryGetValue(a, out var onA)) onA();
                        break;
                }
            }
        }

        void DoHaptic(int hand, float amplitude, float seconds)
        {
            HapticRequested?.Invoke(hand, amplitude, seconds);
            if (!useBuiltInHaptics) return;
            var d = InputDevices.GetDeviceAtXRNode(hand == 0 ? XRNode.LeftHand : XRNode.RightHand);
            if (d.isValid && d.TryGetHapticCapabilities(out HapticCapabilities caps) && caps.supportsImpulse)
                d.SendHapticImpulse(0, Mathf.Clamp01(amplitude), seconds);
        }

        // ------------------------------------------------------------------ audio
        void BuildAudio()
        {
            _audio = gameObject.AddComponent<AudioSource>();
            _audio.playOnAwake = false;
            _audio.spatialBlend = 0f;
            _clips = new[]
            {
                Blip("b4_open", 400f, 800f, 0.12f),
                Blip("b4_close", 800f, 400f, 0.12f),
                Blip("b4_select", 660f, 660f, 0.06f),
                Blip("b4_back", 500f, 380f, 0.08f),
                Blip("b4_on", 600f, 900f, 0.09f),
                Blip("b4_off", 900f, 500f, 0.09f),
                Blip("b4_tick", 1400f, 1400f, 0.03f),
            };
        }

        void PlaySound(int id)
        {
            var s = (B4Sound)id;
            UiSoundRequested?.Invoke(s);
            if (_audio == null || id < 0 || id >= _clips.Length) return;
            AudioClip clip = (customSounds != null && id < customSounds.Length && customSounds[id] != null) ? customSounds[id] : _clips[id];
            _audio.PlayOneShot(clip, volume);
        }

        static AudioClip Blip(string name, float f0, float f1, float seconds)
        {
            int rate = AudioSettings.outputSampleRate;
            int n = Mathf.Max(1, (int)(rate * seconds));
            var data = new float[n];
            double phase = 0;
            float attack = Mathf.Max(1f, rate * 0.004f);
            for (int i = 0; i < n; ++i)
            {
                float t = i / (float)n;
                phase += 2.0 * Math.PI * Mathf.Lerp(f0, f1, t) / rate;
                float env = (1f - t) * (1f - t) * Mathf.Min(1f, i / attack);
                data[i] = (float)Math.Sin(phase) * env * 0.5f;
            }
            var clip = AudioClip.Create(name, n, 1, rate, false);
            clip.SetData(data, 0);
            return clip;
        }

        // ------------------------------------------------------------------ rendering
        void BuildMesh()
        {
            var shader = shaderOverride != null ? shaderOverride : Resources.Load<Shader>("B4MenuShader");
            if (shader == null) shader = Shader.Find("B4/Menu");
            if (shader != null) _material = new Material(shader) { name = "B4Menu (runtime)", hideFlags = HideFlags.HideAndDontSave };

            _mesh = new Mesh { name = "B4Menu", indexFormat = IndexFormat.UInt16 };
            _mesh.MarkDynamic();
            _mesh.SetVertexBufferParams(B4Packet.MaxVerts,
                new VertexAttributeDescriptor(VertexAttribute.Position, VertexAttributeFormat.Float32, 3),
                new VertexAttributeDescriptor(VertexAttribute.Normal, VertexAttributeFormat.Float32, 3),
                new VertexAttributeDescriptor(VertexAttribute.Color, VertexAttributeFormat.Float32, 3),
                new VertexAttributeDescriptor(VertexAttribute.TexCoord0, VertexAttributeFormat.Float32, 1));
            _mesh.SetIndexBufferParams(B4Packet.MaxIndices, IndexFormat.UInt16);
            _mesh.subMeshCount = 2; // 0 = menu, 1 = pointer sphere
            _mesh.bounds = new Bounds(Vector3.zero, Vector3.one * 4f); // pointer is a unit sphere scaled by its model matrix
        }

        void Draw(IntPtr pkt)
        {
            if (_material == null)
            {
                if (!_warnedShader) { _warnedShader = true; Debug.LogError("[B4] Menu shader not found (B4/Menu)."); }
                return;
            }

            int vc = Mathf.Clamp(B4Packet.VertCount(pkt), 0, B4Packet.MaxVerts);
            int ic = Mathf.Clamp(B4Packet.IndexCount(pkt), 0, B4Packet.MaxIndices);
            int menuCount = B4Packet.MenuCount(pkt), ptrCount = B4Packet.PointerCount(pkt);
            if (vc == 0 || ic == 0 || (menuCount == 0 && ptrCount == 0)) return;

            var verts = NativeArrayUnsafeUtility.ConvertExistingDataToNativeArray<B4Vertex>(B4Packet.Verts(pkt), vc, Allocator.None);
            var idx = NativeArrayUnsafeUtility.ConvertExistingDataToNativeArray<ushort>(B4Packet.Indices(pkt), ic, Allocator.None);
#if ENABLE_UNITY_COLLECTIONS_CHECKS
            var safety = AtomicSafetyHandle.GetTempMemoryHandle();
            NativeArrayUnsafeUtility.SetAtomicSafetyHandle(ref verts, safety);
            NativeArrayUnsafeUtility.SetAtomicSafetyHandle(ref idx, safety);
#endif
            const MeshUpdateFlags flags = MeshUpdateFlags.DontRecalculateBounds | MeshUpdateFlags.DontValidateIndices;
            _mesh.SetVertexBufferData(verts, 0, 0, vc, 0, flags);
            _mesh.SetIndexBufferData(idx, 0, 0, ic, flags);
            _mesh.SetSubMesh(0, new SubMeshDescriptor(B4Packet.MenuFirst(pkt), menuCount), flags);
            _mesh.SetSubMesh(1, new SubMeshDescriptor(B4Packet.PointerFirst(pkt), ptrCount), flags);

            if (menuCount > 0)
                Graphics.DrawMesh(_mesh, B4Packet.MenuModel(pkt), _material, drawLayer, null, 0, null, ShadowCastingMode.Off, false);
            if (ptrCount > 0)
                Graphics.DrawMesh(_mesh, B4Packet.PointerModel(pkt), _material, drawLayer, null, 1, null, ShadowCastingMode.Off, false);
        }
    }
}
