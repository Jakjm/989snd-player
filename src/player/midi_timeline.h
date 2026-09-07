#include "third-party/imgui/imgui.h"
#include "third-party/imgui/imgui_internal.h"
#include "third-party/imgui/imgui_stdlib.h"
#include <vector>
#include <map>
#include "sound/989snd/midi_handler.h"
#include "sound/989snd/musicbank.h"
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
    int tick = 0;
    int tempo = 0;
    int PPQ = 0;
    float timelineZoom = 1.0;
    std::vector<SoundInstance> notes = {SoundInstance(10,20,2,0,1), SoundInstance(5,35,0,1,0), SoundInstance(30, 40, 1, 0, 1)};
    int startTick = 0;
    bool beganClickingTimeline = false;
    SoundInstance *stretchedInstanceLeft = nullptr;
    SoundInstance *stretchedInstanceRight = nullptr;
    SoundInstance *draggedInstance = nullptr;
    int timelineStartBeforeClick = -1;
    std::map<SoundInstance*, std::pair<int,int>> selected;
};

void readMidiData(snd::MusicBank *bank, MidiTimelineParams &params);
void MidiTimeline(MidiTimelineParams &params);
void drawMidiTimeline(MidiTimelineParams &params);
void drawProgs(MidiTimelineParams &params);
//void DrawProgInstances(MidiTimelineParams &params);
//void DrawSoundRow();