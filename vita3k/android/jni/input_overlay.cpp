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

#include <SDL3/SDL_gamepad.h>
#include <SDL3/SDL_joystick.h>
#include <SDL3/SDL_log.h>
#include <jni.h>

void set_rear_touchscreen(bool is_back);

static int virtual_joystick_id = -1;
static SDL_Joystick *virtual_joystick = nullptr;

extern "C" {

JNIEXPORT void JNICALL
Java_org_vita3k_emulator_overlay_InputOverlay_attachController(JNIEnv *env, jobject thiz) {
    SDL_VirtualJoystickDesc desc;
    SDL_INIT_INTERFACE(&desc);
    desc.type = SDL_JOYSTICK_TYPE_GAMEPAD;
    desc.naxes = SDL_GAMEPAD_AXIS_COUNT;
    desc.nbuttons = SDL_GAMEPAD_BUTTON_COUNT;
    desc.name = "Vita3K Virtual Controller";

    virtual_joystick_id = SDL_AttachVirtualJoystick(&desc);
    if (virtual_joystick_id == 0)
        SDL_Log("Could not create overlay virtual controller: %s", SDL_GetError());
    else {
        virtual_joystick = SDL_OpenJoystick(virtual_joystick_id);
        if (!virtual_joystick)
            SDL_Log("Could not create virtual joystick: %s", SDL_GetError());
    }
}

JNIEXPORT void JNICALL
Java_org_vita3k_emulator_overlay_InputOverlay_detachController(JNIEnv *env, jobject thiz) {
    if (virtual_joystick)
        SDL_CloseJoystick(virtual_joystick);
    if (virtual_joystick_id >= 0)
        SDL_DetachVirtualJoystick(virtual_joystick_id);
    virtual_joystick = nullptr;
    virtual_joystick_id = -1;
}

JNIEXPORT void JNICALL
Java_org_vita3k_emulator_overlay_InputOverlay_setAxis(JNIEnv *env, jobject thiz, jint axis, jshort value) {
    if (!virtual_joystick)
        return;

    SDL_SetJoystickVirtualAxis(virtual_joystick, axis, value);
}

JNIEXPORT void JNICALL
Java_org_vita3k_emulator_overlay_InputOverlay_setButton(JNIEnv *env, jobject thiz, jint button, jboolean value) {
    if (!virtual_joystick)
        return;

    if (button < 0)
        // l2/r2
        SDL_SetJoystickVirtualAxis(virtual_joystick, -button, value ? SDL_MAX_SINT16 : 0);
    else
        SDL_SetJoystickVirtualButton(virtual_joystick, button, value);
}

JNIEXPORT void JNICALL
Java_org_vita3k_emulator_overlay_InputOverlay_setTouchState(JNIEnv *env, jobject thiz, jboolean is_back) {
    set_rear_touchscreen(static_cast<bool>(is_back));
}

} // extern "C"
