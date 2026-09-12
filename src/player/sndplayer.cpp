#include "sndplayer.h"

#include "flava.h"
#include "tracker_style.h"
#include <algorithm>
#include <array>
#include <cctype>
#include <cfloat>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <vector>

#include "file_io.h"

#include "sound/989snd/ame_handler.h"
#include "sound/989snd/musicbank.h"
#include "sound/989snd/sfxblock.h"
#include "fmt/format.h"
#include "third-party/imgui/imgui.h"
#include "third-party/imgui/imgui_internal.h"
#include "third-party/imgui/imgui_stdlib.h"

#if defined(__ANDROID__)
#include "android_platform.h"
#endif

namespace {
constexpr int DEFAULT_VOL = 0x400;
constexpr int MAX_VOL = 0x400;
constexpr int JAK2_BATTLE_REGISTER = 3;

size_t find_bank_offset(const std::vector<u8>& d) {
  auto u32_at = [&](size_t p) {
    u32 v = 0;
    std::memcpy(&v, d.data() + p, sizeof(v));
    return v;
  };
  auto is_fourcc = [&](size_t p, const char* cc) {
    return p + 4 <= d.size() && d[p] == static_cast<u8>(cc[0]) &&
           d[p + 1] == static_cast<u8>(cc[1]) && d[p + 2] == static_cast<u8>(cc[2]) &&
           d[p + 3] == static_cast<u8>(cc[3]);
  };

  for (size_t off = 0; off + 16 <= d.size(); off += 2048) {
    const u32 type = u32_at(off);
    if (type != 1 && type != 3) {
      continue;
    }
    const u32 num_chunks = u32_at(off + 4);
    if (num_chunks < 2 || num_chunks > 3 || off + 8 + static_cast<size_t>(num_chunks) * 8 > d.size()) {
      continue;
    }
    bool chunks_ok = true;
    for (u32 i = 0; i < num_chunks; i++) {
      const u32 coff = u32_at(off + 8 + i * 8);
      const u32 csz = u32_at(off + 8 + i * 8 + 4);
      if (off + static_cast<size_t>(coff) + csz > d.size()) {
        chunks_ok = false;
        break;
      }
    }
    if (chunks_ok && (is_fourcc(off + u32_at(off + 8), "SBlk") ||
                      is_fourcc(off + u32_at(off + 8), "SBv2"))) {
      return off;
    }
  }
  return SIZE_MAX;
}

void parse_jak1_name_table(const std::vector<u8>& d,
                           size_t limit,
                           std::string& bank_name,
                           std::vector<std::string>& names) {
  bank_name.clear();
  names.clear();
  if (limit < 0x18 || limit > d.size()) {
    return;
  }
  auto read_name16 = [&](size_t p) {
    char buf[17];
    std::memcpy(buf, d.data() + p, 16);
    buf[16] = 0;
    return std::string(buf);
  };
  u32 count = 0;
  std::memcpy(&count, d.data() + 0x14, sizeof(count));

  bank_name = read_name16(0);
  size_t pos = 0x18;
  for (u32 i = 0; i < count && pos + 20 <= limit; i++, pos += 20) {
    names.push_back(read_name16(pos));
  }
}

void draw_scope(ImDrawList* dl,
                const ImVec2& p0,
                const ImVec2& sz,
                const float* samples,
                int n,
                float gain,
                ImU32 line,
                ImU32 grid) {
  const ImVec2 p1(p0.x + sz.x, p0.y + sz.y);
  dl->AddRectFilled(p0, p1, IM_COL32(0, 0, 0, 255));
  dl->AddRect(p0, p1, grid);
  const float cy = p0.y + sz.y * 0.5f;
  dl->AddLine(ImVec2(p0.x, cy), ImVec2(p1.x, cy), grid);

  if (samples == nullptr || n < 2) {
    return;
  }
  const float amp = gain * (sz.y * 0.45f);
  const float half = sz.y * 0.5f;
  ImVec2 prev;
  for (int i = 0; i < n; i++) {
    const float u = static_cast<float>(i) / (n - 1);
    const float x = p0.x + u * sz.x;
    const float s = std::clamp(samples[i] * amp, -half, half);
    const ImVec2 cur(x, cy - s);
    if (i > 0) {
      dl->AddLine(prev, cur, line, 1.5f);
    }
    prev = cur;
  }
}

void* sndplayer_settings_read_open(ImGuiContext*,
                                   ImGuiSettingsHandler* handler,
                                   const char* name) {
  return std::strcmp(name, "State") == 0 ? handler->UserData : nullptr;
}

void sndplayer_settings_read_line(ImGuiContext*,
                                  ImGuiSettingsHandler* handler,
                                  void* /*entry*/,
                                  const char* line) {
  auto* self = static_cast<SndPlayer*>(handler->UserData);
  if (self == nullptr) {
    return;
  }
  char buf[1024];
  if (std::sscanf(line, "BrowseDir=%1023[^\n]", buf) == 1) {
    self->set_browse_dir(buf);
    return;
  }
  float scale = 0.0f;
  if (std::sscanf(line, "FontScale=%f", &scale) == 1) {
    ImGui::GetIO().FontGlobalScale = std::clamp(scale, 0.5f, 4.0f);
  }
}

void sndplayer_settings_write_all(ImGuiContext*,
                                  ImGuiSettingsHandler* handler,
                                  ImGuiTextBuffer* out) {
  auto* self = static_cast<SndPlayer*>(handler->UserData);
  if (self == nullptr) {
    return;
  }
  out->appendf("[%s][State]\n", handler->TypeName);
  out->appendf("BrowseDir=%s\n", self->browse_dir().c_str());
  out->appendf("FontScale=%.3f\n", ImGui::GetIO().FontGlobalScale);
  out->append("\n");
}

void draw_bevel(ImDrawList* dl,
                const ImVec2& p0,
                const ImVec2& p1,
                ImU32 face,
                ImU32 tl,
                ImU32 br,
                float thickness) {
  dl->AddRectFilled(p0, p1, face);
  for (float i = 0.0f; i < thickness; i += 1.0f) {
    const float x0 = p0.x + i, y0 = p0.y + i;
    const float x1 = p1.x - 1.0f - i, y1 = p1.y - 1.0f - i;
    dl->AddLine(ImVec2(x0, y0), ImVec2(x1, y0), tl);  // top
    dl->AddLine(ImVec2(x0, y0), ImVec2(x0, y1), tl);  // left
    dl->AddLine(ImVec2(x0, y1), ImVec2(x1, y1), br);  // bottom
    dl->AddLine(ImVec2(x1, y0), ImVec2(x1, y1), br);  // right
  }
}

bool bevel_button(const char* label, const ImVec2& size_arg = ImVec2(0, 0)) {
  const ImGuiStyle& style = ImGui::GetStyle();
  const ImVec2 label_sz = ImGui::CalcTextSize(label);
  const ImVec2 needed(label_sz.x + style.FramePadding.x * 2.0f,
                      label_sz.y + style.FramePadding.y * 2.0f);
  ImVec2 size = size_arg;
  size.x = size.x <= 0.0f ? needed.x : std::max(size.x, needed.x);
  size.y = size.y <= 0.0f ? needed.y : std::max(size.y, needed.y);

  const ImVec2 p0 = ImGui::GetCursorScreenPos();
  const bool pressed = ImGui::InvisibleButton(label, size);
  const bool hovered = ImGui::IsItemHovered();
  const bool held = ImGui::IsItemActive();

  const ImVec2 p1(p0.x + size.x, p0.y + size.y);
  const ImU32 face = ImGui::GetColorU32(held ? tracker::GADGET_LO
                                             : (hovered ? tracker::GADGET_HI : tracker::GADGET));
  const ImU32 light = ImGui::GetColorU32(tracker::CREAM);
  const ImU32 dark = ImGui::GetColorU32(tracker::BROWN);
  ImDrawList* dl = ImGui::GetWindowDrawList();
  if (held) {
    draw_bevel(dl, p0, p1, face, dark, light, 2.0f);
  } else {
    draw_bevel(dl, p0, p1, face, light, dark, 2.0f);
  }
  const ImVec2 tp(p0.x + (size.x - label_sz.x) * 0.5f, p0.y + (size.y - label_sz.y) * 0.5f);
  dl->AddText(tp, ImGui::GetColorU32(tracker::INK), label);
  return pressed;
}
}  // namespace

SndPlayer::SndPlayer() {
  m_group_volume.fill(MAX_VOL);

  if (ImGui::GetCurrentContext() != nullptr) {
    ImGuiSettingsHandler handler;
    handler.TypeName = "SndPlayer";
    handler.TypeHash = ImHashStr("SndPlayer");
    handler.ReadOpenFn = sndplayer_settings_read_open;
    handler.ReadLineFn = sndplayer_settings_read_line;
    handler.WriteAllFn = sndplayer_settings_write_all;
    handler.UserData = this;
    ImGui::AddSettingsHandler(&handler);
  }
}

void SndPlayer::open_file_dialog() {
#if defined(__ANDROID__)
  android_platform::open_document();
#else
  if (m_browse_dir.empty()) {
    std::error_code ec;
    auto cwd = fs::current_path(ec);
    m_browse_dir = ec ? std::string(".") : cwd.string();
  }
  m_browser_should_open = true;
#endif
}

void SndPlayer::load_bank(const std::string& path) {
  m_last_error.clear();
  std::vector<u8> data;
  try {
    data = file_util::read_binary_file(path);
  } catch (const std::exception& e) {
    m_last_error = fmt::format("Failed to read '{}': {}", path, e.what());
    return;
  }
  load_bank_data(path, std::move(data));

#if !defined(__ANDROID__)
  const auto parent = fs::path(path).parent_path();
  if (!parent.empty()) {
    m_browse_dir = parent.string();
    ImGui::MarkIniSettingsDirty();
  }
#endif
}

void SndPlayer::load_bank_data(const std::string& display_name, std::vector<u8> data) {
  m_last_error.clear();
  LoadedBank lb;
  lb.path = display_name;
  lb.data = std::move(data);

  const size_t bank_off = find_bank_offset(lb.data);
  if (bank_off == SIZE_MAX) {
    m_last_error = fmt::format("'{}' is not a valid .MUS or .SBK bank.", display_name);
    return;
  }

  if (bank_off > 0) {
    parse_jak1_name_table(lb.data, bank_off, lb.external_bank_name, lb.external_names);
  }

  lb.handle = m_player.LoadBank(std::span<u8>(lb.data).subspan(bank_off));
  if (lb.handle == nullptr) {
    m_last_error = fmt::format("'{}' is not a valid .MUS or .SBK bank.", display_name);
    return;
  }
  lb.bank = static_cast<snd::SoundBank*>(lb.handle);
  enumerate_sounds(lb);

  m_banks.push_back(std::move(lb));
  m_selected_bank = static_cast<int>(m_banks.size()) - 1;
  m_selected_sound = m_banks[m_selected_bank].sounds.empty() ? -1 : 0;
  m_sounds_view_bank = m_selected_bank;
}

void SndPlayer::unload_bank(int index) {
  if (index < 0 || index >= static_cast<int>(m_banks.size())) {
    return;
  }

  m_player.UnloadBank(m_banks[index].handle);
  m_banks.erase(m_banks.begin() + index);

  prune_finished();

  if (m_banks.empty()) {
    m_selected_bank = -1;
    m_selected_sound = -1;
  } else if (m_selected_bank == index) {
    m_selected_bank = std::min(index, static_cast<int>(m_banks.size()) - 1);
    m_selected_sound = m_banks[m_selected_bank].sounds.empty() ? -1 : 0;
  } else if (m_selected_bank > index) {
    m_selected_bank -= 1;
  }
  m_sounds_view_bank = m_selected_bank;
}

void SndPlayer::enumerate_sounds(LoadedBank& bank) {
  bank.sounds.clear();

  if (auto* sfx = dynamic_cast<snd::SFXBlock*>(bank.bank)) {
    bank.kind = BankKind::SFX;
    bank.name = sfx->Name;
    if (bank.name.empty()) {
      bank.name = !bank.external_bank_name.empty() ? bank.external_bank_name
                                                   : fs::path(bank.path).stem().string();
    }

    std::map<u32, std::string> names_by_index;
    for (const auto& [name, index] : sfx->Names) {
      names_by_index[index] = name;
    }

    for (u32 i = 0; i < sfx->Sounds.size(); i++) {
      const auto& s = sfx->Sounds[i];
      SoundEntry e;
      e.index = i;
      auto it = names_by_index.find(i);
      if (it != names_by_index.end()) {
        e.name = it->second;
      } else if (i < bank.external_names.size()) {
        e.name = bank.external_names[i];
      }
      const bool loops = (s.Flags.flags & snd::SFXBlock::SFXFlags::SFX_LOOP) != 0;
      e.info = fmt::format("vol {} grp {} pan {} grains {}{}", s.Vol, s.VolGroup, s.Pan,
                           s.Grains.size(), loops ? " loop" : "");
      bank.sounds.push_back(std::move(e));
    }
  } else if (auto* music = dynamic_cast<snd::MusicBank*>(bank.bank)) {
    bank.kind = BankKind::Music;
    bank.name = fs::path(bank.path).stem().string();

    for (u32 i = 0; i < music->Sounds.size(); i++) {
      const auto& s = music->Sounds[i];
      SoundEntry e;
      e.index = i;
      e.info = fmt::format("midi id {} vol {} grp {} pan {} repeats {}", s.MIDIID, s.Vol, s.VolGroup,
                           s.Pan, s.Repeats);
      bank.sounds.push_back(std::move(e));
    }
  } else {
    bank.kind = BankKind::Unknown;
    bank.name = "unknown bank";
  }
}

// ---------------------------------------------------------------------------
// Playback
// ---------------------------------------------------------------------------

void SndPlayer::trigger_selected() {
  if (m_selected_bank < 0 || m_selected_bank >= static_cast<int>(m_banks.size())) {
    return;
  }
  auto& bank = m_banks[m_selected_bank];
  if (m_selected_sound < 0 || m_selected_sound >= static_cast<int>(bank.sounds.size())) {
    return;
  }

  const auto& entry = bank.sounds[m_selected_sound];
  u32 handle = m_player.PlaySound(bank.handle, entry.index, m_vol, m_pan, m_pitch_mod, m_pitch_bend);
  if (handle == 0) {
    return;
  }

  ActiveSound a;
  a.handle = handle;
  a.start_tick = m_player.GetTick();
  a.last_interrupt_tick = a.start_tick;
  a.is_music = bank.kind == BankKind::Music;
  a.bank_name = bank.name;
  a.bank_handle = bank.handle;
  a.sound_index = entry.index;
  a.level = static_cast<float>(m_vol) / static_cast<float>(MAX_VOL);
  a.label = entry.name.empty() ? fmt::format("{} [{}]", bank.name, entry.index)
                               : fmt::format("{}", entry.name);
  m_active.push_back(std::move(a));
  m_selected_active = static_cast<int>(m_active.size()) - 1;
}

void SndPlayer::prune_finished() {
  int selected_handle =
      (m_selected_active >= 0 && m_selected_active < static_cast<int>(m_active.size()))
          ? static_cast<int>(m_active[m_selected_active].handle)
          : -1;

  m_active.erase(std::remove_if(m_active.begin(), m_active.end(),
                                [this](const ActiveSound& a) {
                                  return !m_player.SoundStillActive(a.handle);
                                }),
                 m_active.end());

  m_selected_active = -1;
  for (int i = 0; i < static_cast<int>(m_active.size()); i++) {
    if (static_cast<int>(m_active[i].handle) == selected_handle) {
      m_selected_active = i;
      break;
    }
  }
}

void SndPlayer::sync_selection_to_active(int active_idx) {
  if (active_idx < 0 || active_idx >= static_cast<int>(m_active.size())) {
    return;
  }
  const auto& a = m_active[active_idx];
  for (int i = 0; i < static_cast<int>(m_banks.size()); i++) {
    if (m_banks[i].handle == a.bank_handle) {
      m_sounds_view_bank = i;
      return;
    }
  }
}

void SndPlayer::pause_all() {
  for (auto& a : m_active) {
    if (!a.paused) {
      m_player.PauseSound(a.handle);
      a.elapsed_before_pause += (m_player.GetTick() - a.last_interrupt_tick);
      a.paused = true;
    }
  }
}

void SndPlayer::resume_all() {
  for (auto& a : m_active) {
    if (a.paused) {
      m_player.ContinueSound(a.handle);
      a.paused = false;
      a.last_interrupt_tick = m_player.GetTick();
    }
  }
}

void SndPlayer::stop_all_sounds() {
  m_player.StopAllSounds();
  m_active.clear();
  m_selected_active = -1;
}

void SndPlayer::process_pending_actions() {
#if defined(__ANDROID__)
  prune_finished();
  {
    std::string name;
    std::vector<u8> data;
    while (android_platform::poll_picked_file(name, data)) {
      load_bank_data(name, std::move(data));
    }
  }

  {
    int action = 0;
    while (android_platform::poll_playback_action(action)) {
      switch (action) {
        case android_platform::PlaybackPauseAll:
          pause_all();
          break;
        case android_platform::PlaybackResumeAll:
          resume_all();
          break;
        case android_platform::PlaybackStopAll:
          stop_all_sounds();
          break;
        default:
          break;
      }
    }
  }

  int count = static_cast<int>(m_active.size());
  bool playing = false;
  for (const auto& a : m_active) {
    if (!a.paused) {
      playing = true;
      break;
    }
  }
  std::string bank_name = m_active.empty() ? std::string() : m_active.back().bank_name;
  android_platform::set_playback_state(count, playing, bank_name);
#endif
}

void SndPlayer::draw() {
  prune_finished();
  process_pending_actions();

  draw_menu_bar();
  draw_banks_panel();
  draw_sound_list_panel();
  draw_playback_params_panel();
  draw_state_panel();
  draw_active_panel();
#if !defined(__ANDROID__)
  draw_file_browser();
#endif
  update_stinger();
}

void SndPlayer::update_stinger() {
  const double now = ImGui::GetTime();
  const int level = std::min(m_global_excite / 25, 3);
  const bool running = m_stinger_deadline > 0.0 && now < m_stinger_deadline;

  auto is_jak2_music = [](const ActiveSound& a) {
    if (!a.is_music) {
      return false;
    }
    const flava::FlavaSet* f = flava::lookup(a.bank_name);
    return f != nullptr && f->reg == 14;
  };

  if (level > m_stinger_prev_level && !running) {
    const int value = 9 + level;
    for (auto& a : m_active) {
      if (is_jak2_music(a)) {
        a.registers[0] = value;
        m_player.SetSoundReg(a.handle, 0, static_cast<u8>(value));
      }
    }
    m_stinger_deadline = now + 0.5;
  }
  m_stinger_prev_level = level;

  if (m_stinger_deadline > 0.0 && now >= m_stinger_deadline) {
    for (auto& a : m_active) {
      if (is_jak2_music(a)) {
        a.registers[0] = 0;
        m_player.SetSoundReg(a.handle, 0, 0);
      }
    }
    m_stinger_deadline = 0.0;
  }
}

void SndPlayer::set_zoom(float scale) {
  ImGui::GetIO().FontGlobalScale = std::clamp(scale, 0.5f, 4.0f);
  ImGui::MarkIniSettingsDirty();
}

void SndPlayer::draw_menu_bar() {
  if (ImGui::BeginMainMenuBar()) {
    if (ImGui::BeginMenu("File")) {
      if (ImGui::MenuItem("Open bank...", "Ctrl + O")) {
        open_file_dialog();
      }
      ImGui::Separator();
      if (ImGui::MenuItem("Quit", "Ctrl + Q")) {
        m_should_quit = true;
      }
      ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("Playback")) {
      if (ImGui::MenuItem("Stop all sounds")) {
        m_player.StopAllSounds();
      }
      ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("View")) {
      ImGui::TextDisabled("Zoom: %.0f%%", ImGui::GetIO().FontGlobalScale * 100.0f);
      ImGui::Separator();
      if (ImGui::MenuItem("Zoom in", "Ctrl + +")) {
        set_zoom(ImGui::GetIO().FontGlobalScale + 0.25f);
      }
      if (ImGui::MenuItem("Zoom out", "Ctrl + -")) {
        set_zoom(ImGui::GetIO().FontGlobalScale - 0.25f);
      }
      if (ImGui::MenuItem("Reset zoom", "Ctrl + 0")) {
        set_zoom(kDefaultFontScale);
      }
      ImGui::EndMenu();
    }
    ImGui::EndMainMenuBar();
  }

  const bool ctrl = ImGui::IsKeyDown(ImGuiKey_LeftCtrl) || ImGui::IsKeyDown(ImGuiKey_RightCtrl);
  if (ctrl && ImGui::IsKeyPressed(ImGuiKey_O, false)) {
    open_file_dialog();
  }
  if (ctrl && ImGui::IsKeyPressed(ImGuiKey_Q, false)) {
    m_should_quit = true;
  }

  ImGuiIO& io = ImGui::GetIO();
  if (ctrl && (ImGui::IsKeyPressed(ImGuiKey_Equal, true) ||
               ImGui::IsKeyPressed(ImGuiKey_KeypadAdd, true))) {
    set_zoom(io.FontGlobalScale + 0.25f);
  }
  if (ctrl && (ImGui::IsKeyPressed(ImGuiKey_Minus, true) ||
               ImGui::IsKeyPressed(ImGuiKey_KeypadSubtract, true))) {
    set_zoom(io.FontGlobalScale - 0.25f);
  }
  if (ctrl && (ImGui::IsKeyPressed(ImGuiKey_0, false) ||
               ImGui::IsKeyPressed(ImGuiKey_Keypad0, false))) {
    set_zoom(kDefaultFontScale);
  }
  if (ctrl && io.MouseWheel != 0.0f) {
    set_zoom(io.FontGlobalScale + io.MouseWheel * 0.1f);
  }
}

void SndPlayer::draw_banks_panel() {
  ImGui::Begin("Banks");
  if (bevel_button("Open bank...")) {
    open_file_dialog();
  }
  ImGui::Separator();

  const ImGuiStyle& style = ImGui::GetStyle();
  const float row_h = ImGui::GetFrameHeight();
  const float btn_w = ImGui::CalcTextSize("X").x + style.FramePadding.x * 2.0f;
  int bank_to_remove = -1;

  ImGui::PushStyleVar(ImGuiStyleVar_SelectableTextAlign, ImVec2(0.0f, 0.5f));
  for (int i = 0; i < static_cast<int>(m_banks.size()); i++) {
    auto& b = m_banks[i];
    ImGui::PushID(i);
    const char* kind = b.kind == BankKind::Music  ? "MUS"
                       : b.kind == BankKind::SFX   ? "SBK"
                                                   : "???";
    std::string label = fmt::format("[{}] {}", kind, b.name);
    const float sel_w =
        ImGui::GetContentRegionAvail().x - btn_w - style.ItemSpacing.x;
    if (ImGui::Selectable(label.c_str(), m_selected_bank == i, 0, ImVec2(sel_w, row_h))) {
      m_selected_bank = i;
      m_selected_sound = b.sounds.empty() ? -1 : 0;
      m_sounds_view_bank = i;
    }
    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip("%s\n%zu sounds", b.path.c_str(), b.sounds.size());
    }
    ImGui::SameLine();
    if (bevel_button("X", ImVec2(btn_w, row_h))) {
      bank_to_remove = i;
    }
    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip("Remove this bank (stops its sounds)");
    }
    ImGui::PopID();
  }
  ImGui::PopStyleVar();

  if (bank_to_remove >= 0) {
    unload_bank(bank_to_remove);
  }

  if (m_banks.empty()) {
    ImGui::PushTextWrapPos(0.0f);
    ImGui::TextDisabled("No banks loaded.\nOpen or drag a .MUS/.SBK file to begin.");
    ImGui::PopTextWrapPos();
  }

  if (!m_last_error.empty()) {
    ImGui::Separator();
    ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(190, 55, 55, 255));
    ImGui::TextWrapped("%s", m_last_error.c_str());
    ImGui::PopStyleColor();
    if (bevel_button("Dismiss")) {
      m_last_error.clear();
    }
  }
  ImGui::End();
}

void SndPlayer::draw_sound_list_panel() {
  ImGui::PushStyleColor(ImGuiCol_WindowBg, tracker::SCREEN_BG);
  bool open = ImGui::Begin("Sounds");
  ImGui::PopStyleColor();

  if (open) {
    ImGui::PushStyleColor(ImGuiCol_Text, tracker::SCREEN_FG);

    int view_bank = (m_sounds_view_bank >= 0 && m_sounds_view_bank < static_cast<int>(m_banks.size()))
                        ? m_sounds_view_bank
                        : m_selected_bank;
    if (view_bank < 0 || view_bank >= static_cast<int>(m_banks.size())) {
      ImGui::TextColored(tracker::SCREEN_DIM, "Select a bank.");
    } else {
      auto& bank = m_banks[view_bank];
      const bool is_working = view_bank == m_selected_bank;

      int highlight = -1;
      if (is_working) {
        highlight = m_selected_sound;
      } else if (m_selected_active >= 0 && m_selected_active < static_cast<int>(m_active.size()) &&
                 m_active[m_selected_active].bank_handle == bank.handle) {
        for (int j = 0; j < static_cast<int>(bank.sounds.size()); j++) {
          if (bank.sounds[j].index == m_active[m_selected_active].sound_index) {
            highlight = j;
            break;
          }
        }
      }

      if (is_working) {
        ImGui::Text("%s  (%zu sounds)", bank.name.c_str(), bank.sounds.size());
      } else {
        ImGui::Text("%s  (%zu sounds)  [active track]", bank.name.c_str(), bank.sounds.size());
      }

      ImGui::SetNextItemWidth(-FLT_MIN);
      ImGui::InputTextWithHint("##soundfilter", "Search name or #...", &m_sound_filter);
      const auto lower = [](std::string s) {
        for (char& c : s) {
          c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        }
        return s;
      };
      const std::string needle = lower(m_sound_filter);

      ImGui::Separator();

      float name_w = ImGui::CalcTextSize("Name").x;
      for (const auto& s : bank.sounds) {
        if (!s.name.empty()) {
          name_w = std::max(name_w, ImGui::CalcTextSize(s.name.c_str()).x);
        }
      }
      name_w = std::clamp(name_w + ImGui::GetStyle().CellPadding.x * 2.0f, 80.0f, 400.0f);

      if (ImGui::BeginTable("sounds", 3,
                            ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY |
                                ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_Resizable)) {
        ImGui::TableSetupColumn("#", ImGuiTableColumnFlags_WidthFixed, 40.f);
        ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthFixed, name_w);
        ImGui::TableSetupColumn("Info", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableHeadersRow();

        int shown = 0;
        for (int i = 0; i < static_cast<int>(bank.sounds.size()); i++) {
          const auto& s = bank.sounds[i];
          if (!needle.empty()) {
            const std::string hay = lower(s.name) + " #" + std::to_string(s.index);
            if (hay.find(needle) == std::string::npos) {
              continue;
            }
          }
          ++shown;
          ImGui::TableNextRow();
          ImGui::TableSetColumnIndex(0);
          std::string sel = fmt::format("{}##snd{}", s.index, i);
          if (ImGui::Selectable(sel.c_str(), highlight == i,
                                ImGuiSelectableFlags_SpanAllColumns |
                                    ImGuiSelectableFlags_AllowDoubleClick)) {
            m_selected_bank = view_bank;
            m_sounds_view_bank = view_bank;
            m_selected_sound = i;
            if (ImGui::IsMouseDoubleClicked(0)) {
              trigger_selected();
            }
          }
          ImGui::TableSetColumnIndex(1);
          ImGui::TextUnformatted(s.name.empty() ? "-" : s.name.c_str());
          ImGui::TableSetColumnIndex(2);
          ImGui::TextUnformatted(s.info.c_str());
        }
        ImGui::EndTable();

        if (shown == 0 && !needle.empty()) {
          ImGui::TextColored(tracker::SCREEN_DIM, "No sounds match \"%s\".",
                             m_sound_filter.c_str());
        }
      }
    }

    ImGui::PopStyleColor();
  }
  ImGui::End();
}

void SndPlayer::draw_playback_params_panel() {
  ImGui::Begin("Playback params");

  const bool has_selection =
      m_selected_bank >= 0 && m_selected_sound >= 0 &&
      m_selected_bank < static_cast<int>(m_banks.size()) &&
      m_selected_sound < static_cast<int>(m_banks[m_selected_bank].sounds.size());

  ImGui::BeginDisabled(!has_selection);
  if (bevel_button("Play", ImVec2(80, 0))) {
    trigger_selected();
  }
  ImGui::EndDisabled();
  ImGui::SameLine();
  if (bevel_button("Stop all", ImVec2(80, 0))) {
    m_player.StopAllSounds();
  }

  ImGui::Separator();
  ImGui::TextDisabled("Parameters used when triggering a sound:");
  ImGui::SliderInt("Volume", &m_vol, 0, MAX_VOL);
  ImGui::SliderInt("Pan", &m_pan, -1, 360);
  ImGui::SliderInt("Pitch mod", &m_pitch_mod, -0x400, 0x400);
  ImGui::SliderInt("Pitch bend", &m_pitch_bend, -0x400, 0x400);
  if (bevel_button("Reset params")) {
    m_vol = DEFAULT_VOL;
    m_pan = 0;
    m_pitch_mod = 0;
    m_pitch_bend = 0;
  }

  ImGui::End();
}

void SndPlayer::draw_state_panel() {
  ImGui::Begin("Player state");

  ImGui::SeparatorText("Excitement / Flava");
  if (ImGui::SliderInt("Global excitement", &m_global_excite, 0, 255)) {
    m_player.SetGlobalExcite(static_cast<u8>(m_global_excite));
  }
  if (ImGui::Checkbox("Force all channels (flava hack)", &m_flava_hack)) {
    snd::SoundFlavaHack = m_flava_hack ? 1 : 0;
  }

  ImGui::SeparatorText("Group volumes");
  std::array<int, 16> group_counts{};
  for (const auto& a : m_active) {
    const u8 g = m_player.GetSoundGroup(a.handle);
    if (g < 16) {
      group_counts[g]++;
    }
  }
  if (ImGui::SliderInt("Master", &m_group_volume[16], 0, MAX_VOL)) {
    m_player.SetMasterVolume(16, m_group_volume[16]);
  }
  ImGui::Spacing();
  for (int g = 0; g < 16; g++) {
    const bool is_noop = g == 15;
    const bool active = group_counts[g] > 0 && !is_noop;
    std::string label;
    if (is_noop) {
      label = fmt::format("Group {} (unused)###grp{}", g, g);
    } else {
      label = fmt::format("Group {} ({} playing)###grp{}", g, group_counts[g], g);
    }
    ImGui::BeginDisabled(!active);
    if (ImGui::SliderInt(label.c_str(), &m_group_volume[g], 0, MAX_VOL)) {
      m_player.SetMasterVolume(g, m_group_volume[g]);
    }
    ImGui::EndDisabled();
  }

  ImGui::End();
}

void SndPlayer::draw_active_panel() {
  ImGui::PushStyleColor(ImGuiCol_WindowBg, tracker::SCREEN_BG);
  bool open = ImGui::Begin("Active sounds");
  ImGui::PopStyleColor();

  if (open) {
    ImGui::PushStyleColor(ImGuiCol_Text, tracker::SCREEN_FG);

    if (m_active.empty()) {
      ImGui::TextColored(tracker::SCREEN_DIM, "Nothing playing.");
    } else {
      constexpr int MAX_SCOPE_SAMPLES = 2048;
      const float scope_w = ImGui::GetContentRegionAvail().x;
      const int scope_n = std::clamp(static_cast<int>(scope_w), 256, MAX_SCOPE_SAMPLES);
      std::array<float, MAX_SCOPE_SAMPLES> scope_samples{};
      m_player.GetScopeSamples(scope_samples.data(), scope_n);

      int stop_request = -1;
      for (int i = 0; i < static_cast<int>(m_active.size()); i++) {
        auto& a = m_active[i];
        ImGui::PushID(i);

        std::string label = fmt::format("{} (handle {})", a.label, a.handle);
        if (ImGui::Selectable(label.c_str(), m_selected_active == i)) {
          m_selected_active = i;
          sync_selection_to_active(i);
        }
        if (bevel_button(a.paused ? "Resume" : "Pause", ImVec2(70, 0))) {
          if (a.paused) {
            a.last_interrupt_tick = m_player.GetTick();
            m_player.ContinueSound(a.handle);
          } else {
            a.elapsed_before_pause += (m_player.GetTick() - a.last_interrupt_tick);
            m_player.PauseSound(a.handle);
          }
          a.paused = !a.paused;
        }
        ImGui::SameLine();
        if (bevel_button("Stop", ImVec2(70, 0))) {
          stop_request = i;
        }

        ImVec2 scope_pos = ImGui::GetCursorScreenPos();
        ImVec2 scope_sz(ImGui::GetContentRegionAvail().x, 42.0f);
        ImGui::Dummy(scope_sz);
        draw_scope(ImGui::GetWindowDrawList(), scope_pos, scope_sz, scope_samples.data(),
                   scope_n, a.paused ? 0.0f : 1.0f, ImGui::GetColorU32(tracker::SCOPE),
                   ImGui::GetColorU32(tracker::SCREEN_DIM));
        ImGui::PopID();
      }

      if (stop_request >= 0) {
        m_player.StopSound(m_active[stop_request].handle);
        m_active.erase(m_active.begin() + stop_request);
        if (m_selected_active == stop_request) {
          m_selected_active = -1;
        } else if (m_selected_active > stop_request) {
          m_selected_active--;
        }
      }
      ImGui::Separator();
      
      if (m_selected_active >= 0 && m_selected_active < static_cast<int>(m_active.size())) {
        auto& a = m_active[m_selected_active];

        const flava::FlavaSet* flavas = a.is_music ? flava::lookup(a.bank_name) : nullptr;
        if (flavas) {
          const int reg = flavas->reg;
          if (ImGui::CollapsingHeader(fmt::format("Flavas - {}", a.label).c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
            if (flavas->battle_mode) {
              const bool battle = a.registers[JAK2_BATTLE_REGISTER] != 0;
              if (ImGui::Selectable("battle mode", battle)) {
                const int nv = battle ? 0 : 1;
                a.registers[JAK2_BATTLE_REGISTER] = nv;
                m_player.SetSoundReg(a.handle, static_cast<u8>(JAK2_BATTLE_REGISTER),
                                     static_cast<u8>(nv));
              }
            }
  
            for (const auto& v : flavas->variants) {
              const bool sel = a.registers[reg] == v.value;
              std::string item = fmt::format("{}  ({})", v.name, v.value);
              if (ImGui::Selectable(item.c_str(), sel)) {
                a.registers[reg] = v.value;
                m_player.SetSoundReg(a.handle, static_cast<u8>(reg), static_cast<u8>(v.value));
              }
            }
            ImGui::Spacing();
          }
        }

        if (a.is_music) {
          if (ImGui::CollapsingHeader("AME registers", ImGuiTreeNodeFlags_DefaultOpen))
          {
            ImGui::PushStyleColor(ImGuiCol_FrameBg, tracker::rgb(0x0A, 0x14, 0x26));
            ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, tracker::rgb(0x12, 0x20, 0x38));
            ImGui::PushStyleColor(ImGuiCol_FrameBgActive, tracker::rgb(0x1A, 0x2C, 0x4C));
            ImGui::PushStyleColor(ImGuiCol_Text, tracker::SCOPE);
            for (int r = 0; r < 16; r++) {
              std::string label = fmt::format("reg {}", r);
              bool changed = ImGui::DragInt(label.c_str(), &a.registers[r], 0.25f, 0, 255, "%d",
                                            ImGuiSliderFlags_AlwaysClamp);
              ImGui::SetItemKeyOwner(ImGuiKey_MouseWheelY);
              if (ImGui::IsItemHovered()) {
                float wheel = ImGui::GetIO().MouseWheel;
                if (wheel != 0.0f) {
                  a.registers[r] =
                      std::clamp(a.registers[r] + static_cast<int>(wheel), 0, 255);
                  changed = true;
                }
              }
              if (changed) {
                m_player.SetSoundReg(a.handle, static_cast<u8>(r), static_cast<u8>(a.registers[r]));
              }
            }
            ImGui::PopStyleColor(4);
          }

        }
      }
    }

    if(m_selected_bank != -1 && m_midi_timeline_params.bank != (snd::MusicBank*)m_banks[m_selected_bank].bank){
      readBank((snd::MusicBank*)m_banks[m_selected_bank].bank, m_midi_timeline_params);
      m_midi_timeline_params.manager = m_player.getVManager();
      m_midi_timeline_params.bank = (snd::MusicBank*)m_banks[m_selected_bank].bank;
    }
    if(m_active.size() > 0 && m_selected_active != -1)
    {
      auto& active =  m_active[(size_t)m_selected_active]; 
      int player_tick = active.elapsed_before_pause + (active.paused ? 0 : (m_player.GetTick() - active.last_interrupt_tick));
      m_midi_timeline_params.syncWithPlayerTick(player_tick);
    }
    MidiTimeline(m_midi_timeline_params);
    ImGui::PopStyleColor();
    
  }
  ImGui::End();
}

namespace {
std::string to_lower(std::string s) {
  std::transform(s.begin(), s.end(), s.begin(),
                 [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  return s;
}

bool is_bank_file(const fs::path& p) {
  auto ext = to_lower(p.extension().string());
  return ext == ".mus" || ext == ".sbk";
}

std::string home_directory() {
#if defined(_WIN32)
  if (const char* up = std::getenv("USERPROFILE"); up && *up) {
    return up;
  }
  const char* drive = std::getenv("HOMEDRIVE");
  const char* path = std::getenv("HOMEPATH");
  if (drive && path) {
    return std::string(drive) + path;
  }
#else
  if (const char* home = std::getenv("HOME"); home && *home) {
    return home;
  }
#endif
  return {};
}
}  // namespace

void SndPlayer::draw_file_browser() {
  if (m_browser_should_open) {
    ImGui::OpenPopup("Open bank");
    m_browser_should_open = false;
  }

  ImVec2 center = ImGui::GetMainViewport()->GetCenter();
  ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
  ImGui::SetNextWindowSize(ImVec2(680, 480), ImGuiCond_Appearing);

  if (!ImGui::BeginPopupModal("Open bank", nullptr, ImGuiWindowFlags_NoSavedSettings)) {
    return;
  }

  fs::path dir(m_browse_dir);

  if (bevel_button("Up")) {
    auto parent = dir.parent_path();
    if (!parent.empty() && parent != dir) {
      m_browse_dir = parent.string();
      m_browse_selection.clear();
      ImGui::MarkIniSettingsDirty();
    }
  }
  ImGui::SameLine();
  if (bevel_button("Home")) {
    std::string home = home_directory();
    if (!home.empty()) {
      m_browse_dir = home;
      m_browse_selection.clear();
      ImGui::MarkIniSettingsDirty();
    }
  }
  ImGui::SameLine();
  ImGui::Checkbox("Show all files", &m_browse_show_all);
  ImGui::TextUnformatted(m_browse_dir.c_str());
  ImGui::Separator();

  std::vector<fs::path> dirs;
  std::vector<fs::path> files;
  std::error_code ec;
  for (fs::directory_iterator it(dir, fs::directory_options::skip_permission_denied, ec), end;
       it != end; it.increment(ec)) {
    if (ec) {
      break;
    }
    const auto& entry = *it;
    std::error_code is_ec;
    if (entry.is_directory(is_ec)) {
      dirs.push_back(entry.path());
    } else if (m_browse_show_all || is_bank_file(entry.path())) {
      files.push_back(entry.path());
    }
  }
  auto by_name = [](const fs::path& a, const fs::path& b) {
    return to_lower(a.filename().string()) < to_lower(b.filename().string());
  };
  std::sort(dirs.begin(), dirs.end(), by_name);
  std::sort(files.begin(), files.end(), by_name);

  const float footer = ImGui::GetFrameHeightWithSpacing() + ImGui::GetTextLineHeightWithSpacing();
  ImGui::PushStyleColor(ImGuiCol_ChildBg, tracker::SCREEN_BG);
  ImGui::BeginChild("filelist", ImVec2(0, -footer), ImGuiChildFlags_Borders);
  ImGui::PushStyleColor(ImGuiCol_Text, tracker::SCREEN_FG);
  if (ec) {
    ImGui::TextColored(tracker::SCREEN_DIM, "(cannot read this directory)");
  }
  for (const auto& d : dirs) {
    std::string label = fmt::format("[DIR] {}", d.filename().string());
    if (ImGui::Selectable(label.c_str(), false, ImGuiSelectableFlags_AllowDoubleClick)) {
      if (ImGui::IsMouseDoubleClicked(0)) {
        m_browse_dir = d.string();
        m_browse_selection.clear();
        ImGui::MarkIniSettingsDirty();
      }
    }
  }
  for (const auto& f : files) {
    std::string full = f.string();
    bool selected = full == m_browse_selection;
    if (ImGui::Selectable(f.filename().string().c_str(), selected,
                          ImGuiSelectableFlags_AllowDoubleClick)) {
      m_browse_selection = full;
      if (ImGui::IsMouseDoubleClicked(0)) {
        load_bank(full);
        ImGui::CloseCurrentPopup();
      }
    }
  }
  ImGui::PopStyleColor();
  ImGui::EndChild();
  ImGui::PopStyleColor();

  if (m_browse_selection.empty()) {
    ImGui::TextDisabled("No file selected.");
  } else {
    ImGui::Text("Selected: %s", fs::path(m_browse_selection).filename().string().c_str());
  }

  ImGui::BeginDisabled(m_browse_selection.empty());
  if (bevel_button("Open", ImVec2(120, 0))) {
    load_bank(m_browse_selection);
    ImGui::CloseCurrentPopup();
  }
  ImGui::EndDisabled();
  ImGui::SameLine();
  if (bevel_button("Cancel", ImVec2(120, 0))) {
    ImGui::CloseCurrentPopup();
  }

  ImGui::EndPopup();
}
