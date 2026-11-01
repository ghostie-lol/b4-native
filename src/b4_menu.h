#pragma once
#include <cstdint>

#include "b4_math.h"
#include "b4_mesh.h"

namespace b4 {

// ---- sizes -----------------------------------------------------------------------------------------
constexpr int kMaxFeatures = 64;
constexpr int kMaxWidgets = 32;
constexpr int kLabelLen = 32;
constexpr int kEventQueueSize = 64;

constexpr float kButtonHeight = 0.010f;  // how far buttons rise from the face plate
constexpr float kPanelW = 0.30f, kPanelH = 0.40f, kPanelD = 0.012f;

// ---- ids -------------------------------------------------------------------------------------------
enum : int { kPageHome = 0, kPageSettings, kPageFavorites, kPageGame, kPageCredits, kPageSound, kPageMovement, kPageCount };
enum : int { kCatGame = 1, kCatSound = 2, kCatMovement = 3 };
enum : int { kKindToggle = 0, kKindAction = 1 };
enum : int {
  kSetHaptics = 0, kSetUiSound, kSetAnimations, kSetPointer,  // toggles
  kSetScale, kSetTip, kSetTilt, kSetTouch,                    // cycles
  kSetReset, kSetClearFavs,                                   // actions
  kSettingCount
};
enum : int {
  kEvtMenuVisibility = 1, kEvtPageChanged, kEvtSettingChanged, kEvtHaptic, kEvtUiSound,
  kEvtFeatureToggled, kEvtFeatureTriggered
};
enum : int { kSndOpen = 0, kSndClose, kSndSelect, kSndBack, kSndOn, kSndOff, kSndTick };
enum : int {
  kWkLabel = 0, kWkPageIndicator, kWkCategory, kWkBack, kWkClose, kWkPrev, kWkNext,
  kWkSettingToggle, kWkSettingCycle, kWkSettingAction, kWkFeatureMain, kWkFeatureStar
};

// ---- data ------------------------------------------------------------------------------------------
struct Event { int type; int a; int b; float f; };

struct Settings {
  int haptics = 1, uiSound = 1, animations = 1, pointer = 1;
  int scale = 1, tip = 1, tilt = 2, touch = 1;
};

struct Feature {
  bool used = false;
  int id = 0;
  char name[kLabelLen] = {};
  int category = kCatGame;
  int kind = kKindToggle;
  bool state = false;
};

struct Widget {
  int kind = kWkLabel, param = 0;
  float cx = 0, cy = 0, hx = 0, hy = 0;
  char label[kLabelLen] = {};
  bool interactive = true, led = false, small = false, on = false;
  float hover = 0, press = 0, flash = 0, appear = 0, appearDelay = 0;
  float hoverTarget = 0, pressTarget = 0;
  bool touching = false, hovering = false, armed = false;
  float clearTime = 0, lastActivate = -10.0f;
};

struct Hand {
  bool tracked = false;
  Vec3 pos{0, 0, 0};
  Quat rot{};
};

// ---- the menu --------------------------------------------------------------------------------------
class Menu {
 public:
  bool Init(const char* dataDir);
  void Shutdown();
  void SetBackendName(const char* name);
  void SetLinearColor(bool linear) { linear_ = linear; }

  // input (hand 0 = left, carries the menu; hand 1 = right, the pointer)
  void SetControllerPose(int hand, bool tracked, Vec3 pos, Quat rot);
  void SetMenuAnchor(Vec3 localPos, Vec3 localEulerDeg);
  void SetPointerAnchor(Vec3 localPos);

  void SetVisible(bool v);
  bool IsVisible() const { return visible_; }
  void GoToPage(int page);

  // features
  bool RegisterFeature(int id, const char* name, int category, int kind, bool defaultState);
  int GetFeatureState(int id) const;
  void SetFeatureState(int id, bool state);
  bool IsFavorite(int featureId) const;
  void SetFavorite(int id, bool fav);

  // settings
  int GetSetting(int id) const;
  void SetSetting(int id, int v);
  void ResetSettings();

  // events (haptics, sounds, toggles...) drained by the host
  bool PollEvent(Event& out);

  void Update(float dt);
  void BuildPacket(RenderPacket& pkt, const float* vp, int width, int height, int msaa, bool reversedZ) const;

  Vec3 MenuLocalToWorld(Vec3 local) const;

 private:
  // lifecycle / persistence
  void Load();
  void Save();
  void FlushSave(bool force);

  // features
  int FindFeatureIndex(int id) const;
  bool IsFavId(int id) const;
  void ApplySettingChange(int id);

  // events
  void Emit(int type, int a, int b, float f);
  void EmitHaptic(float amp, int ms);
  void EmitSound(int soundId);

  // pages
  const char* PageTitle() const;
  Widget* AddWidget(int kind, int param, float cx, float cy, float hx, float hy, const char* label, bool interactive);
  void FormatSettingLabel(int id, char* out, int cap) const;
  void BuildHome();
  void BuildSettings();
  void BuildFeatureList(int category);
  void BuildCredits();
  void BuildFooter();
  void RefreshWidgets();
  void RebuildPage(bool keepState);
  const Widget* FindWidget(int kind, int param) const;
  void DisarmAll();

  // update
  void UpdatePoses();
  Vec3 ToLocal(Vec3 world) const;
  void UpdateTouch(float dt);
  void UpdateAnimation(float dt);
  void Activate(int widgetIndex);

  // geometry
  void DrawWidget(MeshBuilder& mb, const Widget& w) const;
  void BuildMenuGeometry(MeshBuilder& mb) const;

  bool init_ = false, visible_ = false, dirty_ = false, linear_ = false;
  mutable bool overflowLogged_ = false;
  bool prevValid_ = false, anyTouch_ = false;
  char path_[512] = {};
  char backend_[32] = "NONE";

  float time_ = 0, open_ = 0, lastSave_ = 0;
  float pageChangeTime_ = 0, lastHoverHaptic_ = -10.0f, lastActivateAny_ = -10.0f;

  Hand hand_[2];
  Vec3 anchorPos_{0.0f, 0.05f, 0.10f}, pointerAnchor_{0, 0, 0}, prevLocal_{0, 0, 0};
  Quat anchorRot_{};
  Vec3 menuPos_{0, 0, 0}, pointerPos_{0, 0, 0};
  Quat menuRot_{};
  float menuScale_ = 1.0f, pointerR_ = 0.015f;

  int page_ = kPageHome, pageIndex_ = 0, pageCount_ = 1;
  Settings settings_;
  Feature features_[kMaxFeatures];
  int favIds_[kMaxFeatures] = {};
  int favCount_ = 0;
  Widget widgets_[kMaxWidgets];
  int widgetCount_ = 0;

  Event queue_[kEventQueueSize] = {};
  int qHead_ = 0, qTail_ = 0;
};

}  // namespace b4
