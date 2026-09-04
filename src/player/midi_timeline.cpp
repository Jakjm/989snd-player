#include "midi_timeline.h"
#include <string>
#include "tracker_style.h"
#include "third-party/imgui/imgui.h"

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
    
    //printf("(%f,%f) (%f,%f) (%f,%f) Cursor (%f,%f)\n", windowPos.x, windowPos.y, windowSize.x, windowSize.y, mousePos.x, mousePos.y);
    //If mouse is on widget 
    if(windowPos.x <= mousePos.x && mousePos.x <= windowPos.x + windowSize.x && 
        windowPos.y <= mousePos.y && mousePos.y <= windowPos.y + 35)
    {
        //Allow the timeline to be dragged left/right
        if (ImGui::IsMouseDragging(ImGuiMouseButton_Left))
        {
            ImVec2 dragDelta = ImGui::GetMouseDragDelta(ImGuiMouseButton_Left);
            if(dragDelta.x < 0.0){
                params.startFrame += (int)(1.0 / params.timelineZoom);
            }
            else if(dragDelta.x > 0.0 && params.startFrame >= 0){
                params.startFrame -= (int)(1.0 / params.timelineZoom);
                if(params.startFrame < 0)
                    params.startFrame = 0;
            }
        }
    
        //Allow zooming in/out
        if(io.MouseWheel > 0.0 && params.timelineZoom < 16.0){
            params.timelineZoom *= 1.05;
        }
        else if(io.MouseWheel < 0.0 && params.timelineZoom > 0.0625){
            params.timelineZoom *= 0.95;
        }
    }



    //Draw the timeline
    auto currentPos = ImGui::GetWindowPos();
    int curFrame = params.startFrame;
    currentPos = ImVec2(currentPos.x + 5, currentPos.y + 2);
    while(currentPos.x < windowPos.x + windowSize.x - 5){
        double lineHeight;
        if(curFrame % 10 == 0){
            lineHeight = 15;

            std::string frameNumberText = std::to_string(curFrame);
            double halfTextWidth = ImGui::CalcTextSize(frameNumberText.c_str()).x / 2.0;
            drawlist->AddText(ImVec2(currentPos.x - halfTextWidth, currentPos.y + 16), 0xFF000000, frameNumberText.c_str());
        }
        else{
            lineHeight = 10;
        }
        drawlist->AddLine(currentPos,ImVec2(currentPos.x, currentPos.y + lineHeight), 0xFF000000, 1.0);
        currentPos.x += 10 * params.timelineZoom;
        ++curFrame;
    }

    
    ImGui::PopStyleColor();
    ImGui::EndChild();
}