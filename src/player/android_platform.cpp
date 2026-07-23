#if defined(__ANDROID__)

#include "android_platform.h"

#include <jni.h>

#include <deque>
#include <mutex>
#include <utility>

#include "third-party/SDL/include/SDL3/SDL_system.h"

namespace {

struct PickedFile {
  std::string name;
  std::vector<uint8_t> data;
};

std::mutex g_mutex;
std::deque<PickedFile> g_picked;

std::mutex g_action_mutex;
std::deque<int> g_actions;

}  // namespace

namespace android_platform {

void open_document() {
  auto* env = static_cast<JNIEnv*>(SDL_GetAndroidJNIEnv());
  auto activity = static_cast<jobject>(SDL_GetAndroidActivity());
  if (env == nullptr || activity == nullptr) {
    return;
  }
  jclass cls = env->GetObjectClass(activity);
  jmethodID mid = env->GetMethodID(cls, "openDocumentPicker", "()V");
  if (mid != nullptr) {
    env->CallVoidMethod(activity, mid);
  }
  if (env->ExceptionCheck()) {
    env->ExceptionClear();
  }
  env->DeleteLocalRef(cls);
  env->DeleteLocalRef(activity);
}

bool poll_picked_file(std::string& name, std::vector<uint8_t>& data) {
  std::lock_guard<std::mutex> lock(g_mutex);
  if (g_picked.empty()) {
    return false;
  }
  PickedFile& f = g_picked.front();
  name = std::move(f.name);
  data = std::move(f.data);
  g_picked.pop_front();
  return true;
}

void set_playback_state(int active_count, bool playing, const std::string& bank_name) {
  static int last_count = -1;
  static int last_playing = -1;
  static std::string last_name;
  const int p = playing ? 1 : 0;
  if (active_count == last_count && p == last_playing && bank_name == last_name) {
    return;
  }
  last_count = active_count;
  last_playing = p;
  last_name = bank_name;

  auto* env = static_cast<JNIEnv*>(SDL_GetAndroidJNIEnv());
  auto activity = static_cast<jobject>(SDL_GetAndroidActivity());
  if (env == nullptr || activity == nullptr) {
    return;
  }
  jclass cls = env->GetObjectClass(activity);
  if (active_count > 0) {
    jmethodID mid =
        env->GetMethodID(cls, "showOrUpdatePlaybackNotification", "(IZLjava/lang/String;)V");
    if (mid != nullptr) {
      jstring jname = env->NewStringUTF(bank_name.c_str());
      env->CallVoidMethod(activity, mid, static_cast<jint>(active_count),
                          static_cast<jboolean>(playing), jname);
      if (jname != nullptr) {
        env->DeleteLocalRef(jname);
      }
    }
  } else {
    jmethodID mid = env->GetMethodID(cls, "hidePlaybackNotification", "()V");
    if (mid != nullptr) {
      env->CallVoidMethod(activity, mid);
    }
  }
  if (env->ExceptionCheck()) {
    env->ExceptionClear();
  }
  env->DeleteLocalRef(cls);
  env->DeleteLocalRef(activity);
}

bool poll_playback_action(int& action) {
  std::lock_guard<std::mutex> lock(g_action_mutex);
  if (g_actions.empty()) {
    return false;
  }
  action = g_actions.front();
  g_actions.pop_front();
  return true;
}

}  // namespace android_platform

extern "C" JNIEXPORT void JNICALL
Java_com_opengoal_sndplayer_SndPlayerActivity_nativeOnDocumentPicked(JNIEnv* env,
                                                                     jclass /*clazz*/,
                                                                     jstring jname,
                                                                     jbyteArray jdata) {
  PickedFile f;
  if (jname != nullptr) {
    const char* chars = env->GetStringUTFChars(jname, nullptr);
    if (chars != nullptr) {
      f.name = chars;
      env->ReleaseStringUTFChars(jname, chars);
    }
  }
  if (jname == nullptr || f.name.empty()) {
    f.name = "bank";
  }
  if (jdata != nullptr) {
    const jsize len = env->GetArrayLength(jdata);
    f.data.resize(static_cast<size_t>(len));
    if (len > 0) {
      env->GetByteArrayRegion(jdata, 0, len, reinterpret_cast<jbyte*>(f.data.data()));
    }
  }
  std::lock_guard<std::mutex> lock(g_mutex);
  g_picked.push_back(std::move(f));
}

extern "C" JNIEXPORT void JNICALL
Java_com_opengoal_sndplayer_SndPlayerActivity_nativeOnPlaybackAction(JNIEnv* /*env*/,
                                                                     jclass /*clazz*/,
                                                                     jint action) {
  std::lock_guard<std::mutex> lock(g_action_mutex);
  g_actions.push_back(static_cast<int>(action));
}

#endif  // __ANDROID__
