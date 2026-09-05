#include "third-party/imgui/imgui.h"
#include "third-party/imgui/imgui_internal.h"
#include "third-party/imgui/imgui_stdlib.h"
#include <vector>
struct SoundInstance{
    int frameStart = 0;
    int frameEnd = 0;
    int soundNumber = 0;
    int channel = 0;
    SoundInstance(int frameStart, int frameEnd, int soundNumber, int channel) : frameStart(frameStart), frameEnd(frameEnd), soundNumber(soundNumber), channel(channel) {

    }
};

struct MidiTimelineParams{
    int startFrameTenth = 0;
    float timelineZoom = 1.0;
    bool beganClickingTimeline = false;
    std::vector<SoundInstance> sounds = {SoundInstance(10,20,0,1), SoundInstance(5,35,1,0), SoundInstance(30, 40, 0, 1)};
    SoundInstance *draggedInstance = nullptr;
    SoundInstance *stretchedInstanceLeft = nullptr;
    SoundInstance *stretchedInstanceRight = nullptr;
    int instanceStartBeforeClick = -1;
    int instanceEndBeforeClick = -1;
};
void MidiTimeline(MidiTimelineParams &params);
//void DrawProgInstances(MidiTimelineParams &params);
//void DrawSoundRow();