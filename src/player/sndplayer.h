#pragma once

#include <array>
#include <string>
#include <vector>

#include "common/common_types.h"
#include "midi_timeline.h"
#include "sound/989snd/player.h"

namespace snd {
class SoundBank;
}

#if defined(__ANDROID__)
inline constexpr float kDefaultFontScale = 2.5f;
#else
inline constexpr float kDefaultFontScale = 1.25f;
#endif

class SndPlayer {
 public:
  SndPlayer();

  void draw();
  void load_bank(const std::string& path);
  void load_bank_data(const std::string& display_name, std::vector<u8> data);
  bool should_quit() const { return m_should_quit; }
  void process_pending_actions();

  const std::string& browse_dir() const { return m_browse_dir; }
  void set_browse_dir(std::string dir) { m_browse_dir = std::move(dir); }

  void set_zoom(float scale);

 private:
  enum class BankKind { Unknown, SFX, Music };

  struct SoundEntry {
    u32 index = 0;
    std::string name;
    std::string info;
  };

  struct LoadedBank {
    std::string path;
    std::string name;
    std::vector<u8> data;
    snd::BankHandle handle = nullptr;
    snd::SoundBank* bank = nullptr;
    BankKind kind = BankKind::Unknown;
    std::vector<SoundEntry> sounds;
    std::string external_bank_name;
    std::vector<std::string> external_names;
  };

  struct ActiveSound {
    u32 handle = 0;
    std::string label;
    std::string bank_name;
    snd::BankHandle bank_handle = nullptr;
    u32 sound_index = 0;
    s32 start_tick = 0;
    bool is_music = false;
    bool paused = false;
    float level = 1.0f;
    std::array<int, 16> registers{};
  };

  snd::Player m_player;
  std::vector<LoadedBank> m_banks;
  std::vector<ActiveSound> m_active;
  int m_selected_bank = -1;
  int m_selected_sound = -1;
  int m_selected_active = -1;
  int m_sounds_view_bank = -1;

  int m_vol = 0x400;
  int m_pan = 0;
  int m_pitch_mod = 0;
  int m_pitch_bend = 0;

  int m_global_excite = 0;
  bool m_flava_hack = false;
  std::array<int, 17> m_group_volume;

  int m_stinger_prev_level = 0;
  double m_stinger_deadline = 0.0;

  std::string m_sound_filter;

  bool m_browser_should_open = false;
  std::string m_browse_dir;
  std::string m_browse_selection;
  bool m_browse_show_all = false;
  std::string m_last_error;
  bool m_should_quit = false;
  bool m_viewing_registers = true;
  MidiTimelineParams m_midi_timeline_params;

  void open_file_dialog();
  void enumerate_sounds(LoadedBank& bank);
  void trigger_selected();
  void prune_finished();
  void unload_bank(int index);
  void sync_selection_to_active(int active_idx);

  void pause_all();
  void resume_all();
  void stop_all_sounds();
  void update_stinger();

  void draw_menu_bar();
  void draw_file_browser();
  void draw_banks_panel();
  void draw_sound_list_panel();
  void draw_playback_params_panel();
  void draw_state_panel();
  void draw_active_panel();
};
