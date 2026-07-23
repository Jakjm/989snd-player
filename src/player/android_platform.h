#pragma once

#if defined(__ANDROID__)

#include <cstdint>
#include <string>
#include <vector>

namespace android_platform {

void open_document();
bool poll_picked_file(std::string& name, std::vector<uint8_t>& data);

enum PlaybackAction {
  PlaybackPauseAll = 0,
  PlaybackResumeAll = 1,
  PlaybackStopAll = 2,
};

void set_playback_state(int active_count, bool playing, const std::string& bank_name);
bool poll_playback_action(int& action);

}  // namespace android_platform

#endif  // __ANDROID__
