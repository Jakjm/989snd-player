#include "midi_timeline.h"

#include <cmath>
#include <iterator>
#include <string>
#include <variant>

#include "tracker_style.h"

#include "common/util/BinaryReader.h"

#include "sound/989snd/musicbank.h"

#include "fmt/format.h"
#include "third-party/imgui/imgui.h"
#include "third-party/imgui/imgui_internal.h"

const double ZOOM_MAX = 16;
const double ZOOM_MIN = 1.0 / 8000.0;
const int NUM_CHANNELS = 16;
const double TIMELINE_BOX_HEIGHT = 35.0;

void readBank(snd::MusicBank* bank, MidiTimelineParams& params) {
  params.notes.clear();
  params.selected.clear();
  params.tabSelected = -1;
  params.beganClickingTimeline = false;
  params.draggedInstance = -1;
  params.stretchedInstanceLeft = -1;
  params.stretchedInstanceRight = -1;
  params.startTick = 0;
  params.timelineZoom = 1.0;

  for (int i = 0; i < 16; ++i) {
    params.registers[i] = 0;
    params.macros[i] = nullptr;
  }
  // params.registers[0] = 1;

  // TODO: read multi midi, add a tab for each one...
  if (std::holds_alternative<snd::Midi>(bank->MidiData))
    readMidiData(std::get<snd::Midi>(bank->MidiData), params);
  else if (std::holds_alternative<snd::MultiMidi>(bank->MidiData)) {
    auto multiMidi = std::get<snd::MultiMidi>(bank->MidiData);
    int midi_ct = 0;
    for (auto& midi : multiMidi.midi) {
      printf("Midi: %d\n", midi_ct);
      readMidiData(midi, params);
      ++midi_ct;
    }
  }
}

std::pair<bool, u8*> RunAME(UnboundedBinaryReader stream, MidiTimelineParams& params) {
  int skip = 0;
  bool done = false;
  bool cont = true;

  while (!done) {
    auto op = stream.read<u8>();
    //printf("0x%X\n", (int)op);
    if (op == 0x4 && skip == 1) {
      skip = 2;
    } else if (op == 0x5 && skip == 2) {
      skip = 0;
    } else if (op == 0xB) {
      // fmt::print("ame trace b\n");
      params.macros[stream.peek<u8>()] = stream.getStart() + stream.get_seek() + 1;
      while (stream.read<u8>() != 0xf7)
        ;
    } else if (op == 0xF) {
      // fmt::print("ame trace f\n");
      if (skip) {
        while (stream.read<u8>() != 0x7f)
          ;
        if (skip == 1)
          skip = 0;
      } else {
        auto group = stream.read<u8>();
        // m_groups[group].basis =
        stream.read<u8>();
        u8 channel = 0;
        while (stream.peek<u8>() != 0xf7) {
          // m_groups[group].channel[channel] =
          stream.read<u8>();
          // m_groups[group].excite_min[channel] =
          stream.read<u8>();
          // m_groups[group].excite_max[channel] =
          stream.read<u8>();
          channel++;
        }
        // m_groups[group].num_channels = channel;
        stream.ffwd(1);  // Skip past f7
      }
    } else {
      if (skip == 1)
        skip = -1;

      if (op == 0x0 && snd::GlobalExcite <= (stream.read<u8>() + 1) && skip == 0)
        skip = 1;
      else if (op == 0x1 && snd::GlobalExcite != (stream.read<u8>() + 1) && skip == 0)
        skip = 1;
      else if (op == 0x2 && snd::GlobalExcite > (stream.read<u8>() + 1) && skip == 0)
        skip = 1;
      else if (op == 0x3) {
        u8 midiNum = stream.read<u8>();
        // TODO: if(skip == 1), stop midiNum
      } else if (op == 0x6 && params.registers[stream.read<u8>()] > (stream.read<u8>() - 1) &&
                 skip == 0)
        skip = 1;
      else if (op == 0x7 && params.registers[stream.read<u8>()] < (stream.read<u8>() + 1) &&
               skip == 0)
        skip = 1;
      else if (op == 0xC) {
        int index = stream.read<u8>();
        u8* dataIndex = params.macros[index];
        if (skip == 0 && !RunAME(UnboundedBinaryReader(params.macros[index]), params).first) {
          cont = false;
          done = true;
        }
      } else if (op == 0xD && skip == 0) {
        auto reg = stream.read<u8>();
        if (skip == 0) {
          cont = false;
          done = true;
          // TODO: StartSegment(registers[reg] - 1);
        }
      } else if (op == 0xE) {
        auto reg = stream.read<u8>();
        if (skip == 0)
          ;
        // TODO: StartSegment(registers[reg] - 1);
      } else if (op == 0x10) {
        stream.read<u8>();
        // u8 group = stream[0];
        // u8 comp = 0;
        // if (m_groups[group].basis == 0) {
        //     comp = GlobalExcite;
        // } else {
        //     comp = m_register[m_groups[group].basis - 1];
        // }
        // // fmt::print("group: {} basis: {} excite: {}\n", group, m_groups[group].basis, comp);
        // for (int i = 0; i < m_groups[group].num_channels; i++) {
        //     // auto xmin = m_groups[group].excite_min[i];
        //     // auto xmax = m_groups[group].excite_max[i];
        //     // fmt::print("chan {} excite: {}-{}\n", i, xmin, xmax);

        //     // note : added hack here! :-)
        //     if (!SoundFlavaHack &&
        //         (comp < m_groups[group].excite_min[i] || comp > m_groups[group].excite_max[i])) {
        //         midi.MuteChannel(m_groups[group].channel[i]);
        //     } else {
        //         midi.UnmuteChannel(m_groups[group].channel[i]);
        //     }
        // }
      } else if (op == 0x11) {
        auto midi = stream.read<u8>();
        if (skip == 0) {
          done = true;
          cont = false;
          // TODO start midi
        }
      } else if (op == 0x12) {
        auto midi = stream.read<u8>();
        if (skip == 0)
          ;
        // TODO start midi
      } else if (op == 0x13) {
        u8 reg = stream.read<u8>();
        u8 val = stream.read<u8>();
        if (skip == 0)
          params.registers[reg] = val;
      } else if (op == 0x14) {
        u8 reg = stream.read<u8>();
        if (skip == 0 && params.registers[reg] < 0x7f)
          params.registers[reg]++;
      } else if (op == 0x15) {
        u8 reg = stream.read<u8>();
        if (skip == 0 && params.registers[reg] > 0)
          params.registers[reg]--;
      } else if (op == 0x16) {
        u8 reg = stream.read<u8>();
        u8 val = stream.read<u8>();
        if (skip == 0 && params.registers[reg] != val)
          skip = 1;
      } else if (op > 0x16) {
        // throw AMEError(fmt::format("Unhandled AME event {:02x}", (u8)op));
        break;
      }

      if (skip < 0)
        skip = 0;
    };

    if (stream.peek<u8>() == 0xf7) {
      stream.ffwd(1);
      done = true;
    }
  }

  return {cont, stream.getStart() + stream.get_seek()};
}

void readMidiData(snd::Midi& midi, MidiTimelineParams& params) {
  auto& notes = params.notes.emplace_back();
  u8* dataStart = midi.DataStart;
  UnboundedBinaryReader reader(dataStart);
  u64 time = 0;
  int cont_ct = 2;

  std::array<std::multimap<int, int>, NUM_CHANNELS> channel_notes;
  std::array<int, NUM_CHANNELS> channel_programs;
  for (int i = 0; i < NUM_CHANNELS; ++i)
    channel_programs[i] = -1;

  params.tempo = midi.Tempo;  // micros per quarter note
  params.PPQ = midi.PPQ;
  // u64 ppt = 100 * mics_per_tick / (params.tempo / midi.PPQ);
  // u64 tickDelta = 0, tickError = 0, tickCountdown;
  u8 status_byte;
  do {
    auto [len, delta] = snd::MidiHandler::ReadVLQ(dataStart + reader.get_seek());
    reader.ffwd(len);
    time += delta;

    // No need to handle time stuff here lol
    //  tickDelta = 100 * delta + tickError;
    //  if (tickDelta < 0 || tickDelta < ppt / 2) {
    //      tickError = tickDelta;
    //      tickDelta = 0;
    //  }
    //  if (tickCountdown != 0) {
    //      tickCountdown = (tickDelta / 100 * midi.Tempo / midi.PPQ - 1 + mics_per_tick) /
    //      mics_per_tick; tickError = tickDelta - ppt * tickCountdown;
    //  }

    if (reader.peek<u8>() & 0x80) {
      status_byte = reader.read<u8>();
    }
    //printf("0x%X\n", (int)(status_byte >> 4));

    // assert(status_byte & 0x80);
    switch (status_byte >> 4) {
      case 0x9: {
        // NoteON
        // If velocity is zero, fall through to note-off case
        if (reader.peek<u8>(1) != 0) {
          u8 channel = status_byte & 0xF;
          u8 note = reader.read<u8>();
          u8 velocity = reader.read<u8>();
          u8 program = channel_programs[channel];
          printf("Note start at %ld %d %d %d %d\n",time, channel, note, velocity, program);

          notes.emplace_back(time, -1, program, note, channel);
          channel_notes[channel].insert({note, notes.size() - 1});
          break;
        }
      }
      case 0x8: {
        // NoteOFF
        // assert(channel_notes[channel] != nullptr);
        u8 channel = status_byte & 0xF;
        u8 note = reader.read<u8>();
        u8 velocity = reader.read<u8>();
        //printf("Note end at %ld %d %d\n",time, channel, note);
        //printf("Note end at %ld %d %d %d\n",time, channel, note, velocity);
        if(const auto it = channel_notes[channel].find(note); it != channel_notes[channel].end())
        {
            auto& noteInstance = notes[it->second];
            noteInstance.tickEnd = time;
            channel_notes[channel].erase(it);
        }
        break;
      }
      case 0xB:
        reader.ffwd(2);
        break;
        // ControllerChange();
        // break;
      case 0xD: {
        u8 channel = status_byte & 0xF;
        u8 note = reader.read<u8>();
        //printf("Note end at %ld %d %d\n",time, channel, note);
        if(const auto it = channel_notes[channel].find(note); it != channel_notes[channel].end())
        {
            auto& noteInstance = notes[it->second];
            noteInstance.tickEnd = time;
            channel_notes[channel].erase(it);
        }
        break;
        // ChannelPressure();
        // break;
      }
      case 0xC: {
        u8 channel = status_byte & 0xf;
        u8 program = reader.read<u8>();

        channel_programs[channel] = program;
        break;
      }
      case 0xE:
        reader.ffwd(2);
        break;
        // ChannelPitch();
        // break;
      case 0xF:
        // normal meta-event
        if (status_byte == 0xFF) {  // META Event means it's time to repeat or something
          // MetaEvent();
          // break;
          u8 eventType = reader.read<u8>();
          size_t len = reader.read<u8>();
          if (eventType == 0x2f) {
            reader.set_seek(0);
            --cont_ct;
            if (cont_ct <= 0)
              return;
            break;  // Break out before looping
          } else if (eventType == 0x51) {
            params.tempo =
                (reader.peek<u8>() << 16) | (reader.peek<u8>(1) << 8) | reader.peek<u8>(2);
            // ppt = 100 * mics_per_tick / (params.tempo / midi.PPQ);
          }
          reader.ffwd(len);
          break;
        }
        if (status_byte == 0xF0) {  // SYSTEM EVENT means AME handler trying to do something

          if (reader.read<u8>() == 0x75) {
            auto [cont, ptr] = RunAME(reader, params);
            reader.set_seek(ptr - dataStart);
          }
          // TODO: stopped by AME.
          else
            return;  // Unknown system event
          // break;
          break;
        }
        [[fallthrough]];
      default: {
        return;
        // throw MidiError(fmt::format("invalid status {}", status_byte));
        // return;
      }
    }
  } while (1);
  return;
}

void drawProgs(MidiTimelineParams& params) {}

bool drawSelectedProperties(MidiTimelineParams& params,
                            ImVec2 windowPos,
                            ImVec2 windowSize,
                            std::vector<NoteInstance>& notes,
                            ImVec2 mousePos,
                            int lastTick) {
  bool clickedProperties = false;
  if (!params.selected.empty()) {
    ImGui::SetNextWindowSizeConstraints(ImVec2(300, 400), ImVec2(300, 400));
    ImGui::SetNextWindowPos(ImVec2(windowPos.x + windowSize.x, windowPos.y + windowSize.y),
                            ImGuiCond_Always, ImVec2(0.95, 0.95));
    bool open = true;
    ImGui::PushStyleColor(ImGuiCol_Text, tracker::SCREEN_FG);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, tracker::CREAM);
    ImGui::Begin(params.selected.size() > 1 ? "SelectedInstances" : "Selected Instance", &open,
                 ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings |
                     ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav |
                     ImGuiWindowFlags_NoMove);

    const auto windowPos = ImGui::GetCursorScreenPos();
    const auto windowSize = ImGui::GetContentRegionAvail();

    if (ImGui::BeginTabBar("Note Selections")) {
      int index = 0;
      for (auto iter : params.selected) {
        auto& instance = notes[iter.first];
        std::string selectionNumberText = "(" + std::to_string(index) + ")";
        auto textSize = ImGui::CalcTextSize(selectionNumberText.c_str());
        if (ImGui::BeginTabItem(selectionNumberText.c_str())) {
          ImGui::SetNextItemWidth(150.0);
          ImGui::SliderInt("Start Tick", &instance.tickStart, 0, instance.tickEnd - 1);
          ImGui::SetNextItemWidth(150.0);
          ImGui::SliderInt("End Tick", &instance.tickEnd, instance.tickStart + 1, lastTick);
          ImGui::SetNextItemWidth(150.0);
          ImGui::SliderInt("Program", &instance.program, 0, NUM_CHANNELS);
          ImGui::SetNextItemWidth(150.0);
          ImGui::SliderInt("Note", &instance.note, 0, NUM_CHANNELS);
          ImGui::EndTabItem();
        }
        ++index;
      }
      ImGui::EndTabBar();
    }

    // printf("mousePos (%f %f) windowPos (%f %f) windowSize (%f %f) \n", mousePos.x, mousePos.y,
    // windowPos.x, windowPos.y, windowSize.x, windowSize.y);
    if (windowPos.x <= mousePos.x && mousePos.x <= windowPos.x + windowSize.x &&
        windowPos.y <= mousePos.y && mousePos.y <= windowPos.y + windowSize.y &&
        ImGui::IsMouseClicked(ImGuiMouseButton_Left))
      clickedProperties = true;

    ImGui::End();
    ImGui::PopStyleColor();
    ImGui::PopStyleColor();
  }
  return clickedProperties;
}

void dragInstances(MidiTimelineParams& params, std::vector<NoteInstance>& notes, double tickWidth) {
  if (ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
    ImVec2 dragDelta = ImGui::GetMouseDragDelta(ImGuiMouseButton_Left);
    if (params.beganClickingTimeline) {
      params.startTick = params.timelineStartBeforeClick - (int)ceil(dragDelta.x / tickWidth);
      if (params.startTick < 0)
        params.startTick = 0;
    } else if (params.stretchedInstanceLeft != -1) {
      auto& stretchLeft = notes[params.stretchedInstanceLeft];
      stretchLeft.tickStart =
          params.selected[params.stretchedInstanceLeft].first + (int)ceil(dragDelta.x / tickWidth);
      if (stretchLeft.tickStart < 0)
        stretchLeft.tickStart = 0;
      else if (stretchLeft.tickStart >= stretchLeft.tickEnd - 1)
        stretchLeft.tickStart = stretchLeft.tickEnd - 1;
    } else if (params.stretchedInstanceRight != -1) {
      auto& stretchRight = notes[params.stretchedInstanceRight];
      stretchRight.tickEnd = params.selected[params.stretchedInstanceRight].second +
                             (int)ceil(dragDelta.x / tickWidth);
      if (stretchRight.tickEnd <= stretchRight.tickStart + 1)
        stretchRight.tickEnd = stretchRight.tickStart + 1;
    } else if (params.draggedInstance != -1) {
      for (auto selectedIter : params.selected) {
        auto& selected = notes[selectedIter.first];
        int tickStartAtClick = selectedIter.second.first;
        int tickEndAtClick = selectedIter.second.second;
        selected.tickStart = tickStartAtClick + (int)ceil(dragDelta.x / tickWidth);
        selected.tickEnd = tickEndAtClick + (int)ceil(dragDelta.x / tickWidth);
        if (selected.tickStart < 0) {
          selected.tickStart = 0;
          selected.tickEnd = (tickEndAtClick - tickStartAtClick);
        }
      }
    }
  }
}

bool handleInstanceSelection(MidiTimelineParams& params,
                             int index,
                             ImVec2 start,
                             ImVec2 end,
                             ImVec2 mousePos,
                             NoteInstance& instance) {
  bool clickedButton = false;
  bool mouseOverlapsInstanceY = start.y <= mousePos.y && mousePos.y <= end.y;
  bool mouseOverlapsInstanceX = start.x <= mousePos.x && mousePos.x <= end.x;
  bool mouseOverlapsInstance = mouseOverlapsInstanceX && mouseOverlapsInstanceY;
  bool mouseOverlapsInstanceStart =
      (start.x <= mousePos.x) && (mousePos.x <= start.x + 20.0) && mouseOverlapsInstanceY;
  bool mouseOverlapsInstanceEnd =
      (end.x - 20.0 <= mousePos.x) && (mousePos.x <= end.x) && mouseOverlapsInstanceY;

  // Check if potentially able to stretch or drag instance
  // Can stretch left
  if (params.stretchedInstanceLeft == index || mouseOverlapsInstanceStart) {
    ImGui::GetWindowDrawList()->AddTriangleFilled(
        ImVec2(start.x + 2, start.y + 20.0), ImVec2(start.x + 12.0, start.y + 14.0),
        ImVec2(start.x + 12.0, start.y + 26.0), 0xFF000000);
    if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) && mouseOverlapsInstanceStart) {
      clickedButton = true;
      ImGui::GetWindowDrawList()->AddTriangleFilled(
          ImVec2(start.x + 2, start.y + 20.0), ImVec2(start.x + 12.0, start.y + 14.0),
          ImVec2(start.x + 12.0, start.y + 26.0), 0xFF000000);
      params.stretchedInstanceLeft = index;
      params.stretchedInstanceRight = -1;
      params.draggedInstance = -1;

      if (!params.selected.contains(index)) {
        if (!ImGui::IsKeyDown(ImGuiKey_LeftCtrl))
          params.selected.clear();
        params.selected.insert({index, {instance.tickStart, instance.tickEnd}});
      }
    }
  }
  // Can stretch right
  else if (params.stretchedInstanceRight == index || mouseOverlapsInstanceEnd) {
    ImGui::GetWindowDrawList()->AddTriangleFilled(ImVec2(end.x - 2, start.y + 20.0),
                                                  ImVec2(end.x - 12.0, start.y + 14.0),
                                                  ImVec2(end.x - 12.0, start.y + 26.0), 0xFF000000);
    if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) && mouseOverlapsInstanceEnd) {
      clickedButton = true;
      params.stretchedInstanceLeft = -1;
      params.stretchedInstanceRight = index;
      params.draggedInstance = -1;

      if (!params.selected.contains(index)) {
        if (!ImGui::IsKeyDown(ImGuiKey_LeftCtrl))
          params.selected.clear();
        params.selected.insert({index, {instance.tickStart, instance.tickEnd}});
      }
    }
  }
  // Can drag
  else if (params.selected.contains(index) || mouseOverlapsInstance) {
    ImGui::GetWindowDrawList()->AddTriangle(ImVec2(start.x + 2, start.y + 20.0),
                                            ImVec2(start.x + 12.0, start.y + 14.0),
                                            ImVec2(start.x + 12.0, start.y + 26.0), 0xFF000000);
    ImGui::GetWindowDrawList()->AddTriangle(ImVec2(end.x - 2, start.y + 20.0),
                                            ImVec2(end.x - 12.0, start.y + 14.0),
                                            ImVec2(end.x - 12.0, start.y + 26.0), 0xFF000000);
    if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) && mouseOverlapsInstance) {
      clickedButton = true;
      params.stretchedInstanceLeft = -1;
      params.stretchedInstanceRight = -1;
      params.draggedInstance = index;

      if (!params.selected.contains(index)) {
        if (!ImGui::IsKeyDown(ImGuiKey_LeftCtrl))
          params.selected.clear();
        params.selected.insert({index, {instance.tickStart, instance.tickEnd}});
      }
    }
  }

  if (auto iter = params.selected.find(index); iter != params.selected.end()) {
    if (mouseOverlapsInstance && ImGui::IsMouseClicked(ImGuiMouseButton_Right))
      params.selected.erase(iter);
    else {
      ImGui::GetWindowDrawList()->AddRect(start, end, 0xFFFF0000, 0.4, 3);
      auto index = std::distance(params.selected.begin(), iter);
      std::string selectionNumberText = "(" + std::to_string(index) + ")";
      auto textSize = ImGui::CalcTextSize(selectionNumberText.c_str());
      ImGui::GetWindowDrawList()->AddText(
          ImVec2(start.x + (end.x - start.x - textSize.x) / 2.0, start.y + 20.0 - textSize.y / 2.0),
          0xFF000000, selectionNumberText.c_str());
    }
  }
  return clickedButton;
}

void drawMidiTimeline(MidiTimelineParams& params) {
  const auto topline =
      fmt::format("Tempo (micros per quarter note): {} PPQ: {}", params.tempo, params.PPQ);
  ImGui::Text(topline.c_str());

  const auto drawlist = ImGui::GetWindowDrawList();
  const auto io = ImGui::GetIO();

  const int ticksPerMeasure = 4 * params.PPQ;
  const double quarterNoteWidth = 10.0 * params.timelineZoom;
  const double measureWidth = 4.0 * quarterNoteWidth;
  const double tickWidth = measureWidth / ticksPerMeasure;

  // Allow the timeline to be dragged left/right
  // Allow zooming in/out
  if (io.MouseWheel > 0.0 && params.timelineZoom < ZOOM_MAX) {
    params.timelineZoom *= 1.05;
  } else if (io.MouseWheel < 0.0 && params.timelineZoom > ZOOM_MIN) {
    params.timelineZoom *= 0.95;
  }

  bool clickedButton = false;
  // Add a tab for each midi
  if (ImGui::BeginTabBar("MIDIS")) {
    for (auto it = params.notes.begin(); it != params.notes.end(); ++it) {
      auto midi_index = std::distance(params.notes.begin(), it);
      auto& notes = *it;
      if (ImGui::BeginTabItem(("Midi " + std::to_string(midi_index)).c_str())) {
        if (params.tabSelected != midi_index) {
          params.selected.clear();
          params.tabSelected = midi_index;
        }
        const auto windowPos = ImGui::GetCursorScreenPos();
        const auto windowSize = ImGui::GetContentRegionAvail();
        const auto mousePos = io.MousePos;
        

        dragInstances(params, notes, tickWidth);

        if (ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
          params.beganClickingTimeline = false;
          params.stretchedInstanceRight = -1;
          params.stretchedInstanceLeft = -1;
          params.draggedInstance = -1;

          // Update initial start/end of selected nodes
          for (auto& iter : params.selected) {
            auto note = notes[iter.first];
            iter.second.first = note.tickStart;
            iter.second.second = note.tickEnd;
          }
        } else if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
          if (windowPos.x <= mousePos.x && mousePos.x <= windowPos.x + windowSize.x &&
              windowPos.y <= mousePos.y && mousePos.y <= windowPos.y + TIMELINE_BOX_HEIGHT) {
            params.beganClickingTimeline = true;
            params.stretchedInstanceRight = -1;
            params.stretchedInstanceLeft = -1;
            params.draggedInstance = -1;
            params.timelineStartBeforeClick = params.startTick;
          } else {
            params.beganClickingTimeline = false;
          }
        }
        
        int firstTick = params.startTick;
        int curTick = params.startTick;
        
        double spaceRemaining = windowSize.x - 7.0;
        int lastTick = curTick + (int)ceil(spaceRemaining / tickWidth);
        int lastMeasure = lastTick / ticksPerMeasure;

        for (auto iter = notes.begin(); iter != notes.end(); ++iter) {
          int index = std::distance(notes.begin(), iter);
          auto& instance = *iter;

          if (params.startTick <= instance.tickEnd && instance.tickStart <= lastTick) {
            double startX = windowPos.x + 5;
            if (instance.tickStart >= params.startTick)
              startX += (double)(tickWidth * (instance.tickStart - firstTick));

            double endX = windowPos.x + windowSize.x - 2;
            if (instance.tickEnd <= lastTick)
              endX = windowPos.x + 5 + tickWidth * (double)(instance.tickEnd - firstTick);

            auto start = ImVec2(startX, windowPos.y + TIMELINE_BOX_HEIGHT + 40 * instance.channel);
            auto end = ImVec2(endX, start.y + 40.0);

            // Draw a rectangle for the instance
            drawlist->AddRectFilled(start, end, 0xFF0000FF, 0.4);
            drawlist->AddRect(start, end, 0xFF000000, 0.4, 1.5);

            if (handleInstanceSelection(params, index, start, end, mousePos, instance))
              clickedButton = true;
          }
        }

        // Draw rectangles around each channel row
        auto currentPos = ImVec2(windowPos.x + 5, windowPos.y + TIMELINE_BOX_HEIGHT);

        for (int channel = 0; channel < NUM_CHANNELS; ++channel) {
          drawlist->AddRect(currentPos, ImVec2(windowPos.x + windowSize.x - 2, currentPos.y + 41),
                            0xFF000000, 0.2);

          const auto channelText = std::string("Channel ") + std::to_string(channel);
          const auto textSize = ImGui::CalcTextSize(channelText.c_str());

          drawlist->AddText(ImVec2(windowPos.x + 5 + (windowSize.x - textSize.x) / 2.0,
                                   currentPos.y + 20 - textSize.y / 2.0),
                            0x77000000, channelText.c_str());

          currentPos.y += 40;
        }

        // Timeline drawing code:
        currentPos = ImVec2(windowPos.x + 5, windowPos.y + 16);
        drawlist->AddRectFilled(
            windowPos, ImVec2(windowPos.x + windowSize.x, windowPos.y + TIMELINE_BOX_HEIGHT),
            0xFFA0A0A0);
        // Draw horizontal line to right side
        drawlist->AddLine(currentPos, ImVec2(windowPos.x + windowSize.x - 2, currentPos.y),
                          0xFF000000);
        // Account for being between quarter notes:
        double distanceToFirstQuarter = 0.0;
        if (int remainder = firstTick % params.PPQ; remainder != 0) {
          // Round up to next quarter note
          curTick += (params.PPQ - remainder);
          distanceToFirstQuarter = (params.PPQ - remainder) * tickWidth;
          currentPos.x += distanceToFirstQuarter;
        }

        // If curTickTenth is divisible by this, draw the currentTick line under the line.
        const auto numberSize = ImGui::CalcTextSize(std::to_string(lastMeasure).c_str());
        int measureNumberVisibleMultiple =
            ticksPerMeasure * (int)ceil(2 * numberSize.x / measureWidth);
        // printf("%d\n", measureNumberVisibleMultiple);
        while (currentPos.x < windowPos.x + windowSize.x - 2) {
          double lineHeight;
          if (curTick % measureNumberVisibleMultiple == 0) {
            lineHeight = 15;

            int curMeasure = curTick / ticksPerMeasure;
            std::string measureNumberText = std::to_string(curMeasure);
            double halfTextWidth = ImGui::CalcTextSize(measureNumberText.c_str()).x / 2.0;
            drawlist->AddText(ImVec2(currentPos.x - halfTextWidth, windowPos.y + 2), 0xFF000000,
                              measureNumberText.c_str());
          } else {
            lineHeight = 10;
          }
          drawlist->AddLine(currentPos, ImVec2(currentPos.x, currentPos.y + lineHeight), 0xFF000000,
                            1.0);
          drawlist->AddLine(currentPos, ImVec2(currentPos.x, windowPos.y + TIMELINE_BOX_HEIGHT + 40 * NUM_CHANNELS), 0x77000000, 1.0);
          // Advance by one eigth note
          currentPos.x += quarterNoteWidth;
          curTick += params.PPQ;
        }

        // Create a window for customizing currently selected instance.
        bool clickedProperties =
            drawSelectedProperties(params, windowPos, windowSize, notes, mousePos, lastTick);
        if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) && (params.stretchedInstanceLeft == -1) &&
            (params.stretchedInstanceRight == -1) && (params.draggedInstance == -1) &&
            !params.beganClickingTimeline && !clickedButton && !clickedProperties) {
          params.selected.clear();
        }

        // Draw faint vertical lines indicating quarter notes 
        

        // Draw a vertical line indicating current tick.
        if (firstTick <= params.playback_tick && params.playback_tick <= lastTick) {
          currentPos = ImVec2(windowPos.x + 5.0 + (params.playback_tick - firstTick) * tickWidth,
                              windowPos.y + 16.0);
          drawlist->AddLine(
              currentPos,
              ImVec2(currentPos.x, windowPos.y + TIMELINE_BOX_HEIGHT + 40 * NUM_CHANNELS),
              0xFF00FF00, 3.0);
        }
        ImGui::EndTabItem();
      }
    }
    ImGui::EndTabBar();
  }
}

void MidiTimeline(MidiTimelineParams& params) {
  ImGui::PushStyleColor(ImGuiCol_ChildBg, tracker::CREAM);
  ImGui::BeginChild("miditimeline");

  if (ImGui::BeginTabBar("MidiTimelineBar")) {
    if (ImGui::BeginTabItem("Timeline")) {
      drawMidiTimeline(params);
      ImGui::EndTabItem();
    }
    if (ImGui::BeginTabItem("Progs")) {
      drawProgs(params);
      ImGui::EndTabItem();
    }
    ImGui::EndTabBar();
  }
  ImGui::PopStyleColor();
  ImGui::EndChild();
}