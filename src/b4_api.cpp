// Plain C exports so the library can be called from C# (P/Invoke) or any other host.
#include <new>

#include "b4_menu.h"

using namespace b4;

namespace {
Menu g_menu;
RenderPacket* g_pkt = nullptr;
}  // namespace

#define B4_EXPORT extern "C" __attribute__((visibility("default")))

B4_EXPORT int B4_Init(const char* dataDir) { return g_menu.Init(dataDir) ? 1 : 0; }
B4_EXPORT void B4_Shutdown() { g_menu.Shutdown(); delete g_pkt; g_pkt = nullptr; }
B4_EXPORT void B4_SetBackendName(const char* n) { g_menu.SetBackendName(n); }
B4_EXPORT void B4_SetLinearColor(int linear) { g_menu.SetLinearColor(linear != 0); }

B4_EXPORT void B4_SetControllerPose(int hand, int tracked, float px, float py, float pz,
                                    float qx, float qy, float qz, float qw) {
  g_menu.SetControllerPose(hand, tracked != 0, {px, py, pz}, {qx, qy, qz, qw});
}
B4_EXPORT void B4_SetMenuAnchor(float x, float y, float z, float ex, float ey, float ez) {
  g_menu.SetMenuAnchor({x, y, z}, {ex, ey, ez});
}
B4_EXPORT void B4_SetPointerAnchor(float x, float y, float z) { g_menu.SetPointerAnchor({x, y, z}); }

B4_EXPORT void B4_SetVisible(int v) { g_menu.SetVisible(v != 0); }
B4_EXPORT int B4_IsVisible() { return g_menu.IsVisible() ? 1 : 0; }
B4_EXPORT void B4_GoToPage(int page) { g_menu.GoToPage(page); }

B4_EXPORT int B4_RegisterFeature(int id, const char* name, int category, int kind, int defaultState) {
  return g_menu.RegisterFeature(id, name, category, kind, defaultState != 0) ? 1 : 0;
}
B4_EXPORT int B4_GetFeatureState(int id) { return g_menu.GetFeatureState(id); }
B4_EXPORT void B4_SetFeatureState(int id, int state) { g_menu.SetFeatureState(id, state != 0); }
B4_EXPORT int B4_GetSetting(int id) { return g_menu.GetSetting(id); }
B4_EXPORT void B4_SetSetting(int id, int v) { g_menu.SetSetting(id, v); }

// Returns 1 and fills type/a/b/f while events are queued.
B4_EXPORT int B4_PollEvent(int* type, int* a, int* b, float* f) {
  Event e;
  if (!g_menu.PollEvent(e)) return 0;
  *type = e.type; *a = e.a; *b = e.b; *f = e.f;
  return 1;
}

B4_EXPORT void B4_Update(float dt) { g_menu.Update(dt); }

// vp = 32 floats (two 4x4 view-projection matrices). Returns the packet, or null on allocation failure.
B4_EXPORT const RenderPacket* B4_BuildPacket(const float* vp, int width, int height, int msaa, int reversedZ) {
  if (!g_pkt) g_pkt = new (std::nothrow) RenderPacket();
  if (!g_pkt) return nullptr;
  g_menu.BuildPacket(*g_pkt, vp, width, height, msaa, reversedZ != 0);
  return g_pkt;
}
