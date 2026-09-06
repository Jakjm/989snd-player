#include "midi_timeline.h"
#include <cmath>
#include <string>
#include "tracker_style.h"
#include "third-party/imgui/imgui.h"
#include "third-party/imgui/imgui_internal.h"

const double ZOOM_MAX = 8;
const double ZOOM_MIN = 1.0 / ZOOM_MAX;
const int NUM_CHANNELS = 16;
const double TIMELINE_BOX_HEIGHT = 35.0;

void MidiTimeline(MidiTimelineParams &params){
    ImGui::PushStyleColor(ImGuiCol_ChildBg, tracker::CREAM);
    ImGui::BeginChild("miditimeline");
    const auto windowPos = ImGui::GetCursorScreenPos();
    const auto windowSize = ImGui::GetContentRegionAvail();
    const auto drawlist = ImGui::GetWindowDrawList();
    const auto io = ImGui::GetIO();

    const auto mousePos = io.MousePos;

    //drawlist->AddRectFilled(windowPos, ImVec2(windowPos.x + windowSize.x,windowPos.y + windowSize.y), 0xFFFFFFFF);
    drawlist->AddRectFilled(windowPos, ImVec2(windowPos.x + windowSize.x,windowPos.y + TIMELINE_BOX_HEIGHT), 0xFFA0A0A0);


    if (ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
        params.beganClickingTimeline = false;
        params.stretchedInstanceRight = nullptr;
        params.stretchedInstanceLeft = nullptr;
        params.draggedInstance = nullptr;

        for(auto& iter : params.selected)
        {
            iter.second.first = iter.first->frameStart;
            iter.second.second = iter.first->frameEnd;
        }
    }
    else if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        if(windowPos.x <= mousePos.x && mousePos.x <= windowPos.x + windowSize.x && 
        windowPos.y <= mousePos.y && mousePos.y <= windowPos.y + TIMELINE_BOX_HEIGHT) {
            params.beganClickingTimeline = true;
            params.stretchedInstanceRight = nullptr;
            params.stretchedInstanceLeft = nullptr;
            params.draggedInstance = nullptr;
            params.timelineStartBeforeClick = params.startFrameTenth;
        }
        else{
            params.beganClickingTimeline = false;
        }
    }
    
    
    if (ImGui::IsMouseDragging(ImGuiMouseButton_Left))
    {
        ImVec2 dragDelta = ImGui::GetMouseDragDelta(ImGuiMouseButton_Left);
        
        if (params.beganClickingTimeline) {
            params.startFrameTenth = params.timelineStartBeforeClick - (int)ceil(dragDelta.x / params.timelineZoom);
            if(params.startFrameTenth < 0)
                params.startFrameTenth = 0;
        }
        else if(const auto stretchLeft = params.stretchedInstanceLeft; stretchLeft && params.selected.size() == 1)
        {
            stretchLeft->frameStart = params.selected[stretchLeft].first + (int)ceil(dragDelta.x / (params.timelineZoom * 10.0));
            if(stretchLeft->frameStart < 0)
                stretchLeft->frameStart = 0;
            else if(stretchLeft->frameStart >= stretchLeft->frameEnd - 1)
                stretchLeft->frameStart = stretchLeft->frameEnd - 1;
        }
        else if(const auto stretchRight = params.stretchedInstanceRight; stretchRight && params.selected.size() == 1)
        {
            stretchRight->frameEnd = params.selected[stretchRight].second + (int)ceil(dragDelta.x / (params.timelineZoom * 10.0));
            if(stretchRight->frameEnd <= stretchRight->frameStart + 1)
                stretchRight->frameEnd = stretchRight->frameStart + 1;
        }
        else if(const auto draggedInstance = params.draggedInstance){
            for(auto selectedIter : params.selected)
            {
                int frameStartAtClick = selectedIter.second.first;
                int frameEndAtClick = selectedIter.second.second;
                selectedIter.first->frameStart = frameStartAtClick + (int)ceil(dragDelta.x / (params.timelineZoom * 10.0));
                selectedIter.first->frameEnd = frameEndAtClick + (int)ceil(dragDelta.x / (params.timelineZoom * 10.0));
                if(selectedIter.first->frameStart < 0)
                {
                    selectedIter.first->frameStart = 0;
                    selectedIter.first->frameEnd = (frameEndAtClick - frameStartAtClick);
                }
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
    
    
    //Timeline drawing code:
    auto currentPos = ImVec2(windowPos.x + 5, windowPos.y + 16);

    
    //Draw the timeline
    int curFrameTenth = params.startFrameTenth;
    
    //Add a bit of margin
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

    currentPos = ImVec2(windowPos.x + 5.0, windowPos.y + 16.0);
    drawlist->AddLine(currentPos, ImVec2(currentPos.x, windowPos.y + TIMELINE_BOX_HEIGHT + 40 * NUM_CHANNELS), 0xFF0000FF, 3.0);

    bool clickedButton = false;
    for(SoundInstance &instance: params.sounds){
        if(params.startFrameTenth <= 10 * instance.frameEnd && instance.frameStart <= lastFrame){
            double startX = windowPos.x + 5;
            if(instance.frameStart >= firstFrame)
                startX += (double)(tenthsUntilNext + 10 * (instance.frameStart - firstFrame)) * params.timelineZoom;

            double endX = windowPos.x + windowSize.x - 2;
            if(instance.frameStart <= lastFrame)
                endX = windowPos.x + 5 + (double)(tenthsUntilNext + 10 * (instance.frameEnd - firstFrame)) * params.timelineZoom;
            
            auto start = ImVec2(startX, windowPos.y + TIMELINE_BOX_HEIGHT + 40 * instance.channel);
            auto end = ImVec2(endX, start.y + 40.0);

            //Draw a rectangle for the instance
            drawlist->AddRectFilled(start, end, 0xFF0000FF, 0.4);

            bool mouseOverlapsInstanceY = start.y <= mousePos.y && mousePos.y <= end.y;
            bool mouseOverlapsInstanceX = start.x <= mousePos.x && mousePos.x <= end.x;
            bool mouseOverlapsInstance = mouseOverlapsInstanceX && mouseOverlapsInstanceY;
            bool mouseOverlapsInstanceStart = start.x <= mousePos.x && mousePos.x <= start.x + 20.0 && mouseOverlapsInstanceY;
            bool mouseOverlapsInstanceEnd = end.x - 20.0 <= mousePos.x && mousePos.x <= end.x && mouseOverlapsInstanceEnd;
            
            //Check if potentially able to stretch or drag instance
            //Can stretch left
            if(params.stretchedInstanceLeft == &instance || mouseOverlapsInstanceStart)
            {
                drawlist->AddTriangleFilled(ImVec2(start.x + 2, start.y + 20.0), ImVec2(start.x + 12.0, start.y + 14.0), ImVec2(start.x + 12.0, start.y + 26.0), 0xFF000000);
                if(ImGui::IsMouseClicked(ImGuiMouseButton_Left) && mouseOverlapsInstanceStart)
                {
                    clickedButton = true;
                    drawlist->AddTriangleFilled(ImVec2(start.x + 2, start.y + 20.0), ImVec2(start.x + 12.0, start.y + 14.0), ImVec2(start.x + 12.0, start.y + 26.0), 0xFF000000);
                    params.stretchedInstanceLeft = &instance;
                    params.stretchedInstanceRight = nullptr;
                    params.draggedInstance = nullptr;

                    if(!params.selected.contains(&instance))
                    {
                        if(!ImGui::IsKeyDown(ImGuiKey_LeftCtrl))
                            params.selected.clear();
                        params.selected.insert({&instance,{instance.frameStart,instance.frameEnd}});
                    }
                }
            }
            //Can stretch right
            else if(params.stretchedInstanceRight == &instance || mouseOverlapsInstanceEnd)
            {
                drawlist->AddTriangleFilled(ImVec2(end.x - 2, start.y + 20.0), ImVec2(end.x - 12.0, start.y + 14.0), ImVec2(end.x - 12.0, start.y + 26.0), 0xFF000000);
                if(ImGui::IsMouseClicked(ImGuiMouseButton_Left) && mouseOverlapsInstanceEnd)
                {
                    clickedButton = true;
                    params.stretchedInstanceLeft = nullptr;
                    params.stretchedInstanceRight = &instance;
                    params.draggedInstance = nullptr;

                    if(!params.selected.contains(&instance))
                    {
                        if(!ImGui::IsKeyDown(ImGuiKey_LeftCtrl))
                            params.selected.clear();
                        params.selected.insert({&instance,{instance.frameStart,instance.frameEnd}});
                    }
                }
            }
            //Can drag
            else if(params.selected.contains(&instance) || mouseOverlapsInstance)
            {
                drawlist->AddTriangle(ImVec2(start.x + 2, start.y + 20.0), ImVec2(start.x + 12.0, start.y + 14.0), ImVec2(start.x + 12.0, start.y + 26.0), 0xFF000000);
                drawlist->AddTriangle(ImVec2(end.x - 2, start.y + 20.0), ImVec2(end.x - 12.0, start.y + 14.0), ImVec2(end.x - 12.0, start.y + 26.0), 0xFF000000);
                if(ImGui::IsMouseClicked(ImGuiMouseButton_Left) && mouseOverlapsInstance)
                {
                    clickedButton = true;
                    params.stretchedInstanceLeft = nullptr;
                    params.stretchedInstanceRight = nullptr;
                    params.draggedInstance = &instance;
                    
                    if(!params.selected.contains(&instance))
                    {
                        if(!ImGui::IsKeyDown(ImGuiKey_LeftCtrl))
                            params.selected.clear();
                        params.selected.insert({&instance,{instance.frameStart,instance.frameEnd}});
                    }
                }
            }
            if(auto iter = params.selected.find(&instance); iter != params.selected.end())
            {
                if(mouseOverlapsInstance && ImGui::IsMouseClicked(ImGuiMouseButton_Right))
                    params.selected.erase(iter);
                else
                    drawlist->AddRect(start, end, 0xFFFF0000, 0.4, 3);
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
    if(params.selected.size() == 1){
        ImGui::SetNextWindowSizeConstraints(ImVec2(300,400), ImVec2(300,400));
        ImGui::SetNextWindowPos(ImVec2(windowPos.x + windowSize.x, windowPos.y + windowSize.y), ImGuiCond_Always, ImVec2(0.95,0.95));
        bool open = true;
        ImGui::PushStyleColor(ImGuiCol_Text, tracker::SCREEN_FG);
        ImGui::PushStyleColor(ImGuiCol_WindowBg, tracker::CREAM);
        ImGui::Begin("Selected Instance", &open, ImGuiWindowFlags_AlwaysAutoResize 
        | ImGuiWindowFlags_NoSavedSettings 
        | ImGuiWindowFlags_NoFocusOnAppearing 
        | ImGuiWindowFlags_NoNav 
        | ImGuiWindowFlags_NoMove);
        
        const auto windowPos = ImGui::GetCursorScreenPos();
        const auto windowSize = ImGui::GetContentRegionAvail();
        
        auto& instance = params.selected.begin()->first;
        ImGui::SetNextItemWidth(150.0);
        ImGui::SliderInt("Start Frame", &instance->frameStart, 0, instance->frameEnd - 1);
        ImGui::SetNextItemWidth(150.0);
        ImGui::SliderInt("End Frame", &instance->frameEnd, instance->frameStart + 1, lastFrame);
        ImGui::SetNextItemWidth(150.0);
        ImGui::SliderInt("Prog:",&instance->soundNumber, 0, NUM_CHANNELS);

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
    ImGui::PopStyleColor();
    ImGui::EndChild();
}