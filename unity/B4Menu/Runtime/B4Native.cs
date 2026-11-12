using System;
using System.Runtime.InteropServices;
using UnityEngine;

namespace B4
{
    public enum B4Category { Game = 1, Sound = 2, Movement = 3 }
    public enum B4Page { Home = 0, Settings, Favorites, Game, Credits, Sound, Movement }
    public enum B4Setting { Haptics = 0, UiSound, Animations, Pointer, Scale, Tip, Tilt, Touch }
    public enum B4Sound { Open = 0, Close, Select, Back, On, Off, Tick }

    /// <summary>Matches b4::Vertex (10 floats, 40 bytes).</summary>
    [StructLayout(LayoutKind.Sequential)]
    internal struct B4Vertex
    {
        public Vector3 position;
        public Vector3 normal;
        public Vector3 color;
        public float emissive;
    }

    /// <summary>P/Invoke surface of libb4menu.so (see src/b4_api.cpp).</summary>
    internal static class B4Native
    {
        const string Lib = "b4menu";

        // native event ids (b4_menu.h)
        public const int EvtMenuVisibility = 1, EvtPageChanged = 2, EvtSettingChanged = 3, EvtHaptic = 4,
                         EvtUiSound = 5, EvtFeatureToggled = 6, EvtFeatureTriggered = 7;

        [DllImport(Lib)] public static extern int B4_Init([MarshalAs(UnmanagedType.LPStr)] string dataDir);
        [DllImport(Lib)] public static extern void B4_Shutdown();
        [DllImport(Lib)] public static extern void B4_SetBackendName([MarshalAs(UnmanagedType.LPStr)] string name);
        [DllImport(Lib)] public static extern void B4_SetLinearColor(int linear);
        [DllImport(Lib)] public static extern void B4_SetControllerPose(int hand, int tracked, float px, float py, float pz,
                                                                         float qx, float qy, float qz, float qw);
        [DllImport(Lib)] public static extern void B4_SetMenuAnchor(float x, float y, float z, float ex, float ey, float ez);
        [DllImport(Lib)] public static extern void B4_SetPointerAnchor(float x, float y, float z);
        [DllImport(Lib)] public static extern void B4_SetVisible(int v);
        [DllImport(Lib)] public static extern int B4_IsVisible();
        [DllImport(Lib)] public static extern void B4_GoToPage(int page);
        [DllImport(Lib)] public static extern int B4_RegisterFeature(int id, [MarshalAs(UnmanagedType.LPStr)] string name,
                                                                      int category, int kind, int defaultState);
        [DllImport(Lib)] public static extern int B4_GetFeatureState(int id);
        [DllImport(Lib)] public static extern void B4_SetFeatureState(int id, int state);
        [DllImport(Lib)] public static extern int B4_GetSetting(int id);
        [DllImport(Lib)] public static extern void B4_SetSetting(int id, int v);
        [DllImport(Lib)] public static extern int B4_PollEvent(out int type, out int a, out int b, out float f);
        [DllImport(Lib)] public static extern void B4_Update(float dt);
        [DllImport(Lib)] public static extern IntPtr B4_BuildPacket(float[] vp, int width, int height, int msaa, int reversedZ);
    }

    /// <summary>
    /// Byte offsets into b4::RenderPacket. Verified with offsetof() against the C++ struct
    /// (sizeof = 2,880,300). If kMaxVerts / kMaxIndices or the struct change, update these.
    /// </summary>
    internal static unsafe class B4Packet
    {
        public const int MaxVerts = 60000;
        public const int MaxIndices = 240000;

        const int OffVerts = 4;
        const int OffIndices = 2400004;
        const int OffVertCount = 2880004;
        const int OffIndexCount = 2880008;
        const int OffMenuFirst = 2880012;
        const int OffMenuCount = 2880016;
        const int OffPointerFirst = 2880020;
        const int OffPointerCount = 2880024;
        const int OffMenuModel = 2880028;
        const int OffPointerModel = 2880092;

        public static B4Vertex* Verts(IntPtr p) => (B4Vertex*)((byte*)p + OffVerts);
        public static ushort* Indices(IntPtr p) => (ushort*)((byte*)p + OffIndices);
        public static int VertCount(IntPtr p) => *(int*)((byte*)p + OffVertCount);
        public static int IndexCount(IntPtr p) => *(int*)((byte*)p + OffIndexCount);
        public static int MenuFirst(IntPtr p) => *(int*)((byte*)p + OffMenuFirst);
        public static int MenuCount(IntPtr p) => *(int*)((byte*)p + OffMenuCount);
        public static int PointerFirst(IntPtr p) => *(int*)((byte*)p + OffPointerFirst);
        public static int PointerCount(IntPtr p) => *(int*)((byte*)p + OffPointerCount);
        public static Matrix4x4 MenuModel(IntPtr p) => ReadMat((float*)((byte*)p + OffMenuModel));
        public static Matrix4x4 PointerModel(IntPtr p) => ReadMat((float*)((byte*)p + OffPointerModel));

        // b4::Mat4 is column-major, same as UnityEngine.Matrix4x4's column constructor.
        static Matrix4x4 ReadMat(float* f) => new Matrix4x4(
            new Vector4(f[0], f[1], f[2], f[3]),
            new Vector4(f[4], f[5], f[6], f[7]),
            new Vector4(f[8], f[9], f[10], f[11]),
            new Vector4(f[12], f[13], f[14], f[15]));
    }
}
