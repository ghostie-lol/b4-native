#include "b4_menu.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "b4_font.h"
#include "b4_log.h"

namespace b4 {
namespace {

// ---- layout constants (menu space, metres; +X right, +Y up, buttons rise toward -Z = the viewer) --
constexpr float kAreaW = 0.27f, kAreaH = 0.22f, kAreaTop = 0.105f, kGap = 0.008f;
constexpr float kFooterY = -0.163f, kFooterHY = 0.024f;
constexpr float kRimThickness = 0.0022f;
constexpr float kRearmTime = 0.08f;     // pointer must be clear of a button this long before it re-arms
constexpr float kPageLock = 0.20f;      // no activations right after a page change / menu open
constexpr float kOpenReady = 0.95f;     // menu must be (almost) fully open before touches count
constexpr const char* kVersionLabel = "V1.0.0";

const float kScaleVals[3] = {0.85f, 1.0f, 1.2f};
const float kTipVals[3] = {0.010f, 0.015f, 0.022f};
const int kTiltVals[5] = {-30, -15, 0, 15, 30};
const float kTouchVals[3] = {0.20f, 0.35f, 0.60f};  // per-button debounce seconds
const char* const kSizeNames[3] = {"S", "M", "L"};
const char* const kTouchNames[3] = {"FAST", "NORM", "SLOW"};

struct Rect { float cx, cy, hx, hy; };

Rect Cell(int cols, int rows, int col, int row) {
  const float cw = (kAreaW - kGap * static_cast<float>(cols - 1)) / static_cast<float>(cols);
  const float ch = (kAreaH - kGap * static_cast<float>(rows - 1)) / static_cast<float>(rows);
  const float x0 = -kAreaW * 0.5f + static_cast<float>(col) * (cw + kGap);
  const float y0 = kAreaTop - static_cast<float>(row) * (ch + kGap);
  return {x0 + cw * 0.5f, y0 - ch * 0.5f, cw * 0.5f, ch * 0.5f};
}

Color Mix(Color a, Color b, float t) {
  t = Clamp01(t);
  return {Lerp(a.r, b.r, t), Lerp(a.g, b.g, t), Lerp(a.b, b.b, t)};
}

// ---- palette: black body, red neon ----------------------------------------------------------------
const Color kBlack{0.012f, 0.012f, 0.016f};
const Color kPlate{0.030f, 0.030f, 0.038f};
const Color kBtnBase{0.045f, 0.045f, 0.056f};
const Color kBtnHover{0.30f, 0.030f, 0.060f};
const Color kBtnOn{0.50f, 0.030f, 0.070f};
const Color kBtnFlash{0.95f, 0.30f, 0.34f};
const Color kBtnDisabled{0.025f, 0.025f, 0.030f};
const Color kNeon{1.00f, 0.08f, 0.12f};
const Color kNeonDim{0.45f, 0.025f, 0.055f};
const Color kText{1.00f, 0.22f, 0.26f};
const Color kTextBright{1.00f, 0.90f, 0.90f};
const Color kTextDim{0.30f, 0.06f, 0.08f};
const Color kAmber{1.00f, 0.74f, 0.12f};

}  // namespace

// =================================================================================================
// lifecycle
// =================================================================================================
bool Menu::Init(const char* dataDir) {
  if (init_) return true;
  if (dataDir && dataDir[0]) std::snprintf(path_, sizeof(path_), "%s/b4menu.cfg", dataDir);
  else path_[0] = 0;
  Load();
  time_ = 0;
  open_ = 0;
  init_ = true;
  RebuildPage(false);
  B4_LOGI("Menu initialised (settings file: %s)", path_[0] ? path_ : "<none>");
  return true;
}

void Menu::Shutdown() {
  if (!init_) return;
  FlushSave(true);
  init_ = false;
}

void Menu::SetBackendName(const char* name) {
  std::snprintf(backend_, sizeof(backend_), "%s", name ? name : "NONE");
}

// =================================================================================================
// input
// =================================================================================================
void Menu::SetControllerPose(int hand, bool tracked, Vec3 pos, Quat rot) {
  if (hand < 0 || hand > 1) return;
  hand_[hand].tracked = tracked;
  hand_[hand].pos = pos;
  hand_[hand].rot = Normalize(rot);
}

void Menu::SetMenuAnchor(Vec3 localPos, Vec3 localEulerDeg) {
  anchorPos_ = localPos;
  anchorRot_ = EulerDeg(localEulerDeg);
}

void Menu::SetPointerAnchor(Vec3 localPos) { pointerAnchor_ = localPos; }

// =================================================================================================
// visibility / navigation
// =================================================================================================
void Menu::SetVisible(bool v) {
  if (visible_ == v) return;
  visible_ = v;
  Emit(kEvtMenuVisibility, v ? 1 : 0, 0, 0);
  EmitSound(v ? kSndOpen : kSndClose);
  if (v) {
    prevValid_ = false;
    RebuildPage(false);  // replays the staggered "buttons rise" animation and disarms all buttons
  }
}

void Menu::GoToPage(int page) {
  if (page < 0 || page >= kPageCount) return;
  page_ = page;
  pageIndex_ = 0;
  RebuildPage(false);
  Emit(kEvtPageChanged, page_, pageIndex_, 0);
}

// =================================================================================================
// features / settings
// =================================================================================================
int Menu::FindFeatureIndex(int id) const {
  for (int i = 0; i < kMaxFeatures; ++i)
    if (features_[i].used && features_[i].id == id) return i;
  return -1;
}

bool Menu::RegisterFeature(int id, const char* name, int category, int kind, bool defaultState) {
  if (!name || category < kCatGame || category > kCatMovement || (kind != kKindToggle && kind != kKindAction)) return false;
  int idx = FindFeatureIndex(id);
  if (idx < 0) {
    for (int i = 0; i < kMaxFeatures; ++i)
      if (!features_[i].used) { idx = i; break; }
    if (idx < 0) { B4_LOGW("Feature table full (%d), '%s' dropped", kMaxFeatures, name); return false; }
    features_[idx].used = true;
    features_[idx].id = id;
    features_[idx].state = (kind == kKindToggle) && defaultState;
  }
  Feature& f = features_[idx];
  std::snprintf(f.name, sizeof(f.name), "%s", name);
  f.category = category;
  f.kind = kind;
  if (init_) {
    const bool onListPage = page_ == kPageFavorites || page_ == kPageGame || page_ == kPageSound || page_ == kPageMovement;
    if (onListPage) RebuildPage(true);
    else RefreshWidgets();
  }
  return true;
}

int Menu::GetFeatureState(int id) const {
  const int i = FindFeatureIndex(id);
  return i < 0 ? -1 : (features_[i].state ? 1 : 0);
}

void Menu::SetFeatureState(int id, bool state) {
  const int i = FindFeatureIndex(id);
  if (i < 0 || features_[i].kind != kKindToggle) return;
  features_[i].state = state;
  RefreshWidgets();
}

bool Menu::IsFavId(int id) const {
  for (int i = 0; i < favCount_; ++i)
    if (favIds_[i] == id) return true;
  return false;
}
bool Menu::IsFavorite(int featureId) const { return IsFavId(featureId); }

void Menu::SetFavorite(int id, bool fav) {
  const bool has = IsFavId(id);
  if (fav && !has && favCount_ < kMaxFeatures) favIds_[favCount_++] = id;
  else if (!fav && has) {
    for (int i = 0; i < favCount_; ++i)
      if (favIds_[i] == id) { favIds_[i] = favIds_[--favCount_]; break; }
  }
  dirty_ = true;
  if (page_ == kPageFavorites) RebuildPage(true);
  else RefreshWidgets();
}

int Menu::GetSetting(int id) const {
  switch (id) {
    case kSetHaptics: return settings_.haptics;
    case kSetUiSound: return settings_.uiSound;
    case kSetAnimations: return settings_.animations;
    case kSetPointer: return settings_.pointer;
    case kSetScale: return settings_.scale;
    case kSetTip: return settings_.tip;
    case kSetTilt: return settings_.tilt;
    case kSetTouch: return settings_.touch;
    default: return 0;
  }
}

void Menu::SetSetting(int id, int v) {
  int* slot = nullptr;
  int maxV = 1;
  switch (id) {
    case kSetHaptics: slot = &settings_.haptics; break;
    case kSetUiSound: slot = &settings_.uiSound; break;
    case kSetAnimations: slot = &settings_.animations; break;
    case kSetPointer: slot = &settings_.pointer; break;
    case kSetScale: slot = &settings_.scale; maxV = 2; break;
    case kSetTip: slot = &settings_.tip; maxV = 2; break;
    case kSetTilt: slot = &settings_.tilt; maxV = 4; break;
    case kSetTouch: slot = &settings_.touch; maxV = 2; break;
    default: return;
  }
  v = std::max(0, std::min(maxV, v));
  if (*slot == v) return;
  *slot = v;
  ApplySettingChange(id);
}

void Menu::ApplySettingChange(int id) {
  Emit(kEvtSettingChanged, id, GetSetting(id), 0);
  dirty_ = true;
  RefreshWidgets();
}

void Menu::ResetSettings() {
  const Settings def;
  const int ids[8] = {kSetHaptics, kSetUiSound, kSetAnimations, kSetPointer, kSetScale, kSetTip, kSetTilt, kSetTouch};
  const int vals[8] = {def.haptics, def.uiSound, def.animations, def.pointer, def.scale, def.tip, def.tilt, def.touch};
  for (int i = 0; i < 8; ++i) SetSetting(ids[i], vals[i]);
}

// =================================================================================================
// events
// =================================================================================================
void Menu::Emit(int type, int a, int b, float f) {
  const int next = (qHead_ + 1) % kEventQueueSize;
  if (next == qTail_) qTail_ = (qTail_ + 1) % kEventQueueSize;  // drop oldest
  queue_[qHead_] = Event{type, a, b, f};
  qHead_ = next;
}
void Menu::EmitHaptic(float amp, int ms) {
  if (settings_.haptics) Emit(kEvtHaptic, 1, ms, amp);  // touches always come from the RIGHT hand
}
void Menu::EmitSound(int soundId) {
  if (settings_.uiSound) Emit(kEvtUiSound, soundId, 0, 0);
}
bool Menu::PollEvent(Event& out) {
  if (qTail_ == qHead_) return false;
  out = queue_[qTail_];
  qTail_ = (qTail_ + 1) % kEventQueueSize;
  return true;
}

// =================================================================================================
// pages / widgets
// =================================================================================================
const char* Menu::PageTitle() const {
  switch (page_) {
    case kPageHome: return "MAIN MENU";
    case kPageSettings: return "SETTINGS";
    case kPageFavorites: return "FAVORITE MODS";
    case kPageGame: return "GAME MODS";
    case kPageCredits: return "CREDITS";
    case kPageSound: return "SOUNDBOARD";
    case kPageMovement: return "MOVEMENT MODS";
    default: return "";
  }
}

Widget* Menu::AddWidget(int kind, int param, float cx, float cy, float hx, float hy, const char* label, bool interactive) {
  if (widgetCount_ >= kMaxWidgets) return nullptr;
  Widget& w = widgets_[widgetCount_];
  w = Widget{};
  w.kind = kind; w.param = param;
  w.cx = cx; w.cy = cy; w.hx = hx; w.hy = hy;
  w.interactive = interactive;
  std::snprintf(w.label, sizeof(w.label), "%s", label ? label : "");
  ++widgetCount_;
  return &w;
}

void Menu::FormatSettingLabel(int id, char* out, int cap) const {
  switch (id) {
    case kSetHaptics: std::snprintf(out, cap, "HAPTICS"); break;
    case kSetUiSound: std::snprintf(out, cap, "UI SOUND"); break;
    case kSetAnimations: std::snprintf(out, cap, "ANIMATE"); break;
    case kSetPointer: std::snprintf(out, cap, "POINTER"); break;
    case kSetScale: std::snprintf(out, cap, "SCALE: %s", kSizeNames[settings_.scale]); break;
    case kSetTip: std::snprintf(out, cap, "TIP: %s", kSizeNames[settings_.tip]); break;
    case kSetTilt: std::snprintf(out, cap, "TILT: %d", kTiltVals[settings_.tilt]); break;
    case kSetTouch: std::snprintf(out, cap, "TOUCH: %s", kTouchNames[settings_.touch]); break;
    case kSetReset: std::snprintf(out, cap, "RESET"); break;
    case kSetClearFavs: std::snprintf(out, cap, "CLEAR FAVS"); break;
    default: out[0] = 0; break;
  }
}

void Menu::BuildHome() {
  static const struct { int page; const char* label; } cats[6] = {
      {kPageSettings, "SETTINGS"}, {kPageFavorites, "FAVORITES"}, {kPageGame, "GAME MODS"},
      {kPageCredits, "CREDITS"},   {kPageSound, "SOUNDBOARD"},    {kPageMovement, "MOVEMENT"}};
  pageCount_ = 1;
  pageIndex_ = 0;
  for (int i = 0; i < 6; ++i) {
    const Rect r = Cell(2, 3, i % 2, i / 2);
    AddWidget(kWkCategory, cats[i].page, r.cx, r.cy, r.hx, r.hy, cats[i].label, true);
  }
}

void Menu::BuildSettings() {
  const int per = 8;
  pageCount_ = (kSettingCount + per - 1) / per;
  pageIndex_ = std::max(0, std::min(pageIndex_, pageCount_ - 1));
  for (int i = 0; i < per; ++i) {
    const int id = pageIndex_ * per + i;
    if (id >= kSettingCount) break;
    const Rect r = Cell(2, 4, i % 2, i / 2);
    int kind = kWkSettingAction;
    if (id <= kSetPointer) kind = kWkSettingToggle;
    else if (id <= kSetTouch) kind = kWkSettingCycle;
    Widget* w = AddWidget(kind, id, r.cx, r.cy, r.hx, r.hy, "", true);
    if (w) w->led = (kind == kWkSettingToggle);
  }
}

void Menu::BuildFeatureList(int category) {
  int idxs[kMaxFeatures];
  int count = 0;
  for (int i = 0; i < kMaxFeatures; ++i) {
    if (!features_[i].used) continue;
    const bool match = (category == 0) ? IsFavId(features_[i].id) : (features_[i].category == category);
    if (match) idxs[count++] = i;
  }
  const int per = 4;
  pageCount_ = std::max(1, (count + per - 1) / per);
  pageIndex_ = std::max(0, std::min(pageIndex_, pageCount_ - 1));
  if (count == 0) {
    Widget* a = AddWidget(kWkLabel, 0, 0.0f, 0.02f, 0.13f, 0.02f, category == 0 ? "NO FAVORITES YET" : "NO MODS REGISTERED", false);
    if (a) a->small = false;
    Widget* b = AddWidget(kWkLabel, 1, 0.0f, -0.025f, 0.13f, 0.02f, category == 0 ? "TOUCH THE STAR ON A MOD" : "REGISTER FROM C#", false);
    if (b) b->small = true;
    return;
  }
  const float starW = 0.045f;
  const float mainW = kAreaW - kGap - starW;
  for (int i = 0; i < per; ++i) {
    const int k = pageIndex_ * per + i;
    if (k >= count) break;
    const Rect r = Cell(1, 4, 0, i);
    const Feature& f = features_[idxs[k]];
    Widget* m = AddWidget(kWkFeatureMain, idxs[k], -kAreaW * 0.5f + mainW * 0.5f, r.cy, mainW * 0.5f, r.hy, f.name, true);
    if (m) m->led = (f.kind == kKindToggle);
    AddWidget(kWkFeatureStar, idxs[k], kAreaW * 0.5f - starW * 0.5f, r.cy, starW * 0.5f, r.hy, "\x01", true);
  }
}

void Menu::BuildCredits() {
  pageCount_ = 1;
  pageIndex_ = 0;
  char backend[40];
  std::snprintf(backend, sizeof(backend), "BACKEND: %s", backend_);
  const struct { const char* text; bool small; } lines[7] = {
      {"B4 MODMAKER", false}, {"NATIVE C++ VR MENU", true}, {"UNITY 2022.3 LTS", true}, {"QUEST 3S - ARM64", true},
      {backend, true}, {"YOUR NAME HERE", true}, {kVersionLabel, true}};
  for (int i = 0; i < 7; ++i) {
    Widget* w = AddWidget(kWkLabel, i, 0.0f, 0.072f - static_cast<float>(i) * 0.031f, 0.13f, 0.014f, lines[i].text, false);
    if (w) w->small = lines[i].small;
  }
}

void Menu::BuildFooter() {
  if (page_ == kPageHome) {
    AddWidget(kWkClose, 0, -0.095f, kFooterY, 0.040f, kFooterHY, "CLOSE", true);
    Widget* v = AddWidget(kWkLabel, 90, 0.090f, kFooterY, 0.045f, kFooterHY, kVersionLabel, false);
    if (v) v->small = true;
    return;
  }
  AddWidget(kWkBack, 0, -0.095f, kFooterY, 0.040f, kFooterHY, "BACK", true);
  if (pageCount_ > 1) {
    Widget* ind = AddWidget(kWkPageIndicator, 0, -0.020f, kFooterY, 0.030f, kFooterHY, "", false);
    if (ind) ind->small = true;
    AddWidget(kWkPrev, 0, 0.0475f, kFooterY, 0.0275f, kFooterHY, "<", pageIndex_ > 0);
    AddWidget(kWkNext, 0, 0.1075f, kFooterY, 0.0275f, kFooterHY, ">", pageIndex_ < pageCount_ - 1);
  }
}

void Menu::RefreshWidgets() {
  for (int i = 0; i < widgetCount_; ++i) {
    Widget& w = widgets_[i];
    switch (w.kind) {
      case kWkSettingToggle:
        w.on = GetSetting(w.param) != 0;
        FormatSettingLabel(w.param, w.label, kLabelLen);
        break;
      case kWkSettingCycle:
      case kWkSettingAction:
        FormatSettingLabel(w.param, w.label, kLabelLen);
        break;
      case kWkFeatureMain:
        w.on = features_[w.param].kind == kKindToggle && features_[w.param].state;
        break;
      case kWkFeatureStar:
        w.on = IsFavId(features_[w.param].id);
        break;
      case kWkPageIndicator:
        std::snprintf(w.label, kLabelLen, "%d/%d", pageIndex_ + 1, pageCount_);
        break;
      default: break;
    }
  }
}

void Menu::RebuildPage(bool keepState) {
  Widget old[kMaxWidgets];
  const int oldCount = widgetCount_;
  if (keepState) std::memcpy(old, widgets_, sizeof(Widget) * static_cast<size_t>(oldCount));

  widgetCount_ = 0;
  switch (page_) {
    case kPageHome: BuildHome(); break;
    case kPageSettings: BuildSettings(); break;
    case kPageFavorites: BuildFeatureList(0); break;
    case kPageGame: BuildFeatureList(kCatGame); break;
    case kPageSound: BuildFeatureList(kCatSound); break;
    case kPageMovement: BuildFeatureList(kCatMovement); break;
    case kPageCredits: BuildCredits(); break;
    default: BuildHome(); break;
  }
  BuildFooter();
  RefreshWidgets();

  for (int i = 0; i < widgetCount_; ++i) {
    Widget& w = widgets_[i];
    bool restored = false;
    if (keepState) {
      for (int j = 0; j < oldCount; ++j) {
        const Widget& o = old[j];
        if (o.kind == w.kind && o.param == w.param && w.kind != kWkLabel) {
          w.hover = o.hover; w.press = o.press; w.flash = o.flash; w.appear = o.appear; w.appearDelay = o.appearDelay;
          w.hoverTarget = o.hoverTarget; w.pressTarget = o.pressTarget;
          w.touching = o.touching; w.hovering = o.hovering; w.armed = o.armed;
          w.clearTime = o.clearTime; w.lastActivate = o.lastActivate;
          restored = true;
          break;
        }
      }
      if (!restored) { w.appear = 1.0f; w.armed = false; }
    } else {
      w.appear = settings_.animations ? 0.0f : 1.0f;
      w.appearDelay = static_cast<float>(i) * 0.02f;
      w.armed = false;
    }
  }
  if (!keepState) {
    pageChangeTime_ = time_;
    prevValid_ = false;
  }
}

const Widget* Menu::FindWidget(int kind, int param) const {
  for (int i = 0; i < widgetCount_; ++i)
    if (widgets_[i].kind == kind && widgets_[i].param == param) return &widgets_[i];
  return nullptr;
}

void Menu::DisarmAll() {
  for (int i = 0; i < widgetCount_; ++i) {
    widgets_[i].armed = false;
    widgets_[i].clearTime = 0;
    widgets_[i].touching = false;
    widgets_[i].hovering = false;
    widgets_[i].hoverTarget = 0;
    widgets_[i].pressTarget = 0;
  }
}

// =================================================================================================
// update: poses, touch, animation
// =================================================================================================
void Menu::UpdatePoses() {
  const Quat tilt = AxisAngle({1, 0, 0}, static_cast<float>(kTiltVals[settings_.tilt]) * kDeg2Rad);
  menuRot_ = hand_[0].rot * anchorRot_ * tilt;
  menuPos_ = hand_[0].pos + Rotate(hand_[0].rot, anchorPos_);
  menuScale_ = kScaleVals[settings_.scale] * (0.55f + 0.45f * SmoothStep(open_));
  pointerPos_ = hand_[1].pos + Rotate(hand_[1].rot, pointerAnchor_);
  pointerR_ = kTipVals[settings_.tip];
}

Vec3 Menu::ToLocal(Vec3 world) const {
  return Rotate(Conjugate(menuRot_), world - menuPos_) * (1.0f / menuScale_);
}

Vec3 Menu::MenuLocalToWorld(Vec3 local) const { return menuPos_ + Rotate(menuRot_, local * menuScale_); }

namespace {
// Sphere (centre c, radius r) vs button volume: x/y = button footprint, z from the raised top face
// (-kButtonHeight, towards the viewer) to just behind the face plate. penetration = how far the
// sphere's far side has passed the top plane.
bool SphereVsButton(Vec3 c, float r, const Widget& w, float grow, float* penetration) {
  const Vec3 mn{w.cx - w.hx - grow, w.cy - w.hy - grow, -kButtonHeight - grow};
  const Vec3 mx{w.cx + w.hx + grow, w.cy + w.hy + grow, 0.004f};
  if (c.z > 0.002f) return false;  // centre behind the face plate: not a front touch
  const Vec3 q{Clamp(c.x, mn.x, mx.x), Clamp(c.y, mn.y, mx.y), Clamp(c.z, mn.z, mx.z)};
  const Vec3 d = c - q;
  if (Dot(d, d) > r * r) return false;
  if (penetration) *penetration = c.z + r + kButtonHeight;
  return true;
}
}  // namespace

void Menu::UpdateTouch(float dt) {
  const bool canTouch = open_ >= kOpenReady && hand_[0].tracked && hand_[1].tracked;
  const Vec3 loc = ToLocal(pointerPos_);
  const float rl = pointerR_ / menuScale_;
  Vec3 prev = prevValid_ ? prevLocal_ : loc;
  if (Length(loc - prev) > 0.5f) prev = loc;  // tracking jump / re-acquire: do not sweep across the menu
  prevLocal_ = loc;
  prevValid_ = 