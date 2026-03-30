// Vita3K emulator project
// Copyright (C) 2026 Vita3K team
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License along
// with this program; if not, write to the Free Software Foundation, Inc.,
// 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

#include <ime/keyboard.h>

#include <SDL3/SDL_keyboard.h>
#include <SDL3/SDL_system.h>
#include <jni.h>

namespace ime {

static SDL_Window *s_window = nullptr;

void set_sdl_window(SDL_Window *window) {
    s_window = window;
}

void set_keyboard_active(bool active) {
    if (s_window) {
        if (active)
            SDL_StartTextInput(s_window);
        else
            SDL_StopTextInput(s_window);
    }

    JNIEnv *env = reinterpret_cast<JNIEnv *>(SDL_GetAndroidJNIEnv());
    jobject activity = reinterpret_cast<jobject>(SDL_GetAndroidActivity());
    jclass clazz = env->GetObjectClass(activity);
    jmethodID method_id = env->GetMethodID(clazz, "setKeyboardActive", "(Z)V");
    if (method_id)
        env->CallVoidMethod(activity, method_id, static_cast<jboolean>(active));
    env->DeleteLocalRef(clazz);
    env->DeleteLocalRef(activity);
}

} // namespace ime
