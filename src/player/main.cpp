#include <algorithm>
#include <cmath>
#include <cstdio>
#include <iterator>
#include <map>
#include <memory>
#include <string>

#include "sndplayer.h"
#include "tracker_style.h"

#if defined(__ANDROID__)
#include <GLES3/gl3.h>
#else
#include "glad/glad.h"
#endif

#include "third-party/SDL/include/SDL3/SDL.h"
#if defined(__ANDROID__)
#include "third-party/SDL/include/SDL3/SDL_main.h"
#endif
#include "third-party/imgui/imgui.h"
#include "third-party/imgui/imgui_impl_opengl3.h"
#include "third-party/imgui/imgui_impl_sdl3.h"
#include "third-party/imgui/imgui_internal.h"

namespace {

#if defined(__ANDROID__)
constexpr const char* GLSL_VERSION = "#version 300 es";
#elif defined(__APPLE__)
constexpr const char* GLSL_VERSION = "#version 410";
constexpr int GL_MAJOR = 4;
constexpr int GL_MINOR = 1;
#else
constexpr const char* GLSL_VERSION = "#version 430";
constexpr int GL_MAJOR = 4;
constexpr int GL_MINOR = 3;
#endif

void build_default_dock_layout(ImGuiID dockspace_id) {
  ImGui::DockBuilderRemoveNode(dockspace_id);
  ImGui::DockBuilderAddNode(dockspace_id, ImGuiDockNodeFlags_DockSpace);
  ImGui::DockBuilderSetNodeSize(dockspace_id, ImGui::GetMainViewport()->WorkSize);

  ImGuiID center = dockspace_id;
  const ImGuiID left = ImGui::DockBuilderSplitNode(center, ImGuiDir_Left, 0.24f, nullptr, &center);
  const ImGuiID right = ImGui::DockBuilderSplitNode(center, ImGuiDir_Right, 0.34f, nullptr, &center);
  ImGuiID left_top = left;
  const ImGuiID left_bottom =
      ImGui::DockBuilderSplitNode(left_top, ImGuiDir_Down, 0.62f, nullptr, &left_top);
  ImGuiID right_top = right;
  const ImGuiID right_bottom =
      ImGui::DockBuilderSplitNode(right_top, ImGuiDir_Down, 0.5f, nullptr, &right_top);

  ImGui::DockBuilderDockWindow("Banks", left_top);
  ImGui::DockBuilderDockWindow("Sounds", left_bottom);
  ImGui::DockBuilderDockWindow("Active sounds", center);
  ImGui::DockBuilderDockWindow("Playback params", right_top);
  ImGui::DockBuilderDockWindow("Player state", right_bottom);
  ImGui::DockBuilderFinish(dockspace_id);
}

#if defined(__ANDROID__)
SDL_AtomicInt g_in_background;
SDL_AtomicInt g_terminating;

bool SDLCALL lifecycle_watch(void* /*userdata*/, SDL_Event* event) {
  switch (event->type) {
    case SDL_EVENT_WILL_ENTER_BACKGROUND:
    case SDL_EVENT_DID_ENTER_BACKGROUND:
      SDL_SetAtomicInt(&g_in_background, 1);
      break;
    case SDL_EVENT_WILL_ENTER_FOREGROUND:
    case SDL_EVENT_DID_ENTER_FOREGROUND:
      SDL_SetAtomicInt(&g_in_background, 0);
      break;
    case SDL_EVENT_TERMINATING:
      SDL_SetAtomicInt(&g_terminating, 1);
      break;
    default:
      break;
  }
  return true;
}
#endif

}  // namespace

int main(int argc, char* argv[]) {
#if defined(__ANDROID__)
  SDL_SetHint(SDL_HINT_ANDROID_BLOCK_ON_PAUSE, "0");
#endif

  if (!SDL_Init(SDL_INIT_VIDEO)) {
    fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
    return 1;
  }

#if defined(__ANDROID__)
  SDL_AddEventWatch(lifecycle_watch, nullptr);
#endif

  SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
#if defined(__ANDROID__)
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
#else
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, GL_MAJOR);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, GL_MINOR);
#endif
  SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
  SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);

  SDL_Window* window = SDL_CreateWindow(
      "989snd Player", 1280, 800,
      SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY);
  if (!window) {
    fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
    SDL_Quit();
    return 1;
  }

  SDL_GLContext gl_context = SDL_GL_CreateContext(window);
  if (!gl_context) {
    fprintf(stderr, "SDL_GL_CreateContext failed: %s\n", SDL_GetError());
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 1;
  }
  SDL_GL_MakeCurrent(window, gl_context);
  SDL_GL_SetSwapInterval(1);

#if !defined(__ANDROID__)
  if (!gladLoadGLLoader((GLADloadproc)SDL_GL_GetProcAddress)) {
    fprintf(stderr, "Failed to load OpenGL via glad\n");
    SDL_GL_DestroyContext(gl_context);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 1;
  }
#endif

  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGui::StyleColorsDark();
  ImGui::GetIO().ConfigDragClickToInputText = true;
  ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_DockingEnable;
  tracker::apply_style();
  ImGui::GetIO().FontGlobalScale = kDefaultFontScale;

#if defined(__ANDROID__)
  static std::string ini_path;
  if (const char* internal = SDL_GetAndroidInternalStoragePath()) {
    ini_path = std::string(internal) + "/imgui.ini";
    ImGui::GetIO().IniFilename = ini_path.c_str();
  }
#endif
  ImGui_ImplSDL3_InitForOpenGL(window, gl_context);
  ImGui_ImplOpenGL3_Init(GLSL_VERSION);

  auto app = std::make_unique<SndPlayer>();

  for (int i = 1; i < argc; i++) {
    app->load_bank(argv[i]);
  }

#if defined(__ANDROID__)
  // Two-finger pinch-to-zoom tracking (SDL3 dropped the old multi-gesture
  // events, so we follow individual fingers ourselves).
  std::map<SDL_FingerID, SDL_FPoint> fingers;
  float prev_pinch_dist = 0.0f;
#endif

  bool running = true;
  while (running) {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
      ImGui_ImplSDL3_ProcessEvent(&event);
      if (event.type == SDL_EVENT_QUIT) {
        running = false;
      }
      if (event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED &&
          event.window.windowID == SDL_GetWindowID(window)) {
        running = false;
      }
#if !defined(__ANDROID__)
      if (event.type == SDL_EVENT_DROP_FILE && event.drop.data != nullptr) {
        app->load_bank(event.drop.data);
      }
#else
      switch (event.type) {
        case SDL_EVENT_FINGER_DOWN:
          fingers[event.tfinger.fingerID] = {event.tfinger.x, event.tfinger.y};
          prev_pinch_dist = 0.0f;
          break;
        case SDL_EVENT_FINGER_UP:
          fingers.erase(event.tfinger.fingerID);
          prev_pinch_dist = 0.0f;
          break;
        case SDL_EVENT_FINGER_MOTION: {
          auto it = fingers.find(event.tfinger.fingerID);
          if (it != fingers.end()) {
            it->second = {event.tfinger.x, event.tfinger.y};
          }
          if (fingers.size() == 2) {
            int win_w = 1, win_h = 1;
            SDL_GetWindowSizeInPixels(window, &win_w, &win_h);
            auto a = fingers.begin();
            auto b = std::next(a);
            const float dx = (a->second.x - b->second.x) * static_cast<float>(win_w);
            const float dy = (a->second.y - b->second.y) * static_cast<float>(win_h);
            const float dist = std::sqrt(dx * dx + dy * dy);
            if (prev_pinch_dist > 1.0f && dist > 1.0f) {
              app->set_zoom(ImGui::GetIO().FontGlobalScale * (dist / prev_pinch_dist));
            }
            prev_pinch_dist = dist;
          }
          break;
        }
        default:
          break;
      }
#endif
    }

#if defined(__ANDROID__)
    if (SDL_GetAtomicInt(&g_terminating)) {
      running = false;
    }
    if (SDL_GetAtomicInt(&g_in_background)) {
      app->process_pending_actions();
      SDL_Delay(100);
      continue;
    }
#endif

    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();

    const ImGuiID dockspace_id =
        ImGui::DockSpaceOverViewport(0, nullptr, ImGuiDockNodeFlags_PassthruCentralNode);

    static bool dock_layout_initialized = false;
    if (!dock_layout_initialized) {
      dock_layout_initialized = true;
      ImGuiDockNode* node = ImGui::DockBuilderGetNode(dockspace_id);
      if (node == nullptr || node->IsEmpty()) {
        build_default_dock_layout(dockspace_id);
      }
    }

    app->draw();
    if (app->should_quit()) {
      running = false;
    }

    ImGui::Render();
    int w, h;
    SDL_GetWindowSizeInPixels(window, &w, &h);
    glViewport(0, 0, w, h);
    glClearColor(0.08f, 0.08f, 0.10f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    SDL_GL_SwapWindow(window);
  }

  ImGui_ImplOpenGL3_Shutdown();
  ImGui_ImplSDL3_Shutdown();
  ImGui::DestroyContext();

  app.reset();

  SDL_GL_DestroyContext(gl_context);
  SDL_DestroyWindow(window);
  SDL_Quit();
  return 0;
}
