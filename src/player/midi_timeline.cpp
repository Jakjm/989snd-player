#include "midi_timeline.h"
#include <cmath>
#include <string>
#include "tracker_style.h"
#include "third-party/imgui/imgui.h"

const double ZOOM_MAX = 8;

void MidiTimeline(MidiTimelineParams &params){
    ImGui::BeginChild("miditimeline");
    ImGui::PushStyleColor(ImGuiCol_WindowBg, tracker::SCREEN_BG);
    const auto windowPos = ImGui::GetCursorScreenPos();
    const auto windowSize = ImGui::GetContentRegionAvail();
    const auto drawlist = ImGui::GetWindowDrawList();
    const auto io = ImGui::GetIO();

    const auto mousePos = io.MousePos;

    drawlist->AddRectFilled(windowPos, ImVec2(windowPos.x + windowSize.x,windowPos.y + windowSize.y), 0xFFFFFFFF);
    drawlist->AddRectFilled(windowPos, ImVec2(windowPos.x + windowSize.x,windowPos.y + 35), 0xFFA0A0A0);


    if (ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
        params.beganClickingTimeline = false;
    }
    else if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        if(windowPos.x <= mousePos.x && mousePos.x <= windowPos.x + windowSize.x && 
        windowPos.y <= mousePos.y && mousePos.y <= windowPos.y + 35) {
            params.beganClickingTimeline = true;
        }
        else{
            params.beganClickingTimeline = false;
        }
    }
    
    
    if (params.beganClickingTimeline && ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
        if (ImGui::IsMouseDragging(ImGuiMouseButton_Left))
        {
            ImVec2 mouseDelta = ImGui::GetIO().MouseDelta;
            //printf("Drag Delta (%f,%f)  (int)(1.0 / params.timelineZoom) %d ", dragDelta.x, dragDelta.y, 
            // (int)(1.0 / params.timelineZoom));
            params.startFrameTenth -= (int)ceil((mouseDelta.x / params.timelineZoom));
            if(params.startFrameTenth < 0)
                params.startFrameTenth = 0;
        }
        //else{
            //printf("Not dragging ");
        //}
    }
    //printf("Released: %d Mouse Down: %d Clicked: %d Began Clicking Timeline%d\n", ImGui::IsMouseReleased(ImGuiMouseButton_Left), ImGui::IsMouseDown(ImGuiMouseButton_Left), ImGui::IsMouseClicked(ImGuiMouseButton_Left), params.beganClickingTimeline);
    //printf("WindowSize:(%f,%f) WindowSize:(%f,%f) Mouse Pos:(%f,%f)\n", windowPos.x, windowPos.y, windowSize.x, windowSize.y, mousePos.x, mousePos.y);
    //If mouse is on widget 
    //Allow the timeline to be dragged left/right
    //Allow zooming in/out
    if(io.MouseWheel > 0.0 && params.timelineZoom < 8.0){
        params.timelineZoom *= 1.05;
    }
    else if(io.MouseWheel < 0.0 && params.timelineZoom > 0.125){
        params.timelineZoom *= 0.95;
    }

    //Draw the timeline
    int curFrameTenth = params.startFrameTenth;
    
    //Add a bit of margin
    auto currentPos = ImVec2(windowPos.x + 5, windowPos.y + 16);
    //Draw horizontal line to right side
    drawlist->AddLine(currentPos, ImVec2(windowPos.x + windowSize.x - 2, currentPos.y), 0xFF000000);

    //Account for being between two frames
    int tenthsUntilNext = 10 - curFrameTenth % 10;
    if(tenthsUntilNext != 10)
    {
        currentPos.x += (double)tenthsUntilNext * params.timelineZoom;
        curFrameTenth += tenthsUntilNext;
    }

    //If curFrameTenth is divisible by this, draw the currentFrame line under the line.
    int visibleMultiple = 50 * (int)ceil(1.0 / params.timelineZoom);
    while(currentPos.x < windowPos.x + windowSize.x - 2){
        double lineHeight;
        if(curFrameTenth % visibleMultiple == 0){
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

    
    ImGui::PopStyleColor();
    ImGui::EndChild();
}