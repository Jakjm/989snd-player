#include "midi_timeline.h"
#include <cmath>
#include <string>
#include "tracker_style.h"
#include "third-party/imgui/imgui.h"

const double ZOOM_MAX = 8;
const double ZOOM_MIN = 1.0 / ZOOM_MAX;
const int NUM_CHANNELS = 16;
const double TIMELINE_BOX_HEIGHT = 35.0;

void MidiTimeline(MidiTimelineParams &params){
    ImGui::BeginChild("miditimeline");
    ImGui::PushStyleColor(ImGuiCol_WindowBg, tracker::SCREEN_BG);
    const auto windowPos = ImGui::GetCursorScreenPos();
    const auto windowSize = ImGui::GetContentRegionAvail();
    const auto drawlist = ImGui::GetWindowDrawList();
    const auto io = ImGui::GetIO();

    const auto mousePos = io.MousePos;

    drawlist->AddRectFilled(windowPos, ImVec2(windowPos.x + windowSize.x,windowPos.y + windowSize.y), 0xFFFFFFFF);
    drawlist->AddRectFilled(windowPos, ImVec2(windowPos.x + windowSize.x,windowPos.y + TIMELINE_BOX_HEIGHT), 0xFFA0A0A0);


    if (ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
        params.beganClickingTimeline = false;
        params.stretchedInstanceRight = nullptr;
        params.stretchedInstanceLeft = nullptr;
        params.draggedInstance = nullptr;
    }
    else if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        if(windowPos.x <= mousePos.x && mousePos.x <= windowPos.x + windowSize.x && 
        windowPos.y <= mousePos.y && mousePos.y <= windowPos.y + TIMELINE_BOX_HEIGHT) {
            params.beganClickingTimeline = true;
            params.stretchedInstanceRight = nullptr;
            params.stretchedInstanceLeft = nullptr;
            params.draggedInstance = nullptr;
        }
        else{
            params.beganClickingTimeline = false;
        }
    }
    
    
    if (ImGui::IsMouseDragging(ImGuiMouseButton_Left))
    {
        //printf("Clicking Timeline: %d Dragged Instance %p\n", params.beganClickingTimeline, params.draggedInstance );

        ImVec2 dragDelta = ImGui::GetMouseDragDelta(ImGuiMouseButton_Left);
        ImVec2 mouseDelta = io.MouseDelta;
        if (params.beganClickingTimeline) {
            //printf("Drag Delta (%f,%f)  (int)(1.0 / params.timelineZoom) %d ", dragDelta.x, dragDelta.y, 
            // (int)(1.0 / params.timelineZoom));
            params.startFrameTenth -= (int)ceil((mouseDelta.x / params.timelineZoom));
            if(params.startFrameTenth < 0)
                params.startFrameTenth = 0;
            //else{
                //printf("Not dragging ");
                //}
        }
        else if(const auto stretchLeft = params.stretchedInstanceLeft)
        {
            stretchLeft->frameStart = params.instanceStartBeforeClick + (int)ceil(dragDelta.x / (params.timelineZoom * 10.0));
            if(stretchLeft->frameStart < 0)
                stretchLeft->frameStart = 0;
            else if(stretchLeft->frameStart >= stretchLeft->frameEnd - 1)
                stretchLeft->frameStart = stretchLeft->frameEnd - 1;
        }
        else if(const auto stretchRight = params.stretchedInstanceRight)
        {
            stretchRight->frameEnd = params.instanceEndBeforeClick + (int)ceil(dragDelta.x / (params.timelineZoom * 10.0));
            if(stretchRight->frameEnd <= stretchRight->frameStart + 1)
                stretchRight->frameEnd = stretchRight->frameStart + 1;
        }
        else if(const auto dragged = params.draggedInstance){
            dragged->frameStart = params.instanceStartBeforeClick + (int)ceil(dragDelta.x / (params.timelineZoom * 10.0));
            dragged->frameEnd = params.instanceEndBeforeClick + (int)ceil(dragDelta.x / (params.timelineZoom * 10.0));
            if(dragged->frameStart < 0)
            {
                dragged->frameStart = 0;
                dragged->frameEnd = (params.instanceEndBeforeClick - params.instanceStartBeforeClick);
            }
        }
    }
    //printf("Released: %d Mouse Down: %d Clicked: %d Began Clicking Timeline%d\n", ImGui::IsMouseReleased(ImGuiMouseButton_Left), ImGui::IsMouseDown(ImGuiMouseButton_Left), ImGui::IsMouseClicked(ImGuiMouseButton_Left), params.beganClickingTimeline);
    //printf("WindowSize:(%f,%f) WindowSize:(%f,%f) Mouse Pos:(%f,%f)\n", windowPos.x, windowPos.y, windowSize.x, windowSize.y, mousePos.x, mousePos.y);
    //If mouse is on widget 
    //Allow the timeline to be dragged left/right
    //Allow zooming in/out
    if(io.MouseWheel > 0.0 && params.timelineZoom < ZOOM_MAX){
        params.timelineZoom *= 1.05;
    }
    else if(io.MouseWheel < 0.0 && params.timelineZoom > ZOOM_MIN){
        params.timelineZoom *= 0.95;
    }

    //Draw the timeline
    int curFrameTenth = params.startFrameTenth;
    
    //Add a bit of margin
    auto currentPos = ImVec2(windowPos.x + 5, windowPos.y + 16);
    //Draw horizontal line to right side
    drawlist->AddLine(currentPos, ImVec2(windowPos.x + windowSize.x - 2, currentPos.y), 0xFF000000);

    //Account for being between two frames
    int tenthsUntilNext = curFrameTenth % 10;
    if(tenthsUntilNext != 0)
    {
        tenthsUntilNext = 10 - tenthsUntilNext;
        currentPos.x += (double)tenthsUntilNext * params.timelineZoom;
        curFrameTenth += tenthsUntilNext;
    }
    int firstFrame = curFrameTenth / 10;
    double spaceRemaining = windowPos.x + windowSize.x - 2.0 - currentPos.x;
    int lastFrame = curFrameTenth + (int)ceil(spaceRemaining / (10 * params.timelineZoom));
    

    //If curFrameTenth is divisible by this, draw the currentFrame line under the line.
    int frameNumberVisibleMultiple = 50 * (int)ceil(1.0 / params.timelineZoom);
    while(currentPos.x < windowPos.x + windowSize.x - 2) {
        double lineHeight;
        if(curFrameTenth % frameNumberVisibleMultiple == 0){
            lineHeight = 15;

            std::string frameNumberText = std::to_string(curFrameTenth / 10);
            double halfTextWidth = ImGui::CalcTextSize(frameNumberText.c_str()).x / 2.0;
            drawlist->AddText(ImVec2(currentPos.x - halfTextWidth, windowPos.y + 2), 0xFF000000, frameNumberText.c_str());
        }
        else{
            lineHeight = 10;
        }
        drawlist->AddLine(currentPos,ImVec2(currentPos.x, currentPos.y + lineHeight), 0xFF000000, 1.0);
        currentPos.x += 10 * params.timelineZoom;
        curFrameTenth += 10;
    }

    //TODO remove this
    

    for(SoundInstance &sound: params.sounds){
        if(params.startFrameTenth <= 10 * sound.frameEnd && sound.frameStart <= lastFrame){
            double startX = windowPos.x + 5;
            if(sound.frameStart >= firstFrame)
                startX += (double)(tenthsUntilNext + 10 * (sound.frameStart - firstFrame)) * params.timelineZoom;

            double endX = windowPos.x + windowSize.x - 2;
            if(sound.frameStart <= lastFrame)
                endX = windowPos.x + 5 + (double)(tenthsUntilNext + 10 * (sound.frameEnd - firstFrame)) * params.timelineZoom;
            
            auto start = ImVec2(startX, windowPos.y + 40 + 40 * sound.channel);
            auto end = ImVec2(endX, start.y + 40.0);

            drawlist->AddRectFilled(start, end, 0xFF0000FF, 0.2);

            //Check if potentially able to stretch or drag instance
            if(start.y <= mousePos.y && mousePos.y <= end.y)
            {
                //Can stretch left
                if(start.x <= mousePos.x && mousePos.x <= start.x + 20.0)
                {
                    drawlist->AddTriangleFilled(ImVec2(start.x + 2, start.y + 20.0), ImVec2(start.x + 12.0, start.y + 14.0), ImVec2(start.x + 12.0, start.y + 26.0), 0xFF000000);
                    if(ImGui::IsMouseClicked(ImGuiMouseButton_Left))
                    {
                        params.stretchedInstanceLeft = &sound;
                        params.stretchedInstanceRight = nullptr;
                        params.draggedInstance = nullptr;
                        params.instanceStartBeforeClick = sound.frameStart;
                        params.instanceEndBeforeClick = sound.frameEnd;
                    }
                }
                //Can stretch right
                else if(end.x - 20.0 <= mousePos.x && mousePos.x <= end.x)
                {
                    drawlist->AddTriangleFilled(ImVec2(end.x - 2, start.y + 20.0), ImVec2(end.x - 12.0, start.y + 14.0), ImVec2(end.x - 12.0, start.y + 26.0), 0xFF000000);
                    if(ImGui::IsMouseClicked(ImGuiMouseButton_Left))
                    {
                        params.stretchedInstanceLeft = nullptr;
                        params.stretchedInstanceRight = &sound;
                        params.draggedInstance = nullptr;
                        params.instanceStartBeforeClick = sound.frameStart;
                        params.instanceEndBeforeClick = sound.frameEnd;
                    }
                }
                //Can drag
                else if(start.x <= mousePos.x && mousePos.x <= end.x)
                {
                    drawlist->AddTriangle(ImVec2(start.x + 2, start.y + 20.0), ImVec2(start.x + 12.0, start.y + 14.0), ImVec2(start.x + 12.0, start.y + 26.0), 0xFF000000);
                    drawlist->AddTriangle(ImVec2(end.x - 2, start.y + 20.0), ImVec2(end.x - 12.0, start.y + 14.0), ImVec2(end.x - 12.0, start.y + 26.0), 0xFF000000);
                    if(ImGui::IsMouseClicked(ImGuiMouseButton_Left))
                    {
                        params.stretchedInstanceLeft = nullptr;
                        params.stretchedInstanceRight = nullptr;
                        params.draggedInstance = &sound;
                        params.instanceStartBeforeClick = sound.frameStart;
                        params.instanceEndBeforeClick = sound.frameEnd;
                    }
                }
            }
        }
    }

    currentPos = ImVec2(windowPos.x + 5, windowPos.y + 40);
    for(int channel = 0; channel < NUM_CHANNELS; ++channel ) {
        drawlist->AddRect(currentPos, ImVec2(windowPos.x + windowSize.x - 2, currentPos.y + 40), 0xFF000000, 0.2);
        currentPos.y += 40;
    }


    
    ImGui::PopStyleColor();
    ImGui::EndChild();
}