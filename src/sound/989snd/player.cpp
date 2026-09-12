// Copyright: 2021 - 2024, Ziemas
// SPDX-License-Identifier: ISC
#include "player.h"

#include <fstream>

#include "sfxblock.h"

#include "fmt/format.h"
#include "sound/989snd/vagvoice.h"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <combaseapi.h>
#include <windows.h>
#endif
#include "common/log/log.h"

namespace snd {

u8 g_global_excite = 0;

Player::Player() : mVmanager(mSynth) {
  InitCubeb();
}

Player::~Player() {
  DestroyCubeb();
}

void Player::InitCubeb() {
#ifdef _WIN32
  HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
  m_coinitialized = SUCCEEDED(hr);
  if (FAILED(hr) && hr != RPC_E_CHANGED_MODE) {
    lg::error("Couldn't initialize COM");
    lg::error("Cubeb init failed");
    return;
  }
#endif

#if defined(__ANDROID__)
  // cubeb's default Android backend order tries OpenSL ES first, which needs a
  // JavaVM wired through its JNI layer for output routing (we don't provide one),
  // so it comes up silent. Force AAudio instead: it's pure NDK, needs no JNI, and
  // is why the app targets API 28+.
  cubeb_init(&mCtx, "OpenGOAL", "aaudio");
#else
  cubeb_init(&mCtx, "OpenGOAL", nullptr);
#endif

  cubeb_stream_params outparam = {};
  outparam.channels = 2;
  outparam.format = CUBEB_SAMPLE_S16LE;
  outparam.rate = 48000;
  outparam.layout = CUBEB_LAYOUT_STEREO;
  outparam.prefs = CUBEB_STREAM_PREF_NONE;

  s32 err = 0;
  u32 latency = 0;
  err = cubeb_get_min_latency(mCtx, &outparam, &latency);
  if (err != CUBEB_OK) {
    lg::error("Cubeb init failed");
    return;
  }

#if defined(__ANDROID__)
  // AAudio's minimum latency underruns intermittently (audible crackle) when the
  // synth callback is briefly starved by UI work or GC. Request a bigger buffer
  // for headroom - latency is not a concern for a bank player.
  if (latency < 4096) {
    latency = 4096;
  }
#endif

  err = cubeb_stream_init(mCtx, &mStream, "OpenGOAL", nullptr, nullptr, nullptr, &outparam, latency,
                          &sound_callback, &state_callback, this);
  if (err != CUBEB_OK) {
    lg::error("Cubeb init failed");
    return;
  }

  err = cubeb_stream_start(mStream);
  if (err != CUBEB_OK) {
    lg::error("Cubeb init failed");
    return;
  }
}

void Player::DestroyCubeb() {
  cubeb_stream_stop(mStream);
  cubeb_stream_destroy(mStream);
  cubeb_destroy(mCtx);
#ifdef _WIN32
  if (m_coinitialized) {
    CoUninitialize();
    m_coinitialized = false;
  }
#endif
}

long Player::sound_callback([[maybe_unused]] cubeb_stream* stream,
                            void* user,
                            [[maybe_unused]] const void* input,
                            void* output_buffer,
                            long nframes) {
  ((Player*)user)->Tick((s16Output*)output_buffer, nframes);
  return nframes;
}

void Player::state_callback([[maybe_unused]] cubeb_stream* stream,
                            [[maybe_unused]] void* user,
                            [[maybe_unused]] cubeb_state state) {}

void Player::Tick(s16Output* stream, int samples) {
  std::scoped_lock lock(mTickLock);
  s16Output* const base = stream;
  static int htick = 200;
  static int stick = 48000;
  for (int i = 0; i < samples; i++) {
    // The handlers expect to tick at 240hz
    // 48000/240 = 200
    if (htick == 200) {
      mTick++;

      for (auto it = mHandlers.begin(); it != mHandlers.end();) {
        bool done = it->second->Tick();
        if (done) {
          // fmt::print("erasing handler\n");
          mHandleAllocator.FreeId(it->first);
          it = mHandlers.erase(it);
        } else {
          ++it;
        }
      }

      htick = 0;
    }

    if (stick == 48000) {
      // fmt::print("{} handlers active\n", m_handlers.size());
      stick = 0;
    }

    stick++;
    htick++;
    *stream++ = mSynth.Tick();
  }

  // Publish the block we just produced to the oscilloscope ring buffer.
  {
    std::scoped_lock scope_lock(mScopeLock);
    for (int i = 0; i < samples; i++) {
      const s32 mono = (static_cast<s32>(base[i].left) + base[i].right) / 2;
      mScopeBuf[mScopeHead] = static_cast<s16>(mono);
      mScopeHead = (mScopeHead + 1) % kScopeBufSize;
    }
  }
}

void Player::GetScopeSamples(float* out, int count) {
  if (count <= 0) {
    return;
  }
  if (count > kScopeBufSize) {
    count = kScopeBufSize;
  }
  std::scoped_lock scope_lock(mScopeLock);
  // mScopeHead points one past the newest sample. Walk back `count` samples and
  // emit them oldest-first.
  int idx = mScopeHead - count;
  idx %= kScopeBufSize;
  if (idx < 0) {
    idx += kScopeBufSize;
  }
  for (int i = 0; i < count; i++) {
    out[i] = mScopeBuf[idx] / 32768.0f;
    idx = (idx + 1) % kScopeBufSize;
  }
}

u32 Player::PlaySound(BankHandle bank_id, u32 sound_id, s32 vol, s32 pan, s32 pm, s32 pb) {
  std::scoped_lock lock(mTickLock);
  auto bank = mLoader.GetBankByHandle(bank_id);
  if (bank == nullptr) {
    lg::error("play_sound: Bank {} does not exist", static_cast<void*>(bank_id));
    return 0;
  }

  u32 handle = mHandleAllocator.GetId();
  auto handler = bank->MakeHandler(mVmanager, sound_id, vol, pan, pm, pb, GetTick(), handle);
  if (!handler.has_value()) {
    return 0;
  }

  auto handler_to_stop = handler.value()->CheckInstanceLimit(mHandlers, vol, true);
  if (handler_to_stop) {
    handler_to_stop->Stop();
    if (handler_to_stop == handler.value().get()) {
      return 0;
    }
  }

  handler.value()->m_sound_handle = handle;
  mHandlers.emplace(handle, std::move(handler.value()));
  // fmt::print("play_sound {}:{} - {}\n", bank_id, sound_id, handle);

  return handle;
}

void Player::DebugPrintAllSoundsInBank(BankHandle bank_id) {
  std::scoped_lock lock(mTickLock);
  auto* bank = mLoader.GetBankByHandle(bank_id);
  if (!bank) {
    lg::error("DebugPrintAllSoundsInBank: invalid bank");
    return;
  }
  bank->DebugPrintAllSounds();
}

u32 Player::PlaySoundByName(BankHandle bank_id,
                            char* bank_name,
                            char* sound_name,
                            s32 vol,
                            s32 pan,
                            s32 pm,
                            s32 pb) {
  std::scoped_lock lock(mTickLock);
  SoundBank* bank = nullptr;
  if (bank_id == 0 && bank_name != nullptr) {
    bank = mLoader.GetBankByName(bank_name);
  } else if (bank_id != 0) {
    bank = mLoader.GetBankByHandle(bank_id);
  } else {
    bank = mLoader.GetBankWithSound(sound_name);
  }

  if (bank == nullptr) {
    // lg::error("play_sound_by_name: failed to find bank for sound {}", sound_name);
    return 0;
  }

  auto sound = bank->GetSoundByName(sound_name);
  if (sound.has_value()) {
    return PlaySound(bank, sound.value(), vol, pan, pm, pb);
  }

  // lg::error("play_sound_by_name: failed to find sound {}", sound_name);

  return 0;
}

void Player::StopSound(u32 sound_id) {
  std::scoped_lock lock(mTickLock);
  auto handler = mHandlers.find(sound_id);
  if (handler == mHandlers.end())
    return;

  handler->second->Stop();

  // m_handle_allocator.free_id(sound_id);
  // m_handlers.erase(sound_id);
}

u32 Player::GetSoundID(u32 sound_handle) {
  std::scoped_lock lock(mTickLock);
  auto handler = mHandlers.find(sound_handle);
  if (handler == mHandlers.end())
    return -1;
  return handler->second->SoundID();
}

void Player::SetSoundReg(u32 sound_id, u8 reg, u8 value) {
  std::scoped_lock lock(mTickLock);
  if (mHandlers.find(sound_id) == mHandlers.end()) {
    // fmt::print("set_midi_reg: Handler {} does not exist\n", sound_id);
    return;
  }

  auto* handler = mHandlers.at(sound_id).get();
  handler->SetRegister(reg, value);
}

u8 Player::GetSoundGroup(u32 sound_id) {
  std::scoped_lock lock(mTickLock);
  auto handler = mHandlers.find(sound_id);
  if (handler == mHandlers.end()) {
    return 0;
  }
  return handler->second->Group();
}

bool Player::SoundStillActive(u32 sound_id) {
  std::scoped_lock lock(mTickLock);
  auto handler = mHandlers.find(sound_id);
  if (handler == mHandlers.end())
    return false;

  // fmt::print("sound_still_active {}\n", sound_id);
  return true;
}

void Player::SetMasterVolume(u32 group, s32 volume) {
  std::scoped_lock lock(mTickLock);
  if (volume > 0x400)
    volume = 0x400;

  if (volume < 0)
    volume = 0;

  if (group == 15)
    return;

  mVmanager.SetMasterVol(group, volume);

  // Master volume
  if (group == 16) {
    // The synth's full-scale volume is 0x3fff (see the Synth ctor). Scale the
    // 0..0x400 slider range onto that. (The original constant here was 0x3ffff,
    // an 18-bit value: it overflowed Volume::Set(u16) - wrapping back to silence
    // every ~256 steps and, near the top, setting bit 15 = EnableSweep while
    // doubling the applied gain, which blew the output out.)
    mSynth.SetMasterVol(0x3fff * volume / 0x400);
  }
}

BankHandle Player::LoadBank(std::span<u8> bank) {
  std::scoped_lock lock(mTickLock);
  return mLoader.BankLoad(bank);
}

void Player::UnloadBank(BankHandle bank_handle) {
  std::scoped_lock lock(mTickLock);
  auto* bank = mLoader.GetBankByHandle(bank_handle);
  if (bank == nullptr)
    return;

  for (auto it = mHandlers.begin(); it != mHandlers.end();) {
    if (&it->second->Bank() == bank_handle) {
      mHandleAllocator.FreeId(it->first);
      it = mHandlers.erase(it);
    } else {
      ++it;
    }
  }

  mLoader.UnloadBank(bank_handle);
}

void Player::SetPanTable(VolPair* pantable) {
  std::scoped_lock lock(mTickLock);
  mVmanager.SetPanTable(pantable);
}

void Player::SetPlaybackMode(s32 mode) {
  std::scoped_lock lock(mTickLock);
  mVmanager.SetPlaybackMode(mode);
}

void Player::PauseSound(s32 sound_id) {
  std::scoped_lock lock(mTickLock);
  auto handler = mHandlers.find(sound_id);
  if (handler == mHandlers.end())
    return;

  handler->second->Pause();
}

void Player::ContinueSound(s32 sound_id) {
  std::scoped_lock lock(mTickLock);
  auto handler = mHandlers.find(sound_id);
  if (handler == mHandlers.end())
    return;

  handler->second->Unpause();
}

void Player::PauseAllSoundsInGroup(u8 group) {
  std::scoped_lock lock(mTickLock);

  for (auto& h : mHandlers) {
    if ((1 << h.second->Group()) & group) {
      h.second->Pause();
    }
  }
}

void Player::ContinueAllSoundsInGroup(u8 group) {
  std::scoped_lock lock(mTickLock);

  for (auto& h : mHandlers) {
    if ((1 << h.second->Group()) & group) {
      h.second->Unpause();
    }
  }
}

void Player::SetSoundVolPan(s32 sound_id, s32 vol, s32 pan) {
  std::scoped_lock lock(mTickLock);
  auto handler = mHandlers.find(sound_id);
  if (handler == mHandlers.end())
    return;

  handler->second->SetVolPan(vol, pan);
}

void Player::SetSoundPmod(s32 sound_handle, s32 mod) {
  std::scoped_lock lock(mTickLock);
  auto handler = mHandlers.find(sound_handle);
  if (handler == mHandlers.end())
    return;

  handler->second->SetPMod(mod);
}

void Player::StopAllSounds() {
  std::scoped_lock lock(mTickLock);
  for (auto it = mHandlers.begin(); it != mHandlers.end();) {
    mHandleAllocator.FreeId(it->first);
    it = mHandlers.erase(it);
  }
}

VoiceManager *Player::getVManager(){
  return &mVmanager;
}

s32 Player::GetSoundUserData(BankHandle block_handle,
                             char* block_name,
                             s32 sound_id,
                             char* sound_name,
                             SFXUserData* dst) {
  std::scoped_lock lock(mTickLock);
  SoundBank* bank = nullptr;
  if (block_handle == nullptr && block_name != nullptr) {
    bank = mLoader.GetBankByName(block_name);
  } else if (block_handle != nullptr) {
    bank = mLoader.GetBankByHandle(block_handle);
  } else {
    bank = mLoader.GetBankWithSound(sound_name);
  }

  if (bank == nullptr) {
    return 0;
  }

  if (sound_id == -1) {
    auto sound = bank->GetSoundByName(sound_name);
    if (sound.has_value()) {
      sound_id = sound.value();
    } else {
      return 0;
    }
  }

  auto ud = bank->GetSoundUserData(sound_id);
  if (ud.has_value()) {
    dst->data[0] = ud.value()->data[0];
    dst->data[1] = ud.value()->data[1];
    dst->data[2] = ud.value()->data[2];
    dst->data[3] = ud.value()->data[3];
    return 1;
  } else {
    return 0;
  }

  return 0;
}

}  // namespace snd
