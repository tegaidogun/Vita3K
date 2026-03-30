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
#include <app/state.h>
#include <config/state.h>
#include <emuenv/state.h>
#include <io/state.h>
#include <packages/sfo.h>
#include <util/fs.h>
#include <util/log.h>

#include <pugixml.hpp>

#include <algorithm>
#include <array>
#include <cstdint>
#include <ctime>
#include <fstream>
#include <mutex>
#include <vector>

static constexpr uint32_t CACHE_VERSION = 1;

namespace app {

namespace {

static std::string read_str(std::ifstream &s) {
    std::size_t len{};
    s.read(reinterpret_cast<char *>(&len), sizeof(len));
    std::vector<char> buf(len);
    s.read(buf.data(), static_cast<std::streamsize>(len));
    return std::string(buf.begin(), buf.end());
}

static void write_str(std::ofstream &s, const std::string &v) {
    const std::size_t len = v.size();
    s.write(reinterpret_cast<const char *>(&len), sizeof(len));
    s.write(v.c_str(), static_cast<std::streamsize>(len));
}

static void save_apps_cache(std::ofstream &f, const AppsListState &state, uint32_t sys_lang) {
    const std::size_t count = state.apps.size();
    f.write(reinterpret_cast<const char *>(&count), sizeof(count));
    f.write(reinterpret_cast<const char *>(&CACHE_VERSION), sizeof(CACHE_VERSION));
    f.write(reinterpret_cast<const char *>(&sys_lang), sizeof(sys_lang));

    for (const AppEntry &app : state.apps) {
        write_str(f, app.app_ver);
        write_str(f, app.category);
        write_str(f, app.content_id);
        write_str(f, app.addcont);
        write_str(f, app.savedata);
        write_str(f, app.parental_level);
        write_str(f, app.stitle);
        write_str(f, app.title);
        write_str(f, app.title_id);
        write_str(f, app.path);
        write_str(f, app.icon_path);
    }
}

static std::string resolve_existing_path(const EmuEnvState &emuenv, const fs::path &path) {
    if (path.empty())
        return {};

    if (path.is_absolute()) {
        if (fs::exists(path))
            return path.generic_string();
        return {};
    }

    const std::array<fs::path, 7> roots = {
        emuenv.pref_path,
        emuenv.default_path,
        emuenv.shared_path,
        emuenv.static_assets_path,
        emuenv.base_path,
        emuenv.config_path,
        emuenv.cache_path,
    };

    for (const auto &root : roots) {
        if (root.empty())
            continue;

        const fs::path candidate = root / path;
        if (fs::exists(candidate))
            return candidate.generic_string();
    }

    return {};
}

static std::string get_icon_path(const EmuEnvState &emuenv, const std::string &title_id) {
    const auto rel = fs::path("ux0/app") / title_id / "sce_sys/icon0.png";
    if (const auto resolved = resolve_existing_path(emuenv, rel); !resolved.empty())
        return resolved;

    const auto default_rel = fs::path("vs0/data/internal/common/default-icon.png");
    if (const auto resolved = resolve_existing_path(emuenv, default_rel); !resolved.empty())
        return resolved;

    return {};
}

static std::vector<AppTime>::iterator find_app_time(
    AppsListState &state,
    const std::string &user_id,
    const std::string &app_path) {
    auto &v = state.app_times[user_id];
    return std::find_if(v.begin(), v.end(), [&](const AppTime &t) {
        return t.app_path == app_path;
    });
}

} // namespace

bool init_apps_list(EmuEnvState &emuenv) {
    if (!load_cached_apps(emuenv))
        return scan_apps(emuenv);
    return true;
}

bool scan_apps(EmuEnvState &emuenv) {
    const fs::path app_path{ emuenv.pref_path / "ux0/app" };
    if (!fs::exists(app_path)) {
        return false;
    }

    std::vector<AppEntry> scanned;
    for (const auto &entry : fs::directory_iterator(app_path)) {
        if (entry.path().empty() || !fs::is_directory(entry.path()) || entry.path().filename_is_dot() || entry.path().filename_is_dot_dot())
            continue;
        scanned.push_back(read_app_info(emuenv, entry.path().stem().generic_string()));
    }

    auto &state = emuenv.app.apps_list;
    const uint32_t sys_lang = static_cast<uint32_t>(emuenv.cfg.sys_lang);

    const auto temp_path{ emuenv.pref_path / "ux0/temp" };
    fs::create_directories(temp_path);
    std::ofstream f((temp_path / "apps.dat").string(), std::ios::out | std::ios::binary);
    if (!f.is_open())
        LOG_ERROR("Failed to open apps list cache for writing");

    {
        std::lock_guard<std::mutex> lock(state.mutex);
        state.apps = std::move(scanned);
        if (f)
            save_apps_cache(f, state, sys_lang);
    }

    if (f && !f)
        LOG_ERROR("Error writing apps list cache");

    return true;
}

bool load_cached_apps(EmuEnvState &emuenv) {
    const auto cache_path{ emuenv.pref_path / "ux0/temp/apps.dat" };

    std::ifstream f(cache_path.string(), std::ios::in | std::ios::binary);
    if (!f.is_open())
        return false;

    std::size_t count{};
    f.read(reinterpret_cast<char *>(&count), sizeof(count));

    uint32_t version{};
    f.read(reinterpret_cast<char *>(&version), sizeof(version));
    if (version != CACHE_VERSION) {
        LOG_WARN("Apps cache version mismatch (found {}, expected {}); rebuilding.", version, CACHE_VERSION);
        return false;
    }

    uint32_t cache_lang{};
    f.read(reinterpret_cast<char *>(&cache_lang), sizeof(cache_lang));
    if (cache_lang != static_cast<uint32_t>(emuenv.cfg.sys_lang)) {
        LOG_WARN("Apps cache lang {} differs from config {}; rebuilding.",
            cache_lang, static_cast<uint32_t>(emuenv.cfg.sys_lang));
        return false;
    }

    std::vector<AppEntry> loaded;
    loaded.reserve(count);
    for (std::size_t i = 0; i < count; ++i) {
        AppEntry app;
        app.app_ver = read_str(f);
        app.category = read_str(f);
        app.content_id = read_str(f);
        app.addcont = read_str(f);
        app.savedata = read_str(f);
        app.parental_level = read_str(f);
        app.stitle = read_str(f);
        app.title = read_str(f);
        app.title_id = read_str(f);
        app.path = read_str(f);
        app.icon_path = read_str(f);
        if (!app.icon_path.empty()) {
            const auto resolved = resolve_existing_path(emuenv, app.icon_path);
            if (!resolved.empty())
                app.icon_path = resolved;
        }
        loaded.push_back(std::move(app));
    }

    if (!f) {
        LOG_ERROR("Apps cache read failed or truncated; rebuilding.");
        return false;
    }

    auto &state = emuenv.app.apps_list;
    std::lock_guard<std::mutex> lock(state.mutex);
    state.apps = std::move(loaded);

    return !state.apps.empty();
}

void save_apps_cache(EmuEnvState &emuenv) {
    const auto temp_path{ emuenv.pref_path / "ux0/temp" };
    fs::create_directories(temp_path);

    std::ofstream f((temp_path / "apps.dat").string(), std::ios::out | std::ios::binary);
    if (!f.is_open()) {
        LOG_ERROR("Failed to open {}", (temp_path / "apps.dat").string());
        return;
    }

    {
        auto &state = emuenv.app.apps_list;
        std::lock_guard<std::mutex> lock(state.mutex);
        save_apps_cache(f, state, static_cast<uint32_t>(emuenv.cfg.sys_lang));
    }

    if (!f)
        LOG_ERROR("Write error during apps list cache save.");
}

AppEntry read_app_info(EmuEnvState &emuenv, const std::string &title_id) {
    sfo::SfoAppInfo info;
    vfs::FileBuffer param;
    if (vfs::read_app_file(param, emuenv.pref_path, title_id, "sce_sys/param.sfo")) {
        sfo::get_param_info(info, param, emuenv.cfg.sys_lang);
    } else {
        info.app_title_id = title_id;
        info.app_addcont = title_id;
        info.app_savedata = title_id;
        info.app_short_title = title_id;
        info.app_title = title_id;
        info.app_version = "N/A";
        info.app_category = "N/A";
        info.app_parental_level = "N/A";
    }

    AppEntry app;
    app.app_ver = info.app_version;
    app.category = info.app_category;
    app.content_id = info.app_content_id;
    app.addcont = info.app_addcont;
    app.savedata = info.app_savedata;
    app.parental_level = info.app_parental_level;
    app.stitle = info.app_short_title;
    app.title = info.app_title;
    app.title_id = info.app_title_id;
    app.path = title_id;
    app.icon_path = get_icon_path(emuenv, title_id);
    return app;
}

void load_app_times(EmuEnvState &emuenv) {
    auto &state = emuenv.app.apps_list;
    std::lock_guard<std::mutex> lock(state.mutex);

    state.app_times.clear();
    const auto time_path{ emuenv.pref_path / "ux0/user/time.xml" };
    if (!fs::exists(time_path))
        return;

    pugi::xml_document doc;
    if (!doc.load_file(time_path.c_str())) {
        LOG_ERROR("time.xml is corrupted at {}; removing.", time_path.string());
        fs::remove(time_path);
        return;
    }

    const auto time_child = doc.child("time");
    if (time_child.child("user").empty())
        return;

    for (const auto &user : time_child) {
        const std::string user_id = user.attribute("id").as_string();
        for (const auto &app : user) {
            AppTime t;
            t.app_path = app.text().as_string();
            t.last_time_used = app.attribute("last-time-used").as_llong();
            t.time_used = app.attribute("time-used").as_llong();
            state.app_times[user_id].push_back(t);
        }
    }
}

void save_app_times(EmuEnvState &emuenv) {
    auto &state = emuenv.app.apps_list;

    pugi::xml_document doc;
    auto decl = doc.append_child(pugi::node_declaration);
    decl.append_attribute("version") = "1.0";
    decl.append_attribute("encoding") = "utf-8";

    auto time_child = doc.append_child("time");

    for (auto &[user_id, app_times] : state.app_times) {
        std::sort(app_times.begin(), app_times.end(), [](const AppTime &a, const AppTime &b) {
            return a.last_time_used > b.last_time_used;
        });

        auto user_child = time_child.append_child("user");
        user_child.append_attribute("id") = user_id.c_str();

        for (const auto &t : app_times) {
            auto app_child = user_child.append_child("app");
            app_child.append_attribute("last-time-used") = t.last_time_used;
            app_child.append_attribute("time-used") = t.time_used;
            app_child.append_child(pugi::node_pcdata).set_value(t.app_path.c_str());
        }
    }

    const auto time_path{ emuenv.pref_path / "ux0/user/time.xml" };
    if (!doc.save_file(time_path.c_str()))
        LOG_ERROR("Failed to write {}", time_path.string());
}

void update_last_time_app_used(EmuEnvState &emuenv, const std::string &app_path) {
    auto &state = emuenv.app.apps_list;
    std::lock_guard<std::mutex> lock(state.mutex);

    const std::string &user_id = emuenv.io.user_id;
    auto it = find_app_time(state, user_id, app_path);

    if (it != state.app_times[user_id].end()) {
        it->last_time_used = std::time(nullptr);
    } else {
        state.app_times[user_id].push_back({ app_path, std::time(nullptr), 0 });
    }

    save_app_times(emuenv);
}

void update_app_time_used(EmuEnvState &emuenv, const std::string &app_path) {
    auto &state = emuenv.app.apps_list;
    std::lock_guard<std::mutex> lock(state.mutex);

    const std::string &user_id = emuenv.io.user_id;
    auto it = find_app_time(state, user_id, app_path);

    if (it == state.app_times[user_id].end()) {
        LOG_WARN("No time record for '{}'; skipping.", app_path);
        return;
    }

    const auto now = std::time(nullptr);
    it->time_used += now - it->last_time_used;
    it->last_time_used = now;

    save_app_times(emuenv);
}

void reset_last_time_app_used(EmuEnvState &emuenv, const std::string &app_path) {
    auto &state = emuenv.app.apps_list;
    std::lock_guard<std::mutex> lock(state.mutex);

    const std::string &user_id = emuenv.io.user_id;
    auto it = find_app_time(state, user_id, app_path);

    if (it != state.app_times[user_id].end())
        it->last_time_used = 0;
    else
        state.app_times[user_id].push_back({ app_path, 0, 0 });

    save_app_times(emuenv);
}

void delete_app(EmuEnvState &emuenv, const std::string &app_path) {
    AppEntry app_entry;
    {
        auto &state = emuenv.app.apps_list;
        std::lock_guard<std::mutex> lock(state.mutex);
        const auto it = std::find_if(state.apps.begin(), state.apps.end(),
            [&](const AppEntry &app) { return app.path == app_path; });
        if (it == state.apps.end()) {
            LOG_WARN("'{}' not found in apps list.", app_path);
            return;
        }
        app_entry = *it;
    }

    try {
        fs::remove_all(emuenv.pref_path / "ux0/app" / app_path);

        const auto custom_cfg{ emuenv.config_path / "config" / fmt::format("config_{}.xml", app_path) };
        if (fs::exists(custom_cfg))
            fs::remove_all(custom_cfg);

        const auto addcont{ emuenv.pref_path / "ux0/addcont" / app_entry.addcont };
        if (fs::exists(addcont))
            fs::remove_all(addcont);

        const auto license{ emuenv.pref_path / "ux0/license" / app_entry.title_id };
        if (fs::exists(license))
            fs::remove_all(license);

        const auto patch{ emuenv.pref_path / "ux0/patch" / app_entry.title_id };
        if (fs::exists(patch))
            fs::remove_all(patch);

        const auto savedata{ emuenv.pref_path / "ux0/user" / emuenv.io.user_id / "savedata" / app_entry.savedata };
        if (fs::exists(savedata))
            fs::remove_all(savedata);

        const auto shader_cache{ emuenv.cache_path / "shaders" / app_entry.title_id };
        if (fs::exists(shader_cache))
            fs::remove_all(shader_cache);

        const auto shader_log{ emuenv.cache_path / "shaderlog" / app_entry.title_id };
        if (fs::exists(shader_log))
            fs::remove_all(shader_log);

        const auto shader_log2{ emuenv.log_path / "shaderlog" / app_entry.title_id };
        if (fs::exists(shader_log2))
            fs::remove_all(shader_log2);

        const auto export_tex{ emuenv.shared_path / "textures/export" / app_entry.title_id };
        if (fs::exists(export_tex))
            fs::remove_all(export_tex);

        const auto import_tex{ emuenv.shared_path / "textures/import" / app_entry.title_id };
        if (fs::exists(import_tex))
            fs::remove_all(import_tex);

        LOG_INFO("App successfully deleted '{}' [{}].", app_entry.title_id, app_entry.title);
    } catch (const std::exception &e) {
        LOG_ERROR("Failed to delete '{}' [{}]: {}", app_entry.title_id, app_entry.title, e.what());
    }

    {
        auto &state = emuenv.app.apps_list;
        std::lock_guard<std::mutex> lock(state.mutex);

        state.apps.erase(
            std::remove_if(state.apps.begin(), state.apps.end(),
                [&](const AppEntry &app) { return app.path == app_path; }),
            state.apps.end());

        for (auto &[user_id, times] : state.app_times) {
            times.erase(
                std::remove_if(times.begin(), times.end(),
                    [&](const AppTime &t) { return t.app_path == app_path; }),
                times.end());
        }

        save_app_times(emuenv);

        const auto temp_path{ emuenv.pref_path / "ux0/temp" };
        fs::create_directories(temp_path);
        std::ofstream f((temp_path / "apps.dat").string(), std::ios::out | std::ios::binary);
        if (f.is_open())
            save_apps_cache(f, state, static_cast<uint32_t>(emuenv.cfg.sys_lang));
        else
            LOG_ERROR("Failed to rewrite apps cache");
    }
}

std::vector<AppEntry> get_apps(const EmuEnvState &emuenv) {
    const auto &state = emuenv.app.apps_list;
    std::lock_guard<std::mutex> lock(state.mutex);
    return state.apps;
}

std::map<std::string, AppTime> get_user_app_times(const EmuEnvState &emuenv) {
    const auto &state = emuenv.app.apps_list;
    std::lock_guard<std::mutex> lock(state.mutex);
    std::map<std::string, AppTime> result;
    const auto it = state.app_times.find(emuenv.io.user_id);
    if (it != state.app_times.end()) {
        for (const auto &t : it->second)
            result[t.app_path] = t;
    }
    return result;
}

bool set_app_info(EmuEnvState &emuenv, const std::string &app_path) {
    const auto &state = emuenv.app.apps_list;
    std::lock_guard<std::mutex> lock(state.mutex);

    const auto it = std::find_if(state.apps.begin(), state.apps.end(), [&](const AppEntry &app) { return app.path == app_path; });

    if (it == state.apps.end()) {
        LOG_ERROR("{} not found in apps list.", app_path);
        return false;
    }

    emuenv.io.app_path = it->path;
    emuenv.io.title_id = it->title_id;
    emuenv.io.addcont = it->addcont;
    emuenv.io.content_id = it->content_id;
    emuenv.io.savedata = it->savedata;
    emuenv.current_app_title = it->title;
    emuenv.app_info.app_version = it->app_ver;
    emuenv.app_info.app_category = it->category;
    emuenv.app_info.app_short_title = it->stitle;

    return true;
}

} // namespace app
