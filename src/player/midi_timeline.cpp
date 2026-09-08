#include "midi_timeline.h"
#include <cmath>
#include <string>
#include "tracker_style.h"
#include "third-party/imgui/imgui.h"
#include "third-party/imgui/imgui_internal.h"
#include "fmt/format.h"
#include <iterator>
#include <variant>
#include "sound/989snd/musicbank.h"

const double ZOOM_MAX = 16;
const double ZOOM_MIN = 1.0 / 8000.0;
const int NUM_CHANNELS = 16;
const double TIMELINE_BOX_HEIGHT = 35.0;

void readBank(snd::MusicBank *bank, MidiTimelineParams &params){
    params.notes.clear();

    //TODO: read multi midi, add a tab for each one...
    if(std::holds_alternative<snd::Midi>(bank->MidiData))
        readMidiData(std::get<snd::Midi>(bank->MidiData), params);
    else if(std::holds_alternative<snd::MultiMidi>(bank->MidiData)){
        auto multiMidi = std::get<snd::MultiMidi>(bank->MidiData);
        for(auto& midi : multiMidi.midi){
            readMidiData(midi, params);
        }
    }
}

void readMidiData(snd::Midi &midi,  MidiTimelineParams &params){
    auto &notes = params.notes.emplace_back();
    u8 *dataStart = midi.DataStart;
    u8 *curData = dataStart;
    u64 time = 0;

    std::array<std::map<int,int>,NUM_CHANNELS> channel_notes;
    std::array<int,NUM_CHANNELS> channel_programs;
    for(int i = 0; i < NUM_CHANNELS;++i)
        channel_programs[i] = -1;

    params.tempo = midi.Tempo; //micros per quarter note
    params.PPQ = midi.PPQ;
    u64 ppt = 100 * mics_per_tick / (params.tempo / midi.PPQ);
    u64 tickDelta = 0, tickError = 0, tickCountdown;
    u8 status_byte;
    do{
        auto [len, delta] = snd::MidiHandler::ReadVLQ(curData);
        curData += len;
        time += delta;

        tickDelta = 100 * delta + tickError;
        if (tickDelta < 0 || tickDelta < ppt / 2) {
            tickError = tickDelta;
            tickDelta = 0;
        }
        if (tickCountdown != 0) {
            tickCountdown = (tickDelta / 100 * midi.Tempo / midi.PPQ - 1 + mics_per_tick) / mics_per_tick;
            tickError = tickDelta - ppt * tickCountdown;
        }

        if(*curData & 0x80)
        {
            status_byte = *curData;
            ++curData;
        }

        //assert(status_byte & 0x80);
        int firstMeta = 0;
        switch(status_byte >> 4){
            case 0x9:
            {
                //NoteON
                //break;
                //break;
                u8 channel = status_byte & 0xF;
                u8 note = *curData;
                u8 velocity = *(curData + 1);
                u8 program = channel_programs[channel];
                
                //If velocity is non-zero, fall through to note-off
                if(velocity != 0)
                {
                    curData += 2; 
                    notes.emplace_back(time,-1,program,note,channel);
                    channel_notes[channel][note] = notes.size() - 1;
                    break;
                }
            }   
            case 0x8:
            {
                //NoteOFF
                //assert(channel_notes[channel] != nullptr);
                u8 channel = status_byte & 0xF;
                u8 note = *curData;

                notes[channel_notes[channel][note]].tickEnd = time;
                channel_notes[channel][note] = -1;
                curData += 2;
                break;
            }
            case 0xB:
                curData += 2;
                break;
                //ControllerChange();
                //break;
            case 0xD:
                //TODO: actually do stuff
                ++curData;
                break;
                //ChannelPressure();
                //break;
            case 0xC:
            {
                u8 channel = status_byte & 0xf;
                u8 program = *curData;

                channel_programs[channel] = program;
                ++curData;
                break;
            }
            case 0xE:
                curData += 2;
                break;
                //ChannelPitch();
                //break;
            case 0xF:
                // normal meta-event
                if (status_byte == 0xFF) {//META Event means it's time to repeat or something
                    // MetaEvent();
                    // break;
                    size_t len = *(curData + 1);
                    if(*curData == 0x2f)
                        return; //Break out before looping
                    else if(*curData == 0x51)
                    {
                        params.tempo = (curData[2] << 16) | (curData[3] << 8) | (curData[4]);
                        ppt = 100 * mics_per_tick / (params.tempo / midi.PPQ);
                    }
                    curData += len + 2;
                    break;
                }
                if (status_byte == 0xF0) {//SYSTEM EVENT means AME handler trying to do something

                    if(*curData == 0x75)
                        ++curData;
                        //TODO: stopped by AME.
                    else
                        return; //Unknown system event
                    // break;
                    break;
                }
            [[fallthrough]];
            default:
            {
                return;
                // throw MidiError(fmt::format("invalid status {}", status_byte));
                // return;
            }
        }
    }while(1);
    return;
}

void drawProgs(MidiTimelineParams &params){

}

void drawMidiTimeline(MidiTimelineParams &params){
    const auto topline = fmt::format("Tempo (micros per quarter note): {} PPQ: {}", params.tempo, params.PPQ);
    ImGui::Text(topline.c_str());

    const auto drawlist = ImGui::GetWindowDrawList();
    const auto io = ImGui::GetIO();

    const int ticksPerMeasure = 4 * params.PPQ;
    const double quarterNoteWidth = 10.0 * params.timelineZoom;
    const double measureWidth = 4.0 * quarterNoteWidth;
    const double tickWidth = measureWidth / ticksPerMeasure;

        
    //Allow the timeline to be dragged left/right
    //Allow zooming in/out
    if(io.MouseWheel > 0.0 && params.timelineZoom < ZOOM_MAX){
        params.timelineZoom *= 1.05;
    }
    else if(io.MouseWheel < 0.0 && params.timelineZoom > ZOOM_MIN){
        params.timelineZoom *= 0.95;
    }
    
    bool clickedButton = false;
    //Add a tab for each midi
    if(ImGui::BeginTabBar("MIDIS")) {
        for(auto it = params.notes.begin(); it != params.notes.end(); ++it)
        {
            auto midi_index = std::distance(params.notes.begin(), it);
            auto &notes = *it;
            if(ImGui::BeginTabItem(("Midi " + std::to_string(midi_index)).c_str())) { 
                const auto windowPos = ImGui::GetCursorScreenPos();
                const auto windowSize = ImGui::GetContentRegionAvail();
                const auto mousePos = io.MousePos;
                auto currentPos = ImVec2(windowPos.x + 5, windowPos.y + 16);
                    
                if (ImGui::IsMouseDragging(ImGuiMouseButton_Left))
                {
                    ImVec2 dragDelta = ImGui::GetMouseDragDelta(ImGuiMouseButton_Left);
                    if (params.beganClickingTimeline) {
                        params.startTick = params.timelineStartBeforeClick - (int)ceil(dragDelta.x / tickWidth);
                        if(params.startTick < 0)
                            params.startTick = 0;
                    }
                    else if(params.stretchedInstanceLeft != -1)
                    {
                        auto &stretchLeft = notes[params.stretchedInstanceLeft];
                        stretchLeft.tickStart = params.selected[params.stretchedInstanceLeft].first + (int)ceil(dragDelta.x / tickWidth);
                        if(stretchLeft.tickStart < 0)
                            stretchLeft.tickStart = 0;
                        else if(stretchLeft.tickStart >= stretchLeft.tickEnd - 1)
                            stretchLeft.tickStart = stretchLeft.tickEnd - 1;
                    }
                    else if(params.stretchedInstanceRight != -1)
                    {
                        auto &stretchRight = notes[params.stretchedInstanceRight];
                        stretchRight.tickEnd = params.selected[params.stretchedInstanceRight].second + (int)ceil(dragDelta.x / tickWidth);
                        if(stretchRight.tickEnd <= stretchRight.tickStart + 1)
                            stretchRight.tickEnd = stretchRight.tickStart + 1;
                    }
                    else if(const auto draggedInstance = params.draggedInstance){
                        for(auto selectedIter : params.selected)
                        {
                            auto& selected = notes[selectedIter.first];
                            int tickStartAtClick = selectedIter.second.first;
                            int tickEndAtClick = selectedIter.second.second;
                            selected.tickStart = tickStartAtClick + (int)ceil(dragDelta.x / tickWidth);
                            selected.tickEnd = tickEndAtClick + (int)ceil(dragDelta.x / tickWidth);
                            if(selected.tickStart < 0)
                            {
                                selected.tickStart = 0;
                                selected.tickEnd = (tickEndAtClick - tickStartAtClick);
                            }
                        }
                    }
                }

                if (ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
                    params.beganClickingTimeline = false;
                    params.stretchedInstanceRight = -1;
                    params.stretchedInstanceLeft = -1;
                    params.draggedInstance = -1;

                    //Update initial start/end of selected nodes 
                    for(auto& iter : params.selected) {
                        auto note = notes[iter.first];
                        iter.second.first = note.tickStart;
                        iter.second.second = note.tickEnd;
                    }
                }
                else if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                    if(windowPos.x <= mousePos.x && mousePos.x <= windowPos.x + windowSize.x && 
                    windowPos.y <= mousePos.y && mousePos.y <= windowPos.y + TIMELINE_BOX_HEIGHT) {
                        params.beganClickingTimeline = true;
                        params.stretchedInstanceRight = -1;
                        params.stretchedInstanceLeft = -1;
                        params.draggedInstance = -1;
                        params.timelineStartBeforeClick = params.startTick;
                    }
                    else{
                        params.beganClickingTimeline = false;
                    }
                }

                //Timeline drawing code:
                drawlist->AddRectFilled(windowPos, ImVec2(windowPos.x + windowSize.x,windowPos.y + TIMELINE_BOX_HEIGHT), 0xFFA0A0A0);
                //Draw horizontal line to right side
                drawlist->AddLine(currentPos, ImVec2(windowPos.x + windowSize.x - 2, currentPos.y), 0xFF000000);

                int firstTick = params.startTick;
                int curTick = params.startTick;

                //Account for being between quarter notes:
                double distanceToFirstQuarter = 0.0;
                if(int remainder = firstTick % params.PPQ; remainder != 0)
                {
                    //Round up to next quarter note
                    curTick += (params.PPQ - remainder);
                    distanceToFirstQuarter = (params.PPQ - remainder) * tickWidth;
                    currentPos.x += distanceToFirstQuarter;
                }

                double spaceRemaining = windowPos.x + windowSize.x - 2.0 - currentPos.x;
                int lastTick = curTick + (int)ceil(spaceRemaining / tickWidth);
                int lastMeasure = lastTick / ticksPerMeasure;

                //If curTickTenth is divisible by this, draw the currentTick line under the line.
                const auto numberSize = ImGui::CalcTextSize(std::to_string(lastMeasure).c_str());
                int measureNumberVisibleMultiple = ticksPerMeasure * (int)ceil(2 * numberSize.x / measureWidth);
                //printf("%d\n", measureNumberVisibleMultiple);
                while(currentPos.x < windowPos.x + windowSize.x - 2) {
                    double lineHeight;
                    if(curTick % measureNumberVisibleMultiple == 0) {
                        lineHeight = 15;
                    
                        int curMeasure = curTick / ticksPerMeasure;
                        std::string measureNumberText = std::to_string(curMeasure);
                        double halfTextWidth = ImGui::CalcTextSize(measureNumberText.c_str()).x / 2.0;
                        drawlist->AddText(ImVec2(currentPos.x - halfTextWidth, windowPos.y + 2), 0xFF000000, measureNumberText.c_str());
                    }
                    else{
                        lineHeight = 10;
                    }
                    drawlist->AddLine(currentPos,ImVec2(currentPos.x, currentPos.y + lineHeight), 0xFF000000, 1.0);

                    //Advance by one eigth note
                    currentPos.x += quarterNoteWidth;
                    curTick += params.PPQ;
                }

                for (auto iter = notes.begin(); iter != notes.end(); ++iter) {
                    int index = std::distance(notes.begin(), iter);
                    auto &instance = *iter;

                //for(SoundInstance &instance: notes){
                    if(params.startTick <= instance.tickEnd && instance.tickStart <= lastTick){
                        double startX = windowPos.x + 5;
                        if(instance.tickStart >= params.startTick)
                            startX += (double)(tickWidth * (instance.tickStart - firstTick));

                        double endX = windowPos.x + windowSize.x - 2;
                        if(instance.tickStart <= lastTick)
                            endX = windowPos.x + 5 + (double)(tickWidth * (instance.tickEnd - firstTick));
                        
                        auto start = ImVec2(startX, windowPos.y + TIMELINE_BOX_HEIGHT + 40 * instance.channel);
                        auto end = ImVec2(endX, start.y + 40.0);

                        //Draw a rectangle for the instance
                        drawlist->AddRectFilled(start, end, 0xFF0000FF, 0.4);
                        drawlist->AddRect(start, end, 0xFF000000, 0.4, 1.5);

                        bool mouseOverlapsInstanceY = start.y <= mousePos.y && mousePos.y <= end.y;
                        bool mouseOverlapsInstanceX = start.x <= mousePos.x && mousePos.x <= end.x;
                        bool mouseOverlapsInstance = mouseOverlapsInstanceX && mouseOverlapsInstanceY;
                        bool mouseOverlapsInstanceStart = (start.x <= mousePos.x) && (mousePos.x <= start.x + 20.0) && mouseOverlapsInstanceY;
                        bool mouseOverlapsInstanceEnd = (end.x - 20.0 <= mousePos.x) && (mousePos.x <= end.x) && mouseOverlapsInstanceY;
                        
                        //Check if potentially able to stretch or drag instance
                        //Can stretch left
                        if(params.stretchedInstanceLeft == index || mouseOverlapsInstanceStart)
                        {
                            drawlist->AddTriangleFilled(ImVec2(start.x + 2, start.y + 20.0), ImVec2(start.x + 12.0, start.y + 14.0), ImVec2(start.x + 12.0, start.y + 26.0), 0xFF000000);
                            if(ImGui::IsMouseClicked(ImGuiMouseButton_Left) && mouseOverlapsInstanceStart)
                            {
                                clickedButton = true;
                                drawlist->AddTriangleFilled(ImVec2(start.x + 2, start.y + 20.0), ImVec2(start.x + 12.0, start.y + 14.0), ImVec2(start.x + 12.0, start.y + 26.0), 0xFF000000);
                                params.stretchedInstanceLeft = index;
                                params.stretchedInstanceRight = -1;
                                params.draggedInstance = -1;

                                if(!params.selected.contains(index))
                                {
                                    if(!ImGui::IsKeyDown(ImGuiKey_LeftCtrl))
                                        params.selected.clear();
                                    params.selected.insert({index,{instance.tickStart,instance.tickEnd}});
                                }
                            }
                        }
                        //Can stretch right
                        else if(params.stretchedInstanceRight == index || mouseOverlapsInstanceEnd)
                        {
                            drawlist->AddTriangleFilled(ImVec2(end.x - 2, start.y + 20.0), ImVec2(end.x - 12.0, start.y + 14.0), ImVec2(end.x - 12.0, start.y + 26.0), 0xFF000000);
                            if(ImGui::IsMouseClicked(ImGuiMouseButton_Left) && mouseOverlapsInstanceEnd)
                            {
                                clickedButton = true;
                                params.stretchedInstanceLeft = -1;
                                params.stretchedInstanceRight = index;
                                params.draggedInstance = -1;

                                if(!params.selected.contains(index))
                                {
                                    if(!ImGui::IsKeyDown(ImGuiKey_LeftCtrl))
                                        params.selected.clear();
                                    params.selected.insert({index,{instance.tickStart,instance.tickEnd}});
                                }
                            }
                        }
                        //Can drag
                        else if(params.selected.contains(index) || mouseOverlapsInstance)
                        {
                            drawlist->AddTriangle(ImVec2(start.x + 2, start.y + 20.0), ImVec2(start.x + 12.0, start.y + 14.0), ImVec2(start.x + 12.0, start.y + 26.0), 0xFF000000);
                            drawlist->AddTriangle(ImVec2(end.x - 2, start.y + 20.0), ImVec2(end.x - 12.0, start.y + 14.0), ImVec2(end.x - 12.0, start.y + 26.0), 0xFF000000);
                            if(ImGui::IsMouseClicked(ImGuiMouseButton_Left) && mouseOverlapsInstance)
                            {
                                clickedButton = true;
                                params.stretchedInstanceLeft = -1;
                                params.stretchedInstanceRight = -1;
                                params.draggedInstance = index;
                                
                                if(!params.selected.contains(index))
                                {
                                    if(!ImGui::IsKeyDown(ImGuiKey_LeftCtrl))
                                        params.selected.clear();
                                    params.selected.insert({index,{instance.tickStart,instance.tickEnd}});
                                }
                            }
                        }
                        if(auto iter = params.selected.find(index); iter != params.selected.end())
                        {
                            if(mouseOverlapsInstance && ImGui::IsMouseClicked(ImGuiMouseButton_Right))
                                params.selected.erase(iter);
                            else
                            {
                                drawlist->AddRect(start, end, 0xFFFF0000, 0.4, 3);
                                auto index = std::distance(params.selected.begin(), iter);
                                std::string selectionNumberText = "(" + std::to_string(index) + ")";
                                auto textSize = ImGui::CalcTextSize(selectionNumberText.c_str());
                                drawlist->AddText(ImVec2(start.x + (end.x - start.x - textSize.x) / 2.0, start.y + 20.0 - textSize.y / 2.0), 0xFF000000, selectionNumberText.c_str());
                            }
                        }
                    }
                }

                //Draw rectangles around each channel row
                currentPos = ImVec2(windowPos.x + 5, windowPos.y + TIMELINE_BOX_HEIGHT);
                for(int channel = 0; channel < NUM_CHANNELS; ++channel ) {
                    drawlist->AddRect(currentPos, ImVec2(windowPos.x + windowSize.x - 2, currentPos.y + 41), 0xFF000000, 0.2);

                    const auto channelText = std::string("Channel ") + std::to_string(channel);
                    const auto textSize = ImGui::CalcTextSize(channelText.c_str());

                    drawlist->AddText(ImVec2(windowPos.x + 5 + (windowSize.x - textSize.x) / 2.0, currentPos.y + 20 - textSize.y / 2.0), 0x77000000, channelText.c_str());
                    
                    currentPos.y += 40;
                }

                //Create a window for customizing currently selected instance.
                bool clickedProperties = false;
                if(!params.selected.empty()){
                    ImGui::SetNextWindowSizeConstraints(ImVec2(300,400), ImVec2(300,400));
                    ImGui::SetNextWindowPos(ImVec2(windowPos.x + windowSize.x, windowPos.y + windowSize.y), ImGuiCond_Always, ImVec2(0.95,0.95));
                    bool open = true;
                    ImGui::PushStyleColor(ImGuiCol_Text, tracker::SCREEN_FG);
                    ImGui::PushStyleColor(ImGuiCol_WindowBg, tracker::CREAM);
                    ImGui::Begin(params.selected.size() > 1 ? "SelectedInstances" : "Selected Instance", 
                    &open, ImGuiWindowFlags_AlwaysAutoResize 
                    | ImGuiWindowFlags_NoSavedSettings 
                    | ImGuiWindowFlags_NoFocusOnAppearing 
                    | ImGuiWindowFlags_NoNav 
                    | ImGuiWindowFlags_NoMove);
                    
                    const auto windowPos = ImGui::GetCursorScreenPos();
                    const auto windowSize = ImGui::GetContentRegionAvail();
                    
                    if(ImGui::BeginTabBar("Note Selections")){
                        int index = 0;
                        for(auto iter : params.selected)
                        {
                            auto& instance = notes[iter.first];
                            std::string selectionNumberText = "(" + std::to_string(index) + ")";
                            auto textSize = ImGui::CalcTextSize(selectionNumberText.c_str());
                            if (ImGui::BeginTabItem(selectionNumberText.c_str()))
                            {
                                ImGui::SetNextItemWidth(150.0);
                                ImGui::SliderInt("Start Tick", &instance.tickStart, 0, instance.tickEnd - 1);
                                ImGui::SetNextItemWidth(150.0);
                                ImGui::SliderInt("End Tick", &instance.tickEnd, instance.tickStart + 1, lastTick);
                                ImGui::SetNextItemWidth(150.0);
                                ImGui::SliderInt("Program",&instance.program, 0, NUM_CHANNELS);
                                ImGui::SetNextItemWidth(150.0);
                                ImGui::SliderInt("Note",&instance.note, 0, NUM_CHANNELS);
                                ImGui::EndTabItem();
                            }
                            ++index;
                        }
                        ImGui::EndTabBar();
                    }
            
                    //printf("mousePos (%f %f) windowPos (%f %f) windowSize (%f %f) \n", mousePos.x, mousePos.y, windowPos.x, windowPos.y, windowSize.x, windowSize.y);
                    if(windowPos.x <= mousePos.x && mousePos.x <= windowPos.x + windowSize.x &&
                        windowPos.y <= mousePos.y && mousePos.y <= windowPos.y + windowSize.y && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
                        clickedProperties = true;
            
                    ImGui::End();
                    ImGui::PopStyleColor();
                    ImGui::PopStyleColor();
                }
                if(ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !params.stretchedInstanceLeft && !params.stretchedInstanceRight && 
                    !params.beganClickingTimeline && !clickedButton && !clickedProperties) 
                {
                    params.selected.clear();
                }
            
                //Draw a line indicating current tick.
                if(firstTick <= params.playback_tick  && params.playback_tick <= lastTick)
                {
                    currentPos = ImVec2(windowPos.x + 5.0 + (params.playback_tick - firstTick) * tickWidth, windowPos.y + 16.0);
                    drawlist->AddLine(currentPos, ImVec2(currentPos.x, windowPos.y + TIMELINE_BOX_HEIGHT + 40 * NUM_CHANNELS), 0xFF00FF00, 3.0);
                }

                ImGui::EndTabItem();
            }
        }
        ImGui::EndTabBar();
    }
}

void MidiTimeline(MidiTimelineParams &params){
    ImGui::PushStyleColor(ImGuiCol_ChildBg, tracker::CREAM);
    ImGui::BeginChild("miditimeline");

    if(ImGui::BeginTabBar("MidiTimelineBar")){
        if (ImGui::BeginTabItem("Timeline"))
        {
            drawMidiTimeline(params);
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Progs"))
        {
            drawProgs(params);
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }
    ImGui::PopStyleColor();
    ImGui::EndChild();
}