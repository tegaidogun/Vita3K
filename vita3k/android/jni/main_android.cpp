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

#include "android_state.h"
#include "interface.h"

#include <app/functions.h>
#include <audio/state.h>
#include <config/settings.h>
#include <ctrl/functions.h>
#include <dialog/state.h>
#include <ime/functions.h>
#include <ime/keyboard.h>
#include <io/state.h>
#include <motion/event_handler.h>
#include <motion/functions.h>
#include <renderer/functions.h>
#include <renderer/state.h>
#include <touch/functions.h>
#include <util/log.h>
#include <util/string_utils.h>

#include <SDL3/SDL.h>

#include <android/native_window.h>

#include <algorithm>
#include <cstdio>
#include <cstring>

namespace {

#if defined(__ANDROID__)
#define VITA3K_ANDROID_EXPORT __attribute__((visibility("default")))
#else
#define VITA3K_ANDROID_EXPORT
#endif

bool is_ime_dialog_active(const EmuEnvState &emuenv) {
    return emuenv.common_dialog.type == IME_DIALOG
        && emuenv.common_dialog.status == SCE_COMMON_DIALOG_STATUS_RUNNING;
}

bool is_any_ime_active(const EmuEnvState &emuenv) {
    return emuenv.ime.state || is_ime_dialog_active(emuenv);
}

void reset_android_session_audio(EmuEnvState &emuenv) {
    if (!emuenv.audio.adapter)
        return;

    LOG_DEBUG("Resetting Android audio backend '{}' after SDL session shutdown", emuenv.audio.audio_backend);
    emuenv.audio.adapter.reset();
    emuenv.audio.audio_backend.clear();
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

void cancel_ime_dialog(EmuEnvState &emuenv) {
    auto &dialog = emuenv.common_dialog;
    if (!dialog.ime.cancelable)
        return;

    std::lock_guard<std::recursive_mutex> dialog_lock(dialog.mutex);
    dialog.ime.status = SCE_IME_DIALOG_BUTTON_CLOSE;
    dialog.status = SCE_COMMON_DIALOG_STATUS_FINISHED;
    dialog.result = SCE_COMMON_DIALOG_RESULT_USER_CANCELED;
}

bool handle_ime_keydown(EmuEnvState &emuenv, const SDL_KeyboardEvent &event) {
    if (!is_any_ime_active(emuenv))
        return false;

    auto &ime = emuenv.ime;
    const bool dialog_ime_active = is_ime_dialog_active(emuenv);

    switch (event.key) {
    case SDLK_BACKSPACE: {
        std::lock_guard<std::mutex> lock(ime.mutex);
        ime_backspace(ime);
        return true;
    }

    case SDLK_RETURN:
    case SDLK_KP_ENTER:
        if (dialog_ime_active) {
            finish_ime_dialog(emuenv);
        } else {
            std::lock_guard<std::mutex> lock(ime.mutex);
            ime.event_id = SCE_IME_EVENT_PRESS_ENTER;
        }
        return true;

    case SDLK_ESCAPE:
        if (dialog_ime_active) {
            cancel_ime_dialog(emuenv);
        } else {
            std::lock_guard<std::mutex> lock(ime.mutex);
            ime.event_id = SCE_IME_EVENT_PRESS_CLOSE;
        }
        return true;

    case SDLK_LEFT: {
        std::lock_guard<std::mutex> lock(ime.mutex);
        ime_cursor_left(ime);
        return true;
    }

    case SDLK_RIGHT: {
        std::lock_guard<std::mutex> lock(ime.mutex);
        ime_cursor_right(ime);
        return true;
    }

    default:
        return false;
    }
}

void handle_ime_text_editing(EmuEnvState &emuenv, const char *text) {
    if (!is_any_ime_active(emuenv))
        return;

    std::lock_guard<std::mutex> lock(emuenv.ime.mutex);
    ime_set_preedit(emuenv.ime, string_utils::utf8_to_utf16(text ? text : ""));
}

void handle_ime_text_input(EmuEnvState &emuenv, const char *text) {
    if (!is_any_ime_active(emuenv) || !text || text[0] == '\0')
        return;

    std::string filtered_text;
    filtered_text.reserve(std::strlen(text));
    for (const char *ch = text; *ch != '\0'; ++ch) {
        if (*ch != '\n' && *ch != '\r')
            filtered_text.push_back(*ch);
    }

    if (filtered_text.empty())
        return;

    std::lock_guard<std::mutex> lock(emuenv.ime.mutex);
    ime_commit_text(emuenv.ime, string_utils::utf8_to_utf16(filtered_text));
}

} // namespace

extern "C" {

// SDL3 entry point — called by SDLActivity's native thread when the Emulator activity starts.
// argv is populated from Emulator.getArguments(), e.g. {"-r", "PCSE00000"}.
VITA3K_ANDROID_EXPORT int SDL_main(int argc, char *argv[]) {
    // Parse title ID from arguments
    std::string title_id;
    for (int i = 0; i < argc; i++) {
        if (std::string(argv[i]) == "-r" && i + 1 < argc) {
            title_id = argv[i + 1];
            break;
        }
    }

    if (title_id.empty()) {
        LOG_ERROR("SDL_main: no title ID provided");
        return -1;
    }

    auto *emuenv = get_emuenv();
    if (!emuenv) {
        LOG_ERROR("SDL_main: emulator not initialized (was NativeLib.init called?)");
        return -1;
    }

    if (!try_begin_game_session()) {
        LOG_ERROR("SDL_main: a game is already running");
        return -1;
    }

    SDL_Window *window = nullptr;
    SDL_GLContext gl_context = nullptr;
    bool launch_prepared = false;
    bool renderer_initialized = false;
    const auto fail_startup = [&]() {
        if (renderer_initialized) {
            app::finish_game_launch(*emuenv);
            reset_android_session_audio(*emuenv);
        } else if (launch_prepared) {
            app::abort_game_launch(*emuenv);
        }
        if (gl_context) {
            SDL_GL_DestroyContext(gl_context);
            gl_context = nullptr;
        }
        reset_game_session_runtime();
        if (window)
            SDL_DestroyWindow(window);
        SDL_Quit();
        return -1;
    };

    LOG_INFO("SDL_main: booting game '{}'", title_id);

    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMEPAD | SDL_INIT_HAPTIC | SDL_INIT_SENSOR | SDL_INIT_CAMERA)) {
        LOG_ERROR("SDL_Init failed: {}", SDL_GetError());
        return fail_startup();
    }

    const auto open_pause_menu = []() {
        JNIEnv *jni_env = reinterpret_cast<JNIEnv *>(SDL_GetAndroidJNIEnv());
        jobject activity = reinterpret_cast<jobject>(SDL_GetAndroidActivity());
        if (!jni_env || !activity)
            return;

        jclass clazz = jni_env->GetObjectClass(activity);
        jmethodID method_id = jni_env->GetMethodID(clazz, "openPauseMenuFromController", "()V");
        if (method_id)
            jni_env->CallVoidMethod(activity, method_id);

        jni_env->DeleteLocalRef(clazz);
        jni_env->DeleteLocalRef(activity);
    };

    {
        JNIEnv *jni_env = reinterpret_cast<JNIEnv *>(SDL_GetAndroidJNIEnv());
        jobject activity = reinterpret_cast<jobject>(SDL_GetAndroidActivity());
        jclass clazz = jni_env->GetObjectClass(activity);
        jmethodID method_id = jni_env->GetMethodID(clazz, "getNativeDisplayRotation", "()I");
        set_display_rotation(jni_env->CallIntMethod(activity, method_id));
        jni_env->DeleteLocalRef(clazz);
        jni_env->DeleteLocalRef(activity);
    }

    if (!app::setup_game_launch(*emuenv, title_id)) {
        LOG_ERROR("Could not find app '{}' in apps list.", title_id);
        return fail_startup();
    }
    launch_prepared = true;

    SDL_WindowFlags window_flags = 0;
    if (emuenv->backend_renderer == renderer::Backend::OpenGL) {
        window_flags |= SDL_WINDOW_OPENGL;
        if (!SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES)
            || !SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3)
            || !SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2)
            || !SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1)
            || !SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 0)) {
            LOG_ERROR("Failed to configure OpenGL ES context attributes: {}", SDL_GetError());
            return fail_startup();
        }
#ifndef NDEBUG
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_DEBUG_FLAG);
#endif
    }

    window = SDL_CreateWindow("Vita3K", 960, 544, window_flags);
    if (!window) {
        LOG_ERROR("SDL_CreateWindow failed: {}", SDL_GetError());
        return fail_startup();
    }
    ime::set_sdl_window(window);

    auto *native_window = static_cast<ANativeWindow *>(
        SDL_GetPointerProperty(SDL_GetWindowProperties(window),
            SDL_PROP_WINDOW_ANDROID_WINDOW_POINTER, nullptr));
    if (!native_window) {
        LOG_ERROR("Failed to get ANativeWindow from SDL window");
        return fail_startup();
    }

    set_game_window(native_window);

    renderer::WindowCallbacks callbacks;
    callbacks.native_handle = native_window;
    callbacks.display_protocol = renderer::DisplayProtocol::Android;
    callbacks.client_width = [window]() -> int {
        int w = 960;
        SDL_GetWindowSize(window, &w, nullptr);
        return w;
    };
    callbacks.client_height = [window]() -> int {
        int h = 544;
        SDL_GetWindowSize(window, nullptr, &h);
        return h;
    };
    callbacks.get_font_dirs = []() -> std::vector<std::string> {
        return { "/system/fonts/" };
    };

    if (emuenv->backend_renderer == renderer::Backend::OpenGL) {
        gl_context = SDL_GL_CreateContext(window);
        if (!gl_context) {
            LOG_ERROR("Failed to create OpenGL ES context: {}", SDL_GetError());
            return fail_startup();
        }

        if (!SDL_GL_MakeCurrent(window, gl_context)) {
            LOG_ERROR("Failed to make OpenGL ES context current: {}", SDL_GetError());
            return fail_startup();
        }

        if (!SDL_GL_SetSwapInterval(static_cast<int>(emuenv->cfg.current_config.v_sync)))
            LOG_WARN("Failed to set OpenGL ES swap interval: {}", SDL_GetError());

        callbacks.get_proc_address = [](const char *name) -> void * {
            return reinterpret_cast<void *>(SDL_GL_GetProcAddress(name));
        };
        callbacks.default_fbo = []() -> unsigned int {
            return 0;
        };
        callbacks.swap = [window]() {
            SDL_GL_SwapWindow(window);
        };
        callbacks.set_vsync = [](bool enabled) -> bool {
            return SDL_GL_SetSwapInterval(enabled ? 1 : 0);
        };
        callbacks.set_current = [window, gl_context]() -> bool {
            return SDL_GL_MakeCurrent(window, gl_context);
        };
        callbacks.done_current = [window]() {
            SDL_GL_MakeCurrent(window, nullptr);
        };
    }

    if (!renderer::init(callbacks, emuenv->renderer, emuenv->backend_renderer, emuenv->cfg, emuenv->get_root_paths())) {
        LOG_ERROR("Failed to initialise renderer.");
        return fail_startup();
    }
    renderer_initialized = true;

    app::apply_renderer_config(*emuenv);

    if (!app::late_init(*emuenv)) {
        LOG_ERROR("Failed late initialisation.");
        return fail_startup();
    }

    int32_t main_module_id = 0;
    if (load_app(main_module_id, *emuenv) != Success) {
        LOG_ERROR("Failed to load app modules.");
        return fail_startup();
    }

    emuenv->renderer->set_app(emuenv->io.title_id.c_str(), emuenv->self_name.c_str());

    app::prepare_game_launch_overlay(*emuenv);

    if (run_app(*emuenv, main_module_id) != Success) {
        LOG_ERROR("Failed to start game threads.");
        return fail_startup();
    }

    if (emuenv->backend_renderer == renderer::Backend::OpenGL)
        SDL_GL_MakeCurrent(window, nullptr);

    renderer::start_render_thread(*emuenv->renderer, emuenv->display, emuenv->gxm, emuenv->mem, emuenv->cfg);

    LOG_INFO("Game started: {} ({})", emuenv->current_app_title, title_id);
    app::LaunchRuntimeMetrics runtime_metrics{};

    // Main event loop — runs until the game exits or the activity is destroyed.
    // Dispatches touch, gamepad, and sensor events to match the Qt desktop implementation.
    bool running = true;
    while (running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            switch (event.type) {
            case SDL_EVENT_QUIT:
            case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
                running = false;
                break;

            case SDL_EVENT_KEY_DOWN:
                if (handle_ime_keydown(*emuenv, event.key))
                    break;
                break;

            case SDL_EVENT_TEXT_EDITING:
                handle_ime_text_editing(*emuenv, event.edit.text);
                break;

            case SDL_EVENT_TEXT_INPUT:
                handle_ime_text_input(*emuenv, event.text.text);
                break;

            // Touch events → Vita front/back touchscreen + overlay mouse
            case SDL_EVENT_FINGER_DOWN:
            case SDL_EVENT_FINGER_MOTION:
            case SDL_EVENT_FINGER_UP: {
                handle_touch_event(event.tfinger);
                // Update overlay mouse state for C++ overlay system (common dialogs, etc.)
                // SDL finger coordinates are normalized [0,1], map to 960×544 virtual viewport
                auto &mouse = emuenv->ctrl.overlay_mouse;
                mouse.x.store(event.tfinger.x * 960.f, std::memory_order_relaxed);
                mouse.y.store(event.tfinger.y * 544.f, std::memory_order_relaxed);
                mouse.pressed.store(event.type != SDL_EVENT_FINGER_UP, std::memory_order_relaxed);
                break;
            }

            // Gamepad added/removed → refresh controller mapping
            case SDL_EVENT_GAMEPAD_ADDED:
            case SDL_EVENT_GAMEPAD_REMOVED:
                refresh_controllers(emuenv->ctrl, *emuenv);
                break;

            case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
                if (event.gbutton.button == SDL_GAMEPAD_BUTTON_GUIDE)
                    open_pause_menu();
                break;

            // Gamepad touchpad → Vita touchscreen (e.g. DS4/DS5 touchpad)
            case SDL_EVENT_GAMEPAD_TOUCHPAD_DOWN:
            case SDL_EVENT_GAMEPAD_TOUCHPAD_MOTION:
            case SDL_EVENT_GAMEPAD_TOUCHPAD_UP:
                handle_touchpad_event(event.gtouchpad);
                break;

            // Motion/sensor events
            case SDL_EVENT_GAMEPAD_SENSOR_UPDATE:
                handle_motion_event(*emuenv, event.gsensor.sensor, event.gsensor);
                break;
            case SDL_EVENT_SENSOR_UPDATE:
                handle_motion_event(*emuenv, SDL_GetSensorTypeForID(event.sensor.which), event.sensor);
                break;

            default:
                break;
            }
        }

        app::update_runtime_metrics(*emuenv, runtime_metrics);

        if (!has_game_running())
            running = false;

        if (running)
            SDL_Delay(16);
    }

    // Cleanup
    LOG_INFO("SDL_main: shutting down game");
    app::finish_game_launch(*emuenv);
    reset_android_session_audio(*emuenv);
    if (gl_context) {
        SDL_GL_DestroyContext(gl_context);
        gl_context = nullptr;
    }
    reset_game_session_runtime();
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}

} // extern "C"
