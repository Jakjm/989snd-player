#include "third-party/imgui/imgui.h"
#include "third-party/imgui/imgui_internal.h"
#include "third-party/imgui/imgui_stdlib.h"

struct MidiTimelineParams{
    int startFrame = 0;
    float timelineZoom = 1.0;
};
void MidiTimeline(MidiTimelineParams &params);