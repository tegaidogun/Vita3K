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

#include <app/functions.h>

#include <audio/state.h>
#include <camera/state.h>
#include <config/settings.h>
#include <config/state.h>
#include <config/version.h>
#include <ctrl/functions.h>
#include <ctrl/state.h>
#include <dialog/state.h>
#include <display/state.h>
#include <emuenv/state.h>
#include <gxm/state.h>
#include <http/state.h>
#include <ime/state.h>
#include <io/functions.h>
#include <kernel/state.h>
#include <lang/state.h>
#include <mem/functions.h>
#include <modules/module_parent.h>
#include <motion/state.h>
#include <net/state.h>
#include <ngs/state.h>
#include <np/functions.h>
#include <np/state.h>
#include <np/trophy/context.h>
#include <overlay/display_manager.h>
#include <overlay/input.h>
#include <overlay/trophy_notification.h>
#include <packages/sfo.h>
#include <regmgr/state.h>
#include <renderer/functions.h>
#include <renderer/state.h>

#include <util/fs.h>
#include <util/log.h>
#include <util/string_utils.h>

#if USE_DISCORD
#include <app/discord.h>
#endif

#include <gdbstub/functions.h>

#include <SDL3/SDL_filesystem.h>
#include <SDL3/SDL_gamepad.h>

#ifdef __ANDROID__
#include <SDL3/SDL_system.h>
#endif

#include <algorithm>
#include <fstream>

namespace app {

static float sdl_axis_to_float(int16_t axis, float mult) {
    const float unsigned_axis = static_cast<float>(axis - INT16_MIN);
    const float f = unsigned_axis / static_cast<float>(UINT16_MAX);
    return std::clamp(((f * 2.0f) - 1.0f) * mult, -1.0f, 1.0f);
}

static overlay::button_states poll_overlay_input(EmuEnvState &emuenv) {
    overlay::button_states states{};
    uint32_t buttons = 0;
    float axes[4] = {};

    std::lock_guard<std::mutex> lock(emuenv.ctrl.mutex);

    auto &kb = emuenv.ctrl.keyboard_state;
    buttons |= kb.buttons_ext;
    axes[0] += kb.axes[0];
    axes[1] += kb.axes[1];
    axes[2] += kb.axes[2];
    axes[3] += kb.axes[3];

    for (const auto &[_, controller] : emuenv.ctrl.controllers) {
        SDL_Gamepad *gp = controller.controller.get();
        for (const auto &binding : get_controller_bindings_ext(emuenv)) {
            if (SDL_GetGamepadButton(gp, binding.controller))
                buttons |= binding.button;
        }

        const auto &axis_binds = emuenv.cfg.controller_axis_binds;
        if (axis_binds.size() > 5) {
            if (SDL_GetGamepadAxis(gp, static_cast<SDL_GamepadAxis>(axis_binds[4])) > 0x3FFF)
                buttons |= SCE_CTRL_L2;
            if (SDL_GetGamepadAxis(gp, static_cast<SDL_GamepadAxis>(axis_binds[5])) > 0x3FFF)
                buttons |= SCE_CTRL_R2;
        }

        if (axis_binds.size() > 3) {
            const float mult = emuenv.cfg.controller_analog_multiplier;
            axes[0] += sdl_axis_to_float(SDL_GetGamepadAxis(gp, static_cast<SDL_GamepadAxis>(axis_binds[0])), mult);
            axes[1] += sdl_axis_to_float(SDL_GetGamepadAxis(gp, static_cast<SDL_GamepadAxis>(axis_binds[1])), mult);
            axes[2] += sdl_axis_to_float(SDL_GetGamepadAxis(gp, static_cast<SDL_GamepadAxis>(axis_binds[2])), mult);
            axes[3] += sdl_axis_to_float(SDL_GetGamepadAxis(gp, static_cast<SDL_GamepadAxis>(axis_binds[3])), mult);
        }
    }

    auto set = [&](uint32_t mask, overlay::pad_button btn) {
        if (buttons & mask)
            states[static_cast<size_t>(btn)] = true;
    };
    set(SCE_CTRL_UP, overlay::pad_button::dpad_up);
    set(SCE_CTRL_DOWN, overlay::pad_button::dpad_down);
    set(SCE_CTRL_LEFT, overlay::pad_button::dpad_left);
    set(SCE_CTRL_RIGHT, overlay::pad_button::dpad_right);
    set(SCE_CTRL_TRIANGLE, overlay::pad_button::triangle);
    set(SCE_CTRL_CIRCLE, overlay::pad_button::circle);
    set(SCE_CTRL_CROSS, overlay::pad_button::cross);
    set(SCE_CTRL_SQUARE, overlay::pad_button::square);
    set(SCE_CTRL_START, overlay::pad_button::start);
    set(SCE_CTRL_SELECT, overlay::pad_button::select);
    set(SCE_CTRL_L1, overlay::pad_button::L1);
    set(SCE_CTRL_R1, overlay::pad_button::R1);
    set(SCE_CTRL_L2, overlay::pad_button::L2);
    set(SCE_CTRL_R2, overlay::pad_button::R2);
    set(SCE_CTRL_L3, overlay::pad_button::L3);
    set(SCE_CTRL_R3, overlay::pad_button::R3);

    constexpr float threshold = 0.5f;
    if (axes[0] < -threshold)
        states[static_cast<size_t>(overlay::pad_button::ls_left)] = true;
    if (axes[0] > threshold)
        states[static_cast<size_t>(overlay::pad_button::ls_right)] = true;
    if (axes[1] < -threshold)
        states[static_cast<size_t>(overlay::pad_button::ls_up)] = true;
    if (axes[1] > threshold)
        states[static_cast<size_t>(overlay::pad_button::ls_down)] = true;
    if (axes[2] < -threshold)
        states[static_cast<size_t>(overlay::pad_button::rs_left)] = true;
    if (axes[2] > threshold)
        states[static_cast<size_t>(overlay::pad_button::rs_right)] = true;
    if (axes[3] < -threshold)
        states[static_cast<size_t>(overlay::pad_button::rs_up)] = true;
    if (axes[3] > threshold)
        states[static_cast<size_t>(overlay::pad_button::rs_down)] = true;

    return states;
}

static void set_backend_renderer(EmuEnvState &emuenv, const std::string &backend_renderer) {
#ifndef __APPLE__
    emuenv.backend_renderer = (string_utils::toupper(backend_renderer) == "OPENGL")
        ? renderer::Backend::OpenGL
        : renderer::Backend::Vulkan;
#else
    emuenv.backend_renderer = renderer::Backend::Vulkan;
#endif
}

void set_current_config(EmuEnvState &emuenv, const std::string &app_path) {
    config::set_current_config(emuenv.cfg, emuenv.config_path, app_path);
    set_backend_renderer(emuenv, emuenv.cfg.current_config.backend_renderer);
    emuenv.audio.set_global_volume(emuenv.cfg.current_config.audio_volume / 100.f);
    lang::set_locale(emuenv.cfg.current_config.sys_lang);
}

void init_paths(Root &root_paths) {
#ifdef __ANDROID__
    fs::path storage_path = fs::path(SDL_GetAndroidExternalStoragePath()) / "";
    fs::path vita_storage_path = storage_path / "vita/";

    root_paths.set_base_path(storage_path);

    // On Android, static assets are bundled inside the APK and accessed via SDL_IOFromFile.
    root_paths.set_static_assets_path({});

    root_paths.set_pref_path(vita_storage_path);
    root_paths.set_log_path(storage_path);
    root_paths.set_config_path(storage_path);
    root_paths.set_shared_path(storage_path);
    root_paths.set_cache_path(storage_path / "cache" / "");
    root_paths.set_patch_path(storage_path / "patch" / "");
#else
    auto sdl_base_path = SDL_GetBasePath();
    auto base_path = fs_utils::utf8_to_path(sdl_base_path);

    root_paths.set_base_path(base_path);
    root_paths.set_static_assets_path(base_path);

#if defined(__APPLE__)
    // On Apple platforms, base_path is "Contents/Resources/" inside the app bundle.
    // An extra parent_path is apparently needed because of the trailing slash.
    auto portable_path = base_path.parent_path().parent_path().parent_path().parent_path() / "portable" / "";
#else
    auto portable_path = base_path / "portable" / "";
#endif

    if (fs::is_directory(portable_path)) {
        // If a portable directory exists, use it for everything else.
        // Note that pref_path should not be the same as the other paths.
        root_paths.set_pref_path(portable_path / "fs" / "");
        root_paths.set_log_path(portable_path);
        root_paths.set_config_path(portable_path);
        root_paths.set_shared_path(portable_path);
        root_paths.set_cache_path(portable_path / "cache" / "");
        root_paths.set_patch_path(portable_path / "patch" / "");
    } else {
        // SDL_GetPrefPath is deferred as it creates the directory.
        // When using a portable directory, it is not needed.
        auto sdl_pref_path = SDL_GetPrefPath(org_name, app_name);
        auto pref_path = fs_utils::utf8_to_path(sdl_pref_path);
        SDL_free(sdl_pref_path);

#if defined(__APPLE__)
        // Store other data in the user-wide path. Otherwise we may end up dumping
        // files into the "/Applications/" install directory or the app bundle.
        // This will typically be "~/Library/Application Support/Vita3K/Vita3K/".
        // Check for config.yml first, though, to maintain backwards compatibility,
        // even though storing user data inside the app bundle is not a good idea.
        auto existing_config = base_path / "config.yml";
        if (!fs::exists(existing_config)) {
            base_path = pref_path;
        }

        // pref_path should not be the same as the other paths.
        // For backwards compatibility, though, check if ux0 exists first.
        auto existing_ux0 = pref_path / "ux0";
        if (!fs::is_directory(existing_ux0)) {
            pref_path = pref_path / "fs" / "";
        }
#endif

        root_paths.set_pref_path(pref_path);
        root_paths.set_log_path(base_path);
        root_paths.set_config_path(base_path);
        root_paths.set_shared_path(base_path);
        root_paths.set_cache_path(base_path / "cache" / "");
        root_paths.set_patch_path(base_path / "patch" / "");

#if defined(__linux__)
        // XDG Data Dirs.
        auto env_home = getenv("HOME");
        auto XDG_DATA_DIRS = getenv("XDG_DATA_DIRS");
        auto XDG_DATA_HOME = getenv("XDG_DATA_HOME");
        auto XDG_CACHE_HOME = getenv("XDG_CACHE_HOME");
        auto XDG_CONFIG_HOME = getenv("XDG_CONFIG_HOME");
        auto APPDIR = getenv("APPDIR"); // Used in AppImage

        if (XDG_DATA_HOME != NULL)
            root_paths.set_pref_path(fs::path(XDG_DATA_HOME) / app_name / app_name / "");

        if (XDG_CONFIG_HOME != NULL)
            root_paths.set_config_path(fs::path(XDG_CONFIG_HOME) / app_name / "");
        else if (env_home != NULL)
            root_paths.set_config_path(fs::path(env_home) / ".config" / app_name / "");

        if (XDG_CACHE_HOME != NULL) {
            root_paths.set_cache_path(fs::path(XDG_CACHE_HOME) / app_name / "");
            root_paths.set_log_path(fs::path(XDG_CACHE_HOME) / app_name / "");
        } else if (env_home != NULL) {
            root_paths.set_cache_path(fs::path(env_home) / ".cache" / app_name / "");
            root_paths.set_log_path(fs::path(env_home) / ".cache" / app_name / "");
        }

        // Don't assume that base_path is portable.
        if (fs::exists(root_paths.get_base_path() / "data") && fs::exists(root_paths.get_base_path() / "shaders-builtin"))
            root_paths.set_static_assets_path(root_paths.get_base_path());
        else if (env_home != NULL)
            root_paths.set_static_assets_path(fs::path(env_home) / ".local/share" / app_name / "");

        if (XDG_DATA_DIRS != NULL) {
            auto env_paths = string_utils::split_string(XDG_DATA_DIRS, ':');
            for (auto &i : env_paths) {
                if (fs::exists(fs::path(i) / app_name)) {
                    root_paths.set_static_assets_path(fs::path(i) / app_name / "");
                    break;
                }
            }
        } else if (XDG_DATA_HOME != NULL) {
            if (fs::exists(fs::path(XDG_DATA_HOME) / app_name / "data") && fs::exists(fs::path(XDG_DATA_HOME) / app_name / "shaders-builtin"))
                root_paths.set_static_assets_path(fs::path(XDG_DATA_HOME) / app_name / "");
        }

        if (APPDIR != NULL && fs::exists(fs::path(APPDIR) / "usr/share/Vita3K")) {
            root_paths.set_static_assets_path(fs::path(APPDIR) / "usr/share/Vita3K");
        }

        // shared path
        if (env_home != NULL)
            root_paths.set_shared_path(fs::path(env_home) / ".local/share" / app_name / "");

        if (XDG_DATA_HOME != NULL) {
            root_paths.set_shared_path(fs::path(XDG_DATA_HOME) / app_name / "");
        }

        // patch path should be in shared path
        root_paths.set_patch_path(root_paths.get_shared_path() / "patch" / "");
#endif
    }
#endif

    // Create default preference and cache path for safety
    fs::create_directories(root_paths.get_config_path());
    fs::create_directories(root_paths.get_cache_path());
    fs::create_directories(root_paths.get_log_path() / "shaderlog");
    fs::create_directories(root_paths.get_log_path() / "texturelog");
    fs::create_directories(root_paths.get_patch_path());
}

bool init(EmuEnvState &state, Config &cfg, const Root &root_paths) {
    state.cfg = std::move(cfg);

    state.base_path = root_paths.get_base_path();
    state.default_path = root_paths.get_pref_path();
    state.log_path = root_paths.get_log_path();
    state.config_path = root_paths.get_config_path();
    state.cache_path = root_paths.get_cache_path();
    state.shared_path = root_paths.get_shared_path();
    state.static_assets_path = root_paths.get_static_assets_path();
    state.patch_path = root_paths.get_patch_path();

    // If configuration does not provide a preference path, use SDL's default
    if (state.cfg.pref_path == root_paths.get_pref_path() || state.cfg.pref_path.empty()) {
        state.pref_path = root_paths.get_pref_path();
    } else {
        auto last_char = state.cfg.pref_path.back();
        if (last_char != fs::path::preferred_separator && last_char != '/')
            state.cfg.pref_path += fs::path::preferred_separator;
        state.pref_path = state.cfg.get_pref_path();
    }

    set_current_config(state, state.cfg.run_app_path.has_value() ? *state.cfg.run_app_path : "");

    LOG_INFO("backend-renderer: {}", state.cfg.current_config.backend_renderer);

    LOG_INFO("Base path: {}", state.base_path);
#if defined(__linux__) && !defined(__ANDROID__)
    LOG_INFO("Static assets path: {}", state.static_assets_path);
    LOG_INFO("Shared path: {}", state.shared_path);
    LOG_INFO("Log path: {}", state.log_path);
    LOG_INFO("User config path: {}", state.config_path);
    LOG_INFO("User cache path: {}", state.cache_path);
#endif
    LOG_INFO("User pref path: {}", state.pref_path);

    if (!init(state.io, state.cache_path, state.log_path, state.pref_path, state.cfg.console)) {
        LOG_ERROR("Failed to initialize file system for the emulator!");
        return false;
    }

#ifdef __ANDROID__
    if (state.renderer)
        state.renderer->current_custom_driver = state.cfg.current_config.custom_driver_name;
#endif

#if USE_DISCORD
    if (state.cfg.discord_rich_presence && discordrpc::init()) {
        discordrpc::update_presence();
    }
#endif

    state.motion.init();

    return true;
}

void deinit(EmuEnvState &state) {
    state.audio.stop_all_ports();

    state.display.abort = true;
    state.renderer->notification_ready.notify_all();

    state.gxm.display_queue.abort();

    state.renderer->render_abort = true;
    state.renderer->command_finish_one.notify_all();

    state.net.abort_all();
    state.http.shutdown_connections();

    state.kernel.exit_delete_all_threads_and_wait();

    state.motion.stop_sensor_sampling();

    state.gxm.deinit();

    state.audio.deinit();

    state.renderer->preclose_action();
    renderer::stop_render_thread(*state.renderer);

    state.display.deinit();

    state.netctl.deinit();

    ngs::deinit(state.ngs, state.mem);

    // trophy (maybe namespace this?)
    deinit(state.np);

    state.http.deinit();

    state.net.deinit();

    io_deinit(state.io);

    state.camera.deinit();

    state.common_dialog.deinit();

    state.ime.deinit();

    state.sfo_handle.header = {};
    state.sfo_handle.entries.clear();

    state.license_content_id.clear();
    state.license_title_id.clear();

    state.app_info = {};

    state.regmgr.system_dreg.clear();
    state.regmgr.system_dreg_path.clear();

    state.kernel.deinit(state.mem);

    state.renderer->cleanup();

    deinit_mem(state.mem);

    state.app_path.clear();
    state.current_app_title.clear();
    state.self_name.clear();
    state.self_path.clear();
    state.main_thread_id = 0;
    state.frame_count = 0;
    state.fps = 0;
    state.avg_fps = 0;
    state.min_fps = 0;
    state.max_fps = 0;
    std::fill_n(state.fps_values, 20, 0.0f);
    state.current_fps_offset = 0;
    state.ms_per_frame = 0;
    state.drop_inputs = false;
    state.missing_nids.clear();
    state.load_exec = false;
    state.load_app_path.clear();
    state.load_exec_argv.clear();
    state.load_exec_path.clear();

    std::fill_n(state.ctrl.last_vcount, 5, 0);
    state.ctrl.input_mode = SCE_CTRL_MODE_DIGITAL;
    state.ctrl.input_mode_ext = SCE_CTRL_MODE_DIGITAL;

#if USE_DISCORD
    if (state.cfg.discord_rich_presence)
        discordrpc::update_presence();
#endif

    // re-init
    init_libraries(state);

    if (!init(state.io, state.cache_path, state.log_path, state.pref_path, state.cfg.console)) {
        LOG_ERROR("Failed to re-initialize file system after deinit!");
    }
}

bool late_init(EmuEnvState &state) {
    // note: mem is not initialized yet but that's not an issue
    // the renderer is not using it yet, just storing it for later uses
    state.renderer->late_init(state.cfg, state.app_path, state.mem);

    const bool need_page_table = state.renderer->mapping_method == MappingMethod::PageTable || state.renderer->mapping_method == MappingMethod::NativeBuffer;
    if (!init(state.mem, need_page_table)) {
        LOG_ERROR("Failed to initialize memory for emulator state!");
        return false;
    }

    if (!state.audio.init(state.cfg.current_config.audio_backend)) {
        LOG_WARN("Failed to initialize audio! Audio will not work.");
    }

    if (!ngs::init(state.ngs, state.mem)) {
        LOG_ERROR("Failed to initialize ngs.");
        return false;
    }

    return true;
}

void apply_renderer_config(EmuEnvState &emuenv) {
    auto &r = *emuenv.renderer;
    const auto &cc = emuenv.cfg.current_config;

    r.res_multiplier = cc.resolution_multiplier;
    r.set_vsync_state(cc.v_sync);
    r.set_surface_sync_state(cc.disable_surface_sync);
    r.set_screen_filter(cc.screen_filter);
    r.set_anisotropic_filtering(cc.anisotropic_filtering);
    r.set_stretch_display(cc.stretch_the_display_area);
    r.stretch_hd_pixel_perfect(cc.fullscreen_hd_res_pixel_perfect);
    r.set_async_compilation(cc.async_pipeline_compilation);
#ifdef __ANDROID__
    if (r.support_custom_drivers())
        r.set_turbo_mode(emuenv.cfg.turbo_mode);
#endif
    emuenv.display.fps_hack = cc.fps_hack;

    if (!emuenv.overlay_manager)
        emuenv.overlay_manager = std::make_unique<overlay::display_manager>();
    r.overlay_manager = emuenv.overlay_manager.get();
    emuenv.overlay_manager->set_flip_request_callback([&r]() {
        r.async_flip_requested.store(true, std::memory_order_relaxed);
        r.command_buffer_queue.wake();
    });
    r.common_dialog = &emuenv.common_dialog;
    r.sys_date_format = cc.sys_date_format;
    r.sys_lang = cc.sys_lang;

    r.show_compile_shaders = emuenv.cfg.show_compile_shaders;
    r.performance_overlay = emuenv.cfg.performance_overlay;
    r.performance_overlay_position = emuenv.cfg.performance_overlay_position;
    r.performance_overlay_detail = emuenv.cfg.performance_overlay_detail;

    // overlay input callbacks
    emuenv.overlay_manager->set_input_callbacks(
        [&emuenv]() -> overlay::button_states {
            return poll_overlay_input(emuenv);
        },
        [&emuenv](bool intercepted) {
            auto &ctrl = emuenv.ctrl;
            if (intercepted) {
                ctrl.overlay_input_intercepted.store(true, std::memory_order_relaxed);
            } else {
                ctrl.overlay_input_intercepted.store(false, std::memory_order_relaxed);
                std::lock_guard<std::mutex> lock(ctrl.mutex);
                ctrl.ignore_input = true;
            }
        },
        [&emuenv]() -> overlay::overlay_touch_state {
            auto &mouse = emuenv.ctrl.overlay_mouse;
            overlay::overlay_touch_state state;
            state.x = mouse.x.load(std::memory_order_relaxed);
            state.y = mouse.y.load(std::memory_order_relaxed);
            state.pressed = mouse.pressed.load(std::memory_order_relaxed);
            return state;
        });

    emuenv.np.trophy_state.add_trophy_unlock_callback([&emuenv](NpTrophyUnlockCallbackData &data) {
        if (!emuenv.overlay_manager)
            return;
        auto notif = emuenv.overlay_manager->get<overlay::trophy_notification>();
        if (!notif)
            notif = emuenv.overlay_manager->create<overlay::trophy_notification>();

        std::vector<uint8_t> grade_icon_buf;
        const char *grade_name = nullptr;
        switch (data.trophy_kind) {
        case np::trophy::SceNpTrophyGrade::SCE_NP_TROPHY_GRADE_PLATINUM: grade_name = "platinum"; break;
        case np::trophy::SceNpTrophyGrade::SCE_NP_TROPHY_GRADE_GOLD: grade_name = "gold"; break;
        case np::trophy::SceNpTrophyGrade::SCE_NP_TROPHY_GRADE_SILVER: grade_name = "silver"; break;
        case np::trophy::SceNpTrophyGrade::SCE_NP_TROPHY_GRADE_BRONZE: grade_name = "bronze"; break;
        default: break;
        }
        if (grade_name) {
            const auto icon_path = emuenv.static_assets_path / "icons" / (std::string(grade_name) + ".png");
            std::ifstream file(icon_path.string(), std::ios::binary | std::ios::ate);
            if (file) {
                const auto size = file.tellg();
                file.seekg(0);
                grade_icon_buf.resize(static_cast<size_t>(size));
                file.read(reinterpret_cast<char *>(grade_icon_buf.data()), size);
            }
        }

        notif->enqueue(data.trophy_name, data.icon_buf, grade_icon_buf);
    });
}

void destroy(EmuEnvState &emuenv) {
    // need to remove this function completely, call server close on application shutdown, on game close, restart server instead
    if (emuenv.cfg.gdbstub)
        server_close(emuenv);
}

void switch_state(EmuEnvState &emuenv, const bool pause) {
    if (pause) {
        emuenv.kernel.pause_threads();
    } else {
        emuenv.kernel.resume_threads();
    }
    emuenv.audio.switch_state(pause);
}

} // namespace app
