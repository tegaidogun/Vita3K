// Vita3K emulator project
// Copyright (C) 2026 Vita3K team
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.

#include "android_state.h"

#include <app/functions.h>
#include <audio/state.h>
#include <ctrl/state.h>
#include <dialog/state.h>
#include <ime/functions.h>
#include <kernel/state.h>
#include <renderer/state.h>
#include <util/string_utils.h>

#include <jni.h>

#include <algorithm>
#include <atomic>
#include <cstdio>
#include <cstring>

namespace {

bool is_ime_dialog_active(const EmuEnvState &emuenv) {
    return emuenv.common_dialog.type == IME_DIALOG
        && emuenv.common_dialog.status == SCE_COMMON_DIALOG_STATUS_RUNNING;
}

bool is_any_ime_active(const EmuEnvState &emuenv) {
    return emuenv.ime.state || is_ime_dialog_active(emuenv);
}

bool set_game_paused_internal(bool paused) {
    auto *emuenv = get_emuenv();
    if (!emuenv || !has_game_running())
        return false;

    const bool currently_paused = emuenv->kernel.is_threads_paused();
    if (currently_paused != paused) {
        if (paused) {
            emuenv->kernel.pause_threads();
        } else {
            emuenv->kernel.resume_threads();
        }
    }
    if (emuenv->audio.adapter)
        emuenv->audio.switch_state(paused);

    emuenv->drop_inputs = paused;
    emuenv->ctrl.overlay_input_intercepted.store(paused, std::memory_order_relaxed);
    if (paused)
        emuenv->ctrl.ignore_input = true;

    if (emuenv->renderer)
        emuenv->renderer->paused.store(paused, std::memory_order_relaxed);

    return true;
}

void finish_ime_dialog(EmuEnvState &emuenv) {
    auto &dialog = emuenv.common_dialog;
    auto &ime = emuenv.ime;

    std::lock_guard<std::recursive_mutex> dialog_lock(dialog.mutex);
    std::lock_guard<std::mutex> ime_lock(ime.mutex);

    const size_t copy_len = std::min(static_cast<size_t>(ime.str.length()),
        static_cast<size_t>(dialog.ime.max_length));
    if (dialog.ime.result) {
        std::memcpy(dialog.ime.result, ime.str.c_str(), copy_len * sizeof(uint16_t));
        dialog.ime.result[copy_len] = 0;
    }

    const std::string utf8 = string_utils::utf16_to_utf8(ime.str);
    std::snprintf(dialog.ime.text, sizeof(dialog.ime.text), "%s", utf8.c_str());
    dialog.ime.status = SCE_IME_DIALOG_BUTTON_ENTER;
    dialog.status = SCE_COMMON_DIALOG_STATUS_FINISHED;
    dialog.result = SCE_COMMON_DIALOG_RESULT_OK;
}

bool submit_current_ime(EmuEnvState &emuenv) {
    if (!is_any_ime_active(emuenv))
        return false;

    if (is_ime_dialog_active(emuenv)) {
        finish_ime_dialog(emuenv);
    } else {
        std::lock_guard<std::mutex> lock(emuenv.ime.mutex);
        emuenv.ime.event_id = SCE_IME_EVENT_PRESS_ENTER;
    }

    return true;
}

bool dismiss_current_ime(EmuEnvState &emuenv) {
    if (!is_any_ime_active(emuenv))
        return false;

    if (is_ime_dialog_active(emuenv)) {
        auto &dialog = emuenv.common_dialog;
        if (!dialog.ime.cancelable)
            return false;

        std::lock_guard<std::recursive_mutex> dialog_lock(dialog.mutex);
        dialog.ime.status = SCE_IME_DIALOG_BUTTON_CLOSE;
        dialog.status = SCE_COMMON_DIALOG_STATUS_FINISHED;
        dialog.result = SCE_COMMON_DIALOG_RESULT_USER_CANCELED;
    } else {
        std::lock_guard<std::mutex> lock(emuenv.ime.mutex);
        emuenv.ime.event_id = SCE_IME_EVENT_PRESS_CLOSE;
    }

    return true;
}

} // namespace

extern "C" {

JNIEXPORT jboolean JNICALL
Java_org_vita3k_emulator_NativeLib_pauseGame(JNIEnv *, jclass) {
    return set_game_paused_internal(true) ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jboolean JNICALL
Java_org_vita3k_emulator_NativeLib_resumeGame(JNIEnv *, jclass) {
    return set_game_paused_internal(false) ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jboolean JNICALL
Java_org_vita3k_emulator_NativeLib_setInputIntercepted(JNIEnv *, jclass, jboolean intercepted) {
    auto *emuenv = get_emuenv();
    if (!emuenv || !has_game_running())
        return JNI_FALSE;

    auto &ctrl = emuenv->ctrl;
    const bool enabled = intercepted == JNI_TRUE;
    ctrl.overlay_input_intercepted.store(enabled, std::memory_order_relaxed);
    if (!enabled) {
        std::lock_guard<std::mutex> lock(ctrl.mutex);
        ctrl.ignore_input = true;
    }

    return JNI_TRUE;
}

JNIEXPORT jboolean JNICALL
Java_org_vita3k_emulator_NativeLib_setGamePaused(JNIEnv *, jclass, jboolean paused) {
    return set_game_paused_internal(paused == JNI_TRUE) ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jboolean JNICALL
Java_org_vita3k_emulator_NativeLib_isGameRunning(JNIEnv *, jclass) {
    return has_game_running() ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jboolean JNICALL
Java_org_vita3k_emulator_NativeLib_isGamePaused(JNIEnv *, jclass) {
    auto *emuenv = get_emuenv();
    if (!emuenv || !has_game_running())
        return JNI_FALSE;

    return emuenv->kernel.is_threads_paused() ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jstring JNICALL
Java_org_vita3k_emulator_NativeLib_getRunningGameTitle(JNIEnv *env, jclass) {
    auto *emuenv = get_emuenv();
    if (!emuenv || !has_game_running())
        return env->NewStringUTF("");

    return env->NewStringUTF(emuenv->current_app_title.c_str());
}

JNIEXPORT jboolean JNICALL
Java_org_vita3k_emulator_NativeLib_isImeActive(JNIEnv *, jclass) {
    auto *emuenv = get_emuenv();
    if (!emuenv || !has_game_running())
        return JNI_FALSE;

    return is_any_ime_active(*emuenv) ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jboolean JNICALL
Java_org_vita3k_emulator_NativeLib_submitIme(JNIEnv *, jclass) {
    auto *emuenv = get_emuenv();
    if (!emuenv || !has_game_running())
        return JNI_FALSE;

    return submit_current_ime(*emuenv) ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jboolean JNICALL
Java_org_vita3k_emulator_NativeLib_dismissIme(JNIEnv *, jclass) {
    auto *emuenv = get_emuenv();
    if (!emuenv || !has_game_running())
        return JNI_FALSE;

    return dismiss_current_ime(*emuenv) ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jboolean JNICALL
Java_org_vita3k_emulator_NativeLib_commitImeText(JNIEnv *env, jclass, jstring text_str) {
    auto *emuenv = get_emuenv();
    if (!emuenv || !has_game_running() || !text_str || !is_any_ime_active(*emuenv))
        return JNI_FALSE;

    std::string text = jstring_to_string(env, text_str);
    text.erase(std::remove(text.begin(), text.end(), '\n'), text.end());
    text.erase(std::remove(text.begin(), text.end(), '\r'), text.end());
    if (text.empty())
        return JNI_TRUE;

    std::lock_guard<std::mutex> lock(emuenv->ime.mutex);
    ime_commit_text(emuenv->ime, string_utils::utf8_to_utf16(text));
    return JNI_TRUE;
}

JNIEXPORT jboolean JNICALL
Java_org_vita3k_emulator_NativeLib_setImeComposingText(JNIEnv *env, jclass, jstring text_str) {
    auto *emuenv = get_emuenv();
    if (!emuenv || !has_game_running() || !is_any_ime_active(*emuenv))
        return JNI_FALSE;

    const std::string text = text_str ? jstring_to_string(env, text_str) : std::string();

    std::lock_guard<std::mutex> lock(emuenv->ime.mutex);
    ime_set_preedit(emuenv->ime, string_utils::utf8_to_utf16(text));
    return JNI_TRUE;
}

JNIEXPORT jboolean JNICALL
Java_org_vita3k_emulator_NativeLib_backspaceIme(JNIEnv *, jclass, jint count) {
    auto *emuenv = get_emuenv();
    if (!emuenv || !has_game_running() || !is_any_ime_active(*emuenv))
        return JNI_FALSE;

    const jint repeat_count = std::max<jint>(count, 1);
    std::lock_guard<std::mutex> lock(emuenv->ime.mutex);
    for (jint i = 0; i < repeat_count; ++i)
        ime_backspace(emuenv->ime);

    return JNI_TRUE;
}

JNIEXPORT jobject JNICALL
Java_org_vita3k_emulator_NativeLib_getImeState(JNIEnv *env, jclass) {
    auto *emuenv = get_emuenv();
    if (!emuenv || !has_game_running() || !is_any_ime_active(*emuenv))
        return nullptr;

    jclass ime_state_class = env->FindClass("org/vita3k/emulator/data/NativeImeState");
    if (!ime_state_class)
        return nullptr;

    jmethodID ctor = env->GetMethodID(ime_state_class, "<init>", "(ZZLjava/lang/String;IIIZLjava/lang/String;)V");
    if (!ctor)
        return nullptr;

    const bool dialog_active = is_ime_dialog_active(*emuenv);
    const bool sce_ime_active = emuenv->ime.state;

    std::u16string text;
    uint32_t preedit_start = 0;
    uint32_t preedit_length = 0;
    uint32_t caret_index = 0;
    bool multiline = false;
    std::string enter_label;
    {
        std::lock_guard<std::mutex> lock(emuenv->ime.mutex);
        text = emuenv->ime.str;
        preedit_start = emuenv->ime.edit_text.preeditIndex;
        preedit_length = emuenv->ime.edit_text.preeditLength;
        caret_index = emuenv->ime.edit_text.caretIndex;
        multiline = dialog_active
            ? emuenv->common_dialog.ime.multiline
            : ((emuenv->ime.param.option & SCE_IME_OPTION_MULTILINE) != 0);
        enter_label = emuenv->ime.enter_label;
    }

    const std::string utf8_text = string_utils::utf16_to_utf8(text);
    jstring text_value = env->NewStringUTF(utf8_text.c_str());
    jstring enter_label_value = env->NewStringUTF(enter_label.c_str());

    jobject result = env->NewObject(
        ime_state_class,
        ctor,
        static_cast<jboolean>(sce_ime_active),
        static_cast<jboolean>(dialog_active),
        text_value,
        static_cast<jint>(preedit_start),
        static_cast<jint>(preedit_length),
        static_cast<jint>(caret_index),
        static_cast<jboolean>(multiline),
        enter_label_value);

    env->DeleteLocalRef(text_value);
    env->DeleteLocalRef(enter_label_value);
    return result;
}

} // extern "C"
