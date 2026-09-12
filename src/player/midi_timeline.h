#include <map>
#include <vector>

#include "sound/989snd/midi_handler.h"
#include "sound/989snd/musicbank.h"
#include "sound/989snd/vagvoice.h"
#include "sound/common/voice.h"

#include "third-party/imgui/imgui.h"
#include "third-party/imgui/imgui_internal.h"
#include "third-party/imgui/imgui_stdlib.h"
#include <memory>

static constexpr int tickrate = 240;
static constexpr int mics_per_tick = 1000000 / tickrate;

struct NoteInstance {
  int tickStart = 0;
  int tickEnd = 0;
  int program = 0;
  int velocity = 0;
  int note = 0;
  int channel = 0;

  std::shared_ptr<snd::midi_voice> voice;
  int voice_started_tick;
  NoteInstance(int tickStart, int tickEnd, int velocity, int program, int note, int channel)
      : tickStart(tickStart), tickEnd(tickEnd), velocity(velocity), program(program), note(note), channel(channel) {}

  void playNote(snd::MusicBank* bank, snd::MusicBank::MIDISound *sound, snd::VoiceManager *vm) {
    if(voice)
      return;
    auto& program = bank->Progs[this->program];

    for (auto& t : program.Tones) {
    //   if (t.MapLow <= note && note <= t.MapHigh) {
    //     s16 pan = m_chanpan[channel] + m_pan;
    //     if (pan >= 360) {
    //       pan -= 360;
    //     }

      auto chanvol = 0x7f;
      this->voice = std::make_shared<snd::midi_voice>(t, program);
      voice->basevol = vm->MakeVolumeB(chanvol, (velocity * chanvol) / 0x7f, 0,
                                        program.Vol, program.Pan, t.Vol, t.Pan);

      voice->note = note;
      voice->channel = channel;
      voice->velocity = velocity;

      voice->start_note = note;
      voice->start_fine = 0;

      // voice->current_pm = m_pitch_bend[channel];
      // voice->current_pb = m_cur_pm;

      voice->group = sound->VolGroup;
      vm->StartTone(voice);
      //m_voices.emplace_front(voice);
    }
  }
  void update(int curTick){
    if(curTick - this->voice_started_tick > this->tickEnd - this->tickStart){
      endNote();
    }
  }
  void endNote(){
    if(voice)
    {
      if (voice->channel == channel && voice->note == note) {
        voice->KeyOff();
      }
      voice = nullptr;
    }
  }
};

  struct MidiTimelineParams {
    snd::MusicBank* bank;
    snd::VoiceManager* manager;
    int playback_tick = 0;
    int tempo = 500000;  // Number of microseconds per quarter note
    int PPQ = 480;       // Number of ticks per quarter note
    u8 registers[16];
    u8* macros[16];
    std::vector<std::vector<NoteInstance>> notes = {{NoteInstance(120, 960, 1, 2, 0, 1),
                                                     NoteInstance(360, 540, 1, 0, 1, 0),
                                                     NoteInstance(600, 840, 1, 1, 0, 0)}};

    float timelineZoom = 1.0;
    int startTick = 0;  // MIDI time tick =
    bool beganClickingTimeline = false;
    int stretchedInstanceLeft = -1;
    int stretchedInstanceRight = -1;
    int draggedInstance = -1;
    int timelineStartBeforeClick = -1;
    int tabSelected = -1;
    std::map<int, std::pair<int, int>> selected;

    void syncWithPlayerTick(int player_tick) {
      int ticks_per_second = PPQ * 1000000 / tempo;
      playback_tick = (ticks_per_second * player_tick / tickrate);
    }
  };

  void readBank(snd::MusicBank* bank, MidiTimelineParams& params);
  void readMidiData(snd::Midi& midi, MidiTimelineParams& params);
  void MidiTimeline(MidiTimelineParams& params);
  void drawMidiTimeline(MidiTimelineParams& params);
  void drawProgs(MidiTimelineParams& params);

  // void DrawProgInstances(MidiTimelineParams &params);
  // void DrawSoundRow();