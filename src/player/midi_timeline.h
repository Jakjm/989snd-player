#include "third-party/imgui/imgui.h"
#include "third-party/imgui/imgui_internal.h"
#include "third-party/imgui/imgui_stdlib.h"
#include <vector>
#include <map>
#include "sound/989snd/midi_handler.h"
#include "sound/989snd/musicbank.h"

static constexpr int tickrate = 240;
static constexpr int mics_per_tick = 1000000 / tickrate;

struct SoundInstance{
    int tickStart = 0;
    int tickEnd = 0;
    int program = 0;
    int note = 0;
    int channel = 0;
    SoundInstance(int tickStart, int tickEnd, int program, int note, int channel) : tickStart(tickStart), tickEnd(tickEnd), program(program), note(note), channel(channel) {

    }
};

struct MidiTimelineParams{
    snd::MusicBank *bank;
    int playback_tick = 0;
    int tempo = 1; //Number of microseconds per quarter note
    int PPQ = 480; //Number of ticks per quarter note
    float timelineZoom = 1.0;
    std::vector<std::vector<SoundInstance>> notes = {{SoundInstance(120,960,2,0,1), SoundInstance(360,540,0,1,0), SoundInstance(600, 840, 1, 0, 0)}};
    int startTick = 0; //MIDI time tick = 
    bool beganClickingTimeline = false;
    int stretchedInstanceLeft = -1;
    int stretchedInstanceRight = -1;
    int draggedInstance = -1;
    int timelineStartBeforeClick = -1;
    std::map<int, std::pair<int,int>> selected;

    void syncWithPlayerTick(int player_tick){
        int ticks_per_second = PPQ * 1000000 / tempo;
        playback_tick = (ticks_per_second * player_tick / tickrate); 
    }
};

void readBank(snd::MusicBank *bank, MidiTimelineParams &params);
void readMidiData(snd::Midi &midi, MidiTimelineParams &params);
void MidiTimeline(MidiTimelineParams &params);
void drawMidiTimeline(MidiTimelineParams &params);
void drawProgs(MidiTimelineParams &params);

//void DrawProgInstances(MidiTimelineParams &params);
//void DrawSoundRow();