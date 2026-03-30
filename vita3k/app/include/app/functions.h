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

#pragma once

#include <app/state.h>
#include <util/fs.h>

#include <chrono>
#include <cstdint>
#include <map>
#include <string>
#include <vector>

struct Config;
struct EmuEnvState;
class Root;

namespace app {

struct LaunchRuntimeMetrics {
    bool tracking_started = false;
    std::chrono::steady_clock::time_point last_fps_time{};
};

struct FirmwareState {
    bool pd0 = false;
    bool vs0 = false;
    bool sa0 = false;
};

struct AdhocAddressOption {
    std::string label;
    std::string subnet_mask;
};

/// Describes the state of the application to be run
enum class AppRunType {
    /// Run type is unknown
    Unknown,
    /// Extracted, files are as they are on console
    Extracted,
};

void init_paths(Root &root_paths);
bool init(EmuEnvState &state, Config &cfg, const Root &root_paths);
void deinit(EmuEnvState &state);
bool late_init(EmuEnvState &state);
void apply_renderer_config(EmuEnvState &emuenv);
void set_current_config(EmuEnvState &emuenv, const std::string &app_path);
void destroy(EmuEnvState &emuenv);
void switch_state(EmuEnvState &emuenv, const bool pause);

bool init_apps_list(EmuEnvState &emuenv);
bool scan_apps(EmuEnvState &emuenv);
bool load_cached_apps(EmuEnvState &emuenv);
void save_apps_cache(EmuEnvState &emuenv);
AppEntry read_app_info(EmuEnvState &emuenv, const std::string &title_id);
void load_app_times(EmuEnvState &emuenv);
void save_app_times(EmuEnvState &emuenv);
void update_last_time_app_used(EmuEnvState &emuenv, const std::string &app_path);
void update_app_time_used(EmuEnvState &emuenv, const std::string &app_path);
void reset_last_time_app_used(EmuEnvState &emuenv, const std::string &app_path);
void delete_app(EmuEnvState &emuenv, const std::string &app_path);
std::vector<AppEntry> get_apps(const EmuEnvState &emuenv);
std::map<std::string, AppTime> get_user_app_times(const EmuEnvState &emuenv);
int get_supported_memory_mapping_mask(const EmuEnvState &emuenv, int gpu_idx = -1);
void ensure_camera_defaults(Config &cfg);
std::vector<std::string> get_available_camera_names();
std::vector<AdhocAddressOption> get_available_adhoc_address_options();

bool set_app_info(EmuEnvState &emuenv, const std::string &app_path);
void reset_controller_binding(EmuEnvState &emuenv);
FirmwareState get_firmware_state(const EmuEnvState &emuenv);
bool has_firmware_installed(const EmuEnvState &emuenv);
bool ensure_current_user(EmuEnvState &emuenv);
bool switch_emulator_path(EmuEnvState &emuenv, const fs::path &pref_path);
bool setup_game_launch(EmuEnvState &emuenv, const std::string &app_path);
void prepare_game_launch_overlay(EmuEnvState &emuenv);
void update_runtime_metrics(EmuEnvState &emuenv, LaunchRuntimeMetrics &metrics);
void abort_game_launch(EmuEnvState &emuenv);
void finish_game_launch(EmuEnvState &emuenv);

void load_users(EmuEnvState &emuenv);
void save_user(EmuEnvState &emuenv, const std::string &user_id);
std::string create_user(EmuEnvState &emuenv, const std::string &name);
void delete_user(EmuEnvState &emuenv, const std::string &user_id);
bool activate_user(EmuEnvState &emuenv, const std::string &user_id);

} // namespace app
