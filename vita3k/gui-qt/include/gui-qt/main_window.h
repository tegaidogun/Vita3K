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
#include <emuenv/state.h>

#include <QMainWindow>
#include <QTimer>

#include <memory>

class GuiSettings;
class PersistentSettings;

namespace Ui {
class MainWindow;
}

class AppsList;
class LogWidget;
class GameWindow;
class GameCompatibility;
class QWindow;
class QLineEdit;
class QPushButton;
class QSlider;
class QToolBar;
class TrophyCollectionDialog;
class CtrlKeyboardFilter;
class DebugWidget;
class LiveAreaWidget;
class UserManagementDialog;

class MainWindow final : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(EmuEnvState &emuenv,
        std::shared_ptr<GuiSettings> gui_settings,
        std::shared_ptr<PersistentSettings> persistent_settings);
    ~MainWindow();

    LogWidget *log_widget() const { return m_log_widget; }

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

    void changeEvent(QEvent *event) override;
    void closeEvent(QCloseEvent *event) override;
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dropEvent(QDropEvent *event) override;

private slots:
    void on_about_action_triggered();
    void on_controls_action_triggered();
    void on_app_selected(const app::AppEntry &app);
    void open_settings(int tab_index = 0);
    void apply_ui_language(const QString &locale_tag);
    void on_game_closed();
    void pump_sdl_events();
    void open_trophy_collection();
    void open_user_management();
    void show_live_area(const std::string &title_id);
    void on_live_area_play();
    void on_live_area_closed();

    void on_install_firmware_triggered();
    void on_install_pkg_triggered();
    void on_install_zip_triggered();
    void on_install_license_triggered();

    void on_install_finished();

    void on_pause_triggered();
    void on_stop_triggered();
    void on_ps_button();
    void on_toolbar_refresh();
    void on_toolbar_fullscreen();
    void on_toolbar_start();
    void on_app_selection_changed(const app::AppEntry *app);
    void on_context_menu_requested(const QPoint &global_pos, const std::vector<const app::AppEntry *> &apps);
    void resize_icons(int index);

private:
    void refresh_emulation_actions();
    void initialize();
    void init_current_user();
    void boot_game(const std::string &title_id);
    static QImage load_app_background(const std::string &app_path,
        const std::string &pref_path);

    void handle_drop(const QString &path);

    void init_discord();
    void sync_discord_presence();
    void shutdown_discord();

    void setup_toolbar();
    void repaint_toolbar_icons();
    void setup_status_bar();
    void show_title_bars(bool show) const;
    void update_renderer_button();
    void update_accuracy_button();
    void update_screen_filter_button();
    void update_audio_backend_button();
    void update_ngs_button();
    void update_volume_button();
    void refresh_status_bar();
    void save_config();
    bool confirm_missing_firmware_warning();
    void save_window_state();
    void restore_window_state();
    void open_debug_widget(int tab);
    void repaint_gui();
    void init_default_style();
    void init_first_run_stylesheet();
    void apply_stylesheet();

    EmuEnvState &emuenv;
    std::unique_ptr<Ui::MainWindow> m_ui;
    std::shared_ptr<GuiSettings> m_gui_settings;
    std::shared_ptr<PersistentSettings> m_persistent_settings;

    QMainWindow *m_dock_host = nullptr;
    AppsList *m_apps_list_widget = nullptr;
    LogWidget *m_log_widget = nullptr;
    GameWindow *m_game_window = nullptr;
    GameCompatibility *m_game_compat = nullptr;
    QWindow *m_game_container = nullptr;
    QTimer *m_sdl_pump_timer = nullptr;

    TrophyCollectionDialog *m_trophy_dialog = nullptr;
    UserManagementDialog *m_user_mgmt_dialog = nullptr;
    CtrlKeyboardFilter *m_kb_filter = nullptr;
    DebugWidget *m_debug_widget = nullptr;
    LiveAreaWidget *m_live_area_widget = nullptr;
    std::string m_live_area_title_id;

    QLineEdit *m_search_bar = nullptr;
    QSlider *m_icon_size_slider = nullptr;
    QWidget *m_slider_container = nullptr;
    bool m_fullscreen = false;
    bool m_app_selected = false;
    QString m_default_style;
#if USE_DISCORD
    bool m_discord_rich_presence_old = false;
#endif

    QIcon m_icon_play;
    QIcon m_icon_pause;
    QIcon m_icon_stop;
    QIcon m_icon_fullscreen_on;
    QIcon m_icon_fullscreen_off;

    QPushButton *m_renderer_button = nullptr;
    QPushButton *m_accuracy_button = nullptr;
    QPushButton *m_screen_filter_button = nullptr;
    QPushButton *m_audio_backend_button = nullptr;
    QPushButton *m_ngs_button = nullptr;
    QPushButton *m_volume_button = nullptr;
};
