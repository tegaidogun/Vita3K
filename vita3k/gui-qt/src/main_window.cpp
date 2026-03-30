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

#include "ui_main_window.h"

#include <gui-qt/about_dialog.h>
#include <gui-qt/apps_list.h>
#include <gui-qt/apps_list_context_menu.h>
#include <gui-qt/archive_install_dialog.h>
#include <gui-qt/controls_dialog.h>
#include <gui-qt/ctrl_keyboard_filter.h>
#include <gui-qt/debug_widget.h>
#include <gui-qt/firmware_install_dialog.h>
#include <gui-qt/game_compatibility.h>
#include <gui-qt/game_window.h>
#include <gui-qt/gui_language.h>
#include <gui-qt/gui_settings.h>
#include <gui-qt/ime_keyboard_filter.h>
#include <gui-qt/license_install_dialog.h>
#include <gui-qt/live_area_widget.h>
#include <gui-qt/log_widget.h>
#include <gui-qt/main_window.h>
#include <gui-qt/persistent_settings.h>
#include <gui-qt/pkg_install_dialog.h>
#include <gui-qt/qt_utils.h>
#include <gui-qt/settings_dialog.h>
#include <gui-qt/stylesheets.h>
#include <gui-qt/trophy_collection_dialog.h>
#include <gui-qt/user_management_dialog.h>
#include <gui-qt/welcome_dialog.h>

#include <app/functions.h>
#include <archive.h>
#include <audio/state.h>
#include <config/functions.h>
#include <config/settings.h>
#include <config/state.h>
#include <config/version.h>
#include <ctrl/functions.h>
#include <display/state.h>
#include <gxm/state.h>
#include <interface.h>
#include <io/state.h>
#include <kernel/state.h>
#include <motion/event_handler.h>
#include <np/state.h>
#include <packages/functions.h>
#include <packages/license.h>
#include <renderer/functions.h>
#include <renderer/shaders.h>
#include <renderer/state.h>
#include <touch/functions.h>
#include <util/fs.h>
#include <util/log.h>
#include <util/string_utils.h>

#if USE_DISCORD
#include <app/discord.h>
#endif

#include <SDL3/SDL_events.h>
#include <SDL3/SDL_gamepad.h>
#include <SDL3/SDL_sensor.h>

#include <QApplication>
#include <QCheckBox>
#include <QCloseEvent>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QMenu>
#include <QMessageBox>
#include <QMimeData>
#include <QOpenGLContext>
#include <QPalette>
#include <QProgressDialog>
#include <QPushButton>
#include <QSlider>
#include <QStandardPaths>
#include <QStatusBar>
#include <QStyleFactory>
#include <QStyleHints>
#include <QToolBar>
#include <QWidgetAction>
#include <QtResource>

#if defined(HAVE_X11) || defined(HAVE_WAYLAND)
#include <qpa/qplatformnativeinterface.h>
#endif

namespace {

QPalette make_dark_palette() {
    QPalette palette;
    palette.setColor(QPalette::Window, QColor(45, 45, 48));
    palette.setColor(QPalette::WindowText, QColor(243, 243, 243));
    palette.setColor(QPalette::Base, QColor(30, 30, 30));
    palette.setColor(QPalette::AlternateBase, QColor(38, 38, 41));
    palette.setColor(QPalette::ToolTipBase, QColor(45, 45, 48));
    palette.setColor(QPalette::ToolTipText, QColor(243, 243, 243));
    palette.setColor(QPalette::Text, QColor(243, 243, 243));
    palette.setColor(QPalette::Button, QColor(45, 45, 48));
    palette.setColor(QPalette::ButtonText, QColor(243, 243, 243));
    palette.setColor(QPalette::BrightText, Qt::white);
    palette.setColor(QPalette::Link, QColor(66, 156, 255));
    palette.setColor(QPalette::Highlight, QColor(47, 125, 209));
    palette.setColor(QPalette::HighlightedText, Qt::white);
    palette.setColor(QPalette::PlaceholderText, QColor(160, 160, 160));
    palette.setColor(QPalette::Disabled, QPalette::Text, QColor(115, 115, 115));
    palette.setColor(QPalette::Disabled, QPalette::ButtonText, QColor(115, 115, 115));
    palette.setColor(QPalette::Disabled, QPalette::WindowText, QColor(115, 115, 115));
    return palette;
}

void update_status_button_accent(QPushButton *button, const QString &accent) {
    if (!button)
        return;

    if (button->objectName() != QStringLiteral("status_button"))
        button->setObjectName(QStringLiteral("status_button"));

    if (button->property("accent").toString() != accent) {
        button->setProperty("accent", accent);
        button->style()->unpolish(button);
        button->style()->polish(button);
    }

    button->update();
}

} // namespace

MainWindow::MainWindow(EmuEnvState &emuenv,
    std::shared_ptr<GuiSettings> gui_settings,
    std::shared_ptr<PersistentSettings> persistent_settings)
    : QMainWindow(nullptr)
    , emuenv{ emuenv }
    , m_ui(std::make_unique<Ui::MainWindow>())
    , m_gui_settings(std::move(gui_settings))
    , m_persistent_settings(std::move(persistent_settings)) {
    initialize();
}

MainWindow::~MainWindow() {
    shutdown_discord();
}

void MainWindow::changeEvent(QEvent *event) {
    if (event->type() == QEvent::LanguageChange) {
        m_ui->retranslateUi(this);
        setWindowTitle(QString::fromStdString(window_title));
        refresh_emulation_actions();
        refresh_status_bar();
        if (m_search_bar)
            m_search_bar->setPlaceholderText(tr("Search..."));
        m_ui->toolbar_fullscreen->setText(m_fullscreen ? tr("Exit Fullscreen") : tr("Fullscreen"));
    }

    QMainWindow::changeEvent(event);
}

void MainWindow::closeEvent(QCloseEvent *event) {
    if (m_game_window) {
        const int result = QMessageBox::question(this,
            tr("Exit?"),
            tr("A game is still running. Do you really want to exit?\n\nAny unsaved progress will be lost!"),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No);

        if (result != QMessageBox::Yes) {
            event->ignore();
            return;
        }
        on_game_closed();
    }

    save_window_state();
    QMainWindow::closeEvent(event);
}

void MainWindow::save_window_state() {
    if (!m_gui_settings)
        return;

    m_gui_settings->set_value(gui::mw_geometry, saveGeometry(), false);
    m_gui_settings->set_value(gui::mw_windowState, saveState(), false);
    if (m_dock_host)
        m_gui_settings->set_value(gui::mw_mwState, m_dock_host->saveState(), false);

    if (m_apps_list_widget)
        m_apps_list_widget->save_settings(*m_gui_settings);

    if (m_icon_size_slider)
        m_gui_settings->set_value(gui::gl_iconSize, m_icon_size_slider->value(), false);

    if (m_log_widget)
        m_gui_settings->set_value(gui::mw_loggerVisible, m_log_widget->isVisible(), false);
    if (m_apps_list_widget)
        m_gui_settings->set_value(gui::mw_appsListVisible, m_apps_list_widget->isVisible(), false);

    m_gui_settings->set_value(gui::mw_toolBarVisible, m_ui->show_toolbar_action->isChecked(), false);
    m_gui_settings->set_value(gui::mw_titleBarsVisible, m_ui->show_title_bars_action->isChecked(), false);

    m_gui_settings->sync();
}

void MainWindow::restore_window_state() {
    if (!m_gui_settings)
        return;

    if (!restoreGeometry(m_gui_settings->get_value(gui::mw_geometry).toByteArray()))
        resize(1280, 720);

    restoreState(m_gui_settings->get_value(gui::mw_windowState).toByteArray());

    if (m_apps_list_widget)
        m_apps_list_widget->load_settings(*m_gui_settings);

    if (m_icon_size_slider) {
        const int icon_size = m_gui_settings->get_value(gui::gl_iconSize).toInt();
        m_icon_size_slider->setValue(icon_size);
    }

    bool log_visible = m_gui_settings->get_value(gui::mw_loggerVisible).toBool();
    bool gamelist_visible = m_gui_settings->get_value(gui::mw_appsListVisible).toBool();
    if (!log_visible && !gamelist_visible) {
        log_visible = true;
        gamelist_visible = true;
        m_gui_settings->set_value(gui::mw_loggerVisible, true, false);
        m_gui_settings->set_value(gui::mw_appsListVisible, true, false);
    }

    if (m_dock_host) {
        const QByteArray dock_state = m_gui_settings->get_value(gui::mw_mwState).toByteArray();
        if (!dock_state.isEmpty() && m_apps_list_widget && m_log_widget) {
            m_dock_host->restoreState(dock_state);

            if (!m_apps_list_widget->isVisible() && !m_log_widget->isVisible()) {
                m_dock_host->addDockWidget(Qt::LeftDockWidgetArea, m_apps_list_widget);
                m_dock_host->addDockWidget(Qt::LeftDockWidgetArea, m_log_widget);
                m_dock_host->splitDockWidget(m_apps_list_widget, m_log_widget, Qt::Vertical);
                m_dock_host->resizeDocks({ m_log_widget }, { m_dock_host->sizeHint().height() / 5 }, Qt::Vertical);
            }
        }
    }

    if (m_log_widget) {
        m_log_widget->setVisible(log_visible);
        m_ui->show_log_action->setChecked(log_visible);
    }
    if (m_apps_list_widget) {
        m_apps_list_widget->setVisible(gamelist_visible);
        m_ui->show_apps_list_action->setChecked(gamelist_visible);
    }

    const bool toolbar_visible = m_gui_settings->get_value(gui::mw_toolBarVisible).toBool();
    m_ui->show_toolbar_action->setChecked(toolbar_visible);
    m_ui->toolBar->setVisible(toolbar_visible);

    const bool title_bars_visible = m_gui_settings->get_value(gui::mw_titleBarsVisible).toBool();
    m_ui->show_title_bars_action->setChecked(title_bars_visible);
    show_title_bars(title_bars_visible);
}

void MainWindow::initialize() {
    Q_INIT_RESOURCE(resources);
    m_ui->setupUi(this);

    this->resize(1280, 720);
    this->setWindowIcon(QIcon(":/Vita3K.png"));
    this->setWindowTitle(QString::fromStdString(window_title));

    emuenv.compat.log_compat_warn = emuenv.cfg.log_compat_warn;
    m_game_compat = new GameCompatibility(emuenv.compat, emuenv.cache_path.string(), this);

    emuenv.vulkan_device_info = std::make_unique<renderer::VulkanDeviceInfo>(renderer::enumerate_vulkan_devices());

    init_current_user();

    m_dock_host = new QMainWindow(this);
    m_dock_host->setWindowFlags(Qt::Widget);
    m_dock_host->setContextMenuPolicy(Qt::PreventContextMenu);
    m_dock_host->setDockNestingEnabled(true);
    setCentralWidget(m_dock_host);

    m_apps_list_widget = new AppsList(emuenv, m_dock_host);
    m_apps_list_widget->refresh();
    m_dock_host->addDockWidget(Qt::LeftDockWidgetArea, m_apps_list_widget);

    m_log_widget = new LogWidget(m_dock_host);
    m_log_widget->attach();
    m_dock_host->addDockWidget(Qt::LeftDockWidgetArea, m_log_widget);
    m_dock_host->splitDockWidget(m_apps_list_widget, m_log_widget, Qt::Vertical);
    m_dock_host->resizeDocks({ m_log_widget }, { m_dock_host->sizeHint().height() / 5 }, Qt::Vertical);

    connect(m_apps_list_widget, &AppsList::boot_requested,
        this, &MainWindow::on_app_selected);

    connect(m_apps_list_widget, &AppsList::context_menu_requested,
        this, &MainWindow::on_context_menu_requested);

    m_sdl_pump_timer = new QTimer(this);
    m_sdl_pump_timer->setInterval(4);
    connect(m_sdl_pump_timer, &QTimer::timeout, this, &MainWindow::pump_sdl_events);
    m_sdl_pump_timer->start();

    setup_toolbar();

    connect(m_ui->welcome_dialog_action, &QAction::triggered, this, [this]() {
        auto *welcome = new WelcomeDialog(emuenv, true, this);
        welcome->open();
    });

    connect(m_ui->trophy_collection_action, &QAction::triggered,
        this, &MainWindow::open_trophy_collection);

    connect(m_ui->user_management_action, &QAction::triggered,
        this, &MainWindow::open_user_management);

    connect(m_ui->settings_core_action, &QAction::triggered, this, [this] { open_settings(static_cast<int>(SettingsTab::Core)); });
    connect(m_ui->settings_cpu_action, &QAction::triggered, this, [this] { open_settings(static_cast<int>(SettingsTab::CPU)); });
    connect(m_ui->settings_gpu_action, &QAction::triggered, this, [this] { open_settings(static_cast<int>(SettingsTab::GPU)); });
    connect(m_ui->settings_audio_action, &QAction::triggered, this, [this] { open_settings(static_cast<int>(SettingsTab::Audio)); });
    connect(m_ui->settings_camera_action, &QAction::triggered, this, [this] { open_settings(static_cast<int>(SettingsTab::Camera)); });
    connect(m_ui->settings_system_action, &QAction::triggered, this, [this] { open_settings(static_cast<int>(SettingsTab::System)); });
    connect(m_ui->settings_emulator_action, &QAction::triggered, this, [this] { open_settings(static_cast<int>(SettingsTab::Emulator)); });
    connect(m_ui->settings_gui_action, &QAction::triggered, this, [this] { open_settings(static_cast<int>(SettingsTab::GUI)); });
    connect(m_ui->settings_network_action, &QAction::triggered, this, [this] { open_settings(static_cast<int>(SettingsTab::Network)); });
    connect(m_ui->settings_debug_action, &QAction::triggered, this, [this] { open_settings(static_cast<int>(SettingsTab::Debug)); });

    connect(m_ui->install_firmware_action, &QAction::triggered,
        this, &MainWindow::on_install_firmware_triggered);
    connect(m_ui->install_pkg_action, &QAction::triggered,
        this, &MainWindow::on_install_pkg_triggered);
    connect(m_ui->install_zip_action, &QAction::triggered,
        this, &MainWindow::on_install_zip_triggered);
    connect(m_ui->install_license_action, &QAction::triggered,
        this, &MainWindow::on_install_license_triggered);

    connect(m_ui->debug_threads_action, &QAction::triggered, this, [this] { open_debug_widget(DebugWidget::Threads); });
    connect(m_ui->debug_mutexes_action, &QAction::triggered, this, [this] { open_debug_widget(DebugWidget::Mutexes); });
    connect(m_ui->debug_lw_mutexes_action, &QAction::triggered, this, [this] { open_debug_widget(DebugWidget::LwMutexes); });
    connect(m_ui->debug_condvars_action, &QAction::triggered, this, [this] { open_debug_widget(DebugWidget::Condvars); });
    connect(m_ui->debug_lw_condvars_action, &QAction::triggered, this, [this] { open_debug_widget(DebugWidget::LwCondvars); });
    connect(m_ui->debug_semaphores_action, &QAction::triggered, this, [this] { open_debug_widget(DebugWidget::Semaphores); });
    connect(m_ui->debug_event_flags_action, &QAction::triggered, this, [this] { open_debug_widget(DebugWidget::EventFlags); });
    connect(m_ui->debug_allocations_action, &QAction::triggered, this, [this] { open_debug_widget(DebugWidget::Allocations); });
    connect(m_ui->debug_disassembly_action, &QAction::triggered, this, [this] { open_debug_widget(DebugWidget::Disassembly); });

    connect(m_ui->pause_action, &QAction::triggered,
        this, &MainWindow::on_toolbar_start);
    connect(m_ui->stop_action, &QAction::triggered,
        this, &MainWindow::on_stop_triggered);

    connect(m_ui->show_apps_list_action, &QAction::triggered, this, [this](bool checked) {
        m_apps_list_widget->setVisible(checked);
        m_gui_settings->set_value(gui::mw_appsListVisible, checked);
    });
    connect(m_apps_list_widget, &QDockWidget::visibilityChanged,
        m_ui->show_apps_list_action, &QAction::setChecked);

    connect(m_ui->show_log_action, &QAction::triggered, this, [this](bool checked) {
        m_log_widget->setVisible(checked);
        m_gui_settings->set_value(gui::mw_loggerVisible, checked);
    });
    connect(m_log_widget, &QDockWidget::visibilityChanged,
        m_ui->show_log_action, &QAction::setChecked);

    connect(m_ui->show_toolbar_action, &QAction::triggered, this, [this](bool checked) {
        m_ui->toolBar->setVisible(checked);
        m_gui_settings->set_value(gui::mw_toolBarVisible, checked);
    });

    connect(m_ui->show_title_bars_action, &QAction::triggered, this, [this](bool checked) {
        show_title_bars(checked);
        m_gui_settings->set_value(gui::mw_titleBarsVisible, checked);
    });

    setAcceptDrops(true);

    setup_status_bar();

    setMinimumSize(350, minimumSizeHint().height());
    restore_window_state();

    if (m_log_widget && m_gui_settings) {
        const int buf_size = m_gui_settings->get_value(gui::l_bufferSize).toInt();
        m_log_widget->set_log_buffer_size(buf_size);
    }

    init_default_style();
    init_first_run_stylesheet();
    apply_stylesheet();

    if (emuenv.cfg.show_welcome) {
        WelcomeDialog welcome(emuenv, false, nullptr);
        welcome.exec();
    }

    connect(QGuiApplication::styleHints(), &QStyleHints::colorSchemeChanged, this, [this]() {
        apply_stylesheet();
    });

    init_discord();

    if (emuenv.cfg.run_app_path.has_value()) {
        QTimer::singleShot(0, this, [this]() {
            const std::string title_id = *emuenv.cfg.run_app_path;
            emuenv.cfg.run_app_path.reset();
            boot_game(title_id);
        });
    }
}

void MainWindow::on_install_firmware_triggered() {
    auto *dlg = new FirmwareInstallDialog(emuenv, this);
    connect(dlg, &FirmwareInstallDialog::install_complete,
        this, &MainWindow::on_install_finished);
    connect(dlg, &QDialog::finished, dlg, &QObject::deleteLater);
    dlg->exec();
}

void MainWindow::on_install_pkg_triggered() {
    auto *dlg = new PkgInstallDialog(emuenv, this);
    connect(dlg, &PkgInstallDialog::install_complete,
        this, [this](const QString &) { on_install_finished(); });
    connect(dlg, &QDialog::finished, dlg, &QObject::deleteLater);
    dlg->exec();
}

void MainWindow::on_install_zip_triggered() {
    auto *dlg = new ArchiveInstallDialog(emuenv, this);
    connect(dlg, &ArchiveInstallDialog::install_complete,
        this, [this](const QList<ArchiveInstallResult> &) { on_install_finished(); });
    connect(dlg, &QDialog::finished, dlg, &QObject::deleteLater);
    dlg->exec();
}

void MainWindow::on_install_license_triggered() {
    auto *dlg = new LicenseInstallDialog(emuenv, this);
    connect(dlg, &QDialog::finished, dlg, &QObject::deleteLater);
    dlg->exec();
}

void MainWindow::on_install_finished() {
    m_apps_list_widget->refresh(true);
}

void MainWindow::dragEnterEvent(QDragEnterEvent *event) {
    if (event->mimeData()->hasUrls()) {
        for (const QUrl &url : event->mimeData()->urls()) {
            if (!url.isLocalFile())
                continue;
            const QString path = url.toLocalFile();
            const QString ext = QFileInfo(path).suffix().toLower();
            if (ext == QStringLiteral("pup") || ext == QStringLiteral("vpk")
                || ext == QStringLiteral("zip") || ext == QStringLiteral("pkg")
                || ext == QStringLiteral("rif") || path.endsWith(QStringLiteral("work.bin"))
                || QFileInfo(path).isDir() || path.endsWith(QStringLiteral("theme.xml"))) {
                event->acceptProposedAction();
                return;
            }
        }
    }
}

void MainWindow::dropEvent(QDropEvent *event) {
    for (const QUrl &url : event->mimeData()->urls()) {
        if (url.isLocalFile())
            handle_drop(url.toLocalFile());
    }
}

void MainWindow::handle_drop(const QString &path) {
    const fs::path drop_path(path.toStdString());
    const std::string ext = string_utils::tolower(drop_path.extension().string());

    if (ext == ".pup") {
        QProgressDialog prog(tr("Installing firmware\u2026"),
            /*cancel=*/QString(), 0, 100, this);
        prog.setWindowTitle(tr("Install Firmware"));
        prog.setWindowModality(Qt::WindowModal);
        prog.setMinimumDuration(0);
        prog.setValue(0);

        const std::string fw_version = install_pup(
            emuenv.pref_path,
            drop_path,
            [&prog](uint32_t pct) {
                prog.setValue(static_cast<int>(pct));
                QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
            });

        prog.setValue(100);

        if (!fw_version.empty()) {
            LOG_INFO("Firmware {} installed successfully via drop!", fw_version);
            on_install_finished();
        } else {
            QMessageBox::critical(this, tr("Install Failed"),
                tr("Failed to install the dropped firmware file.\n"
                   "Check the log for details."));
        }

    } else if (ext == ".vpk" || ext == ".zip") {
        const ReinstallCallback reinstall_cb = [this]() -> bool {
            return QMessageBox::question(
                       this,
                       tr("Already Installed"),
                       tr("This content is already installed.\n"
                          "Do you want to overwrite it?"),
                       QMessageBox::Yes | QMessageBox::No,
                       QMessageBox::No)
                == QMessageBox::Yes;
        };

        const std::vector<ContentInfo> contents = install_archive(emuenv, drop_path, {}, reinstall_cb);

        if (!contents.empty())
            on_install_finished();

    } else if (ext == ".rif" || drop_path.filename() == "work.bin") {
        if (copy_license(emuenv, drop_path))
            LOG_INFO("License installed via drop: {}", drop_path.string());
        else
            QMessageBox::critical(this, tr("Install Failed"),
                tr("Failed to install the dropped license file.\n"
                   "The file may be corrupted."));

    } else if (fs::is_directory(drop_path)) {
        const uint32_t n = install_contents(emuenv, drop_path);
        if (n > 0)
            on_install_finished();

    } else if (drop_path.filename() == "theme.xml") {
        const uint32_t n = install_contents(emuenv, drop_path.parent_path());
        if (n > 0)
            on_install_finished();

    } else {
        LOG_ERROR("Dropped file '{}' is not a supported install type.",
            drop_path.filename().string());
    }
}

void MainWindow::on_about_action_triggered() {
    AboutDialog about(emuenv, this);
    about.exec();
}

void MainWindow::on_controls_action_triggered() {
    m_sdl_pump_timer->stop();
    ControlsDialog dlg(emuenv, this);
    dlg.exec();
    m_sdl_pump_timer->start();
}

void MainWindow::open_settings(int tab_index) {
    SettingsDialog dlg(emuenv, m_gui_settings, {}, tab_index, this);
    connect(&dlg, &SettingsDialog::gui_stylesheet_request, this, &MainWindow::apply_stylesheet);
    connect(&dlg, &SettingsDialog::ui_language_request, this, &MainWindow::apply_ui_language);
    dlg.exec();

    sync_discord_presence();

    refresh_status_bar();

    if (m_log_widget && m_gui_settings) {
        const int buf_size = m_gui_settings->get_value(gui::l_bufferSize).toInt();
        m_log_widget->set_log_buffer_size(buf_size);
    }

    if (dlg.storage_path_switched())
        m_apps_list_widget->refresh(true);
}

void MainWindow::apply_ui_language(const QString &locale_tag) {
    gui::i18n::apply_ui_language(*qApp, locale_tag.toStdString());
}

void MainWindow::init_current_user() {
    const auto &users = emuenv.app.user_list.users;

    if (users.empty()) {
        const std::string id = app::create_user(emuenv, "Vita3K");
        app::activate_user(emuenv, id);
        emuenv.cfg.user_id = id;
        config::serialize_config(emuenv.cfg, emuenv.cfg.config_path);
        return;
    }

    if (!emuenv.cfg.user_id.empty() && users.contains(emuenv.cfg.user_id)) {
        app::activate_user(emuenv, emuenv.cfg.user_id);
        return;
    }

    const auto &first = users.begin();
    app::activate_user(emuenv, first->first);
    emuenv.cfg.user_id = first->first;
    config::serialize_config(emuenv.cfg, emuenv.cfg.config_path);
}

void MainWindow::on_app_selected(const app::AppEntry &app) {
    if (emuenv.cfg.show_live_area_screen && !m_game_window)
        show_live_area(app.title_id);
    else
        boot_game(app.title_id);
}

void MainWindow::boot_game(const std::string &title_id) {
    if (m_game_window) {
        const int result = QMessageBox::question(this, tr("Switch App?"),
            tr("An app is already running.\n"
               "Do you want to close it and launch another app?\n\n"
               "Any unsaved progress will be lost!"),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No);

        if (result != QMessageBox::Yes)
            return;

        on_game_closed();
    }

    if (!confirm_missing_firmware_warning())
        return;

    if (!m_live_area_widget) {
        if (!app::set_app_info(emuenv, title_id)) {
            QMessageBox::critical(this, tr("Error"),
                tr("Could not find app '%1' in apps list.").arg(QString::fromStdString(title_id)));
            return;
        }
        get_license(emuenv, emuenv.io.title_id, emuenv.io.content_id);
        app::update_last_time_app_used(emuenv, title_id);
    }

    app::set_current_config(emuenv, title_id);

    QRect launch_geometry(0, 0, 960, 544);
    bool launch_fullscreen = emuenv.cfg.boot_apps_full_screen;
    if (m_game_container) {
        launch_geometry = m_game_container->geometry();
        launch_fullscreen = m_game_container->visibility() == QWindow::FullScreen;
    }

    if (m_live_area_widget) {
        m_live_area_widget->removeEventFilter(this);
        m_live_area_widget->deleteLater();
        m_live_area_widget = nullptr;
        m_game_container = nullptr;
    }

    m_game_window = new GameWindow(emuenv, emuenv.backend_renderer, screen());
    m_game_container = m_game_window;
    m_game_window->setTitle(tr("%1 | %2 (%3) | Loading...")
                                .arg(QString::fromStdString(window_title),
                                    QString::fromStdString(emuenv.current_app_title),
                                    QString::fromStdString(emuenv.io.title_id)));
    m_game_window->setIcon(windowIcon());
    m_game_window->setGeometry(launch_geometry);
    m_game_window->installEventFilter(this);
    m_game_window->show();

    if (launch_fullscreen)
        m_game_window->showFullScreen();

    m_fullscreen = launch_fullscreen;
    m_ui->toolbar_fullscreen->setIcon(m_fullscreen ? m_icon_fullscreen_off : m_icon_fullscreen_on);
    m_ui->toolbar_fullscreen->setText(m_fullscreen ? tr("Exit Fullscreen") : tr("Fullscreen"));

    connect(m_game_window, &GameWindow::closed,
        this, &MainWindow::on_game_closed);

    const auto abort_boot = [&](const QString &msg) {
        QMessageBox::critical(this, tr("Error"), msg);
        if (m_game_window) {
            m_game_window->removeEventFilter(this);
            m_game_window->deleteLater();
        }
        m_game_window = nullptr;
        m_game_container = nullptr;
    };

    renderer::WindowCallbacks callbacks;
#ifdef _WIN32
    callbacks.native_handle = reinterpret_cast<void *>(m_game_window->winId());
    callbacks.display_protocol = renderer::DisplayProtocol::Win32;
#elif defined(__APPLE__)
    callbacks.native_handle = reinterpret_cast<void *>(m_game_window->winId());
    callbacks.display_protocol = renderer::DisplayProtocol::MacOS;
#elif defined(HAVE_X11) || defined(HAVE_WAYLAND)
    {
        QPlatformNativeInterface *native = QGuiApplication::platformNativeInterface();
        if (native) {
#if defined(HAVE_WAYLAND)
            auto *wl_dpy = native->nativeResourceForWindow("display", nullptr);
            auto *wl_surf = native->nativeResourceForWindow("surface",
                const_cast<QWindow *>(static_cast<const QWindow *>(m_game_window)));
            if (wl_dpy && wl_surf) {
                callbacks.native_handle = wl_surf;
                callbacks.native_display = wl_dpy;
                callbacks.display_protocol = renderer::DisplayProtocol::Wayland;
                LOG_INFO("Using Wayland display protocol for Vulkan surface");
            } else
#endif
#if defined(HAVE_X11)
            {
                void *x11_display = native->nativeResourceForIntegration("display");
                callbacks.native_handle = reinterpret_cast<void *>(m_game_window->winId());
                callbacks.native_display = x11_display;
                callbacks.display_protocol = renderer::DisplayProtocol::X11;
                LOG_INFO("Using X11 display protocol for Vulkan surface");
            }
#endif
        } else {
            LOG_ERROR("Could not get QPlatformNativeInterface, display protocol unknown");
        }
    }
#endif
    callbacks.client_width = [win = m_game_window]() -> int {
        return win->client_width_px();
    };
    callbacks.client_height = [win = m_game_window]() -> int {
        return win->client_height_px();
    };
    callbacks.get_font_dirs = []() -> std::vector<std::string> {
        const QStringList locations = QStandardPaths::standardLocations(QStandardPaths::FontsLocation);
        std::vector<std::string> font_dirs;
        for (const QString &location : locations) {
            std::string font_dir = location.toStdString();
            if (!font_dir.ends_with('/') && !font_dir.ends_with('\\'))
                font_dir += '/';
            font_dirs.push_back(font_dir);
        }
        return font_dirs;
    };

    bool renderer_ok = false;
    if (emuenv.backend_renderer == renderer::Backend::Vulkan) {
        renderer_ok = renderer::init(callbacks, emuenv.renderer,
            emuenv.backend_renderer, emuenv.cfg, emuenv.get_root_paths());
    } else {
        if (!m_game_window->create_gl_context()) {
            abort_boot(tr("Could not create OpenGL context.\nDoes your GPU support at least OpenGL 4.4?"));
            return;
        }
        if (!m_game_window->make_current()) {
            abort_boot(tr("Could not make OpenGL context current."));
            return;
        }
        auto *gl_ctx = m_game_window->gl_context();
        callbacks.get_proc_address = [gl_ctx](const char *name) -> void * {
            return reinterpret_cast<void *>(
                reinterpret_cast<uintptr_t>(gl_ctx->getProcAddress(name)));
        };
        callbacks.default_fbo = [gl_ctx]() -> unsigned int {
            return gl_ctx->defaultFramebufferObject();
        };
        callbacks.swap = [win = m_game_window]() {
            win->swap_buffers();
        };
        callbacks.set_vsync = [win = m_game_window](bool enabled) -> bool {
            return win->set_vsync_enabled(enabled);
        };
        callbacks.set_current = [win = m_game_window]() -> bool {
            return win->make_current();
        };
        callbacks.done_current = [win = m_game_window]() {
            win->done_current();
        };
        renderer_ok = renderer::init(callbacks, emuenv.renderer,
            emuenv.backend_renderer, emuenv.cfg, emuenv.get_root_paths());
    }

    if (!renderer_ok) {
        abort_boot(tr("Failed to initialise the renderer."));
        return;
    }

    app::apply_renderer_config(emuenv);

    m_game_window->setTitle(tr("%1 | %2 (%3) | Initializing...")
                                .arg(QString::fromStdString(window_title),
                                    QString::fromStdString(emuenv.current_app_title),
                                    QString::fromStdString(emuenv.io.title_id)));

    if (!app::late_init(emuenv)) {
        abort_boot(tr("Failed to initialize emulator state."));
        return;
    }

    m_game_window->setTitle(tr("%1 | %2 (%3) | Loading modules...")
                                .arg(QString::fromStdString(window_title),
                                    QString::fromStdString(emuenv.current_app_title),
                                    QString::fromStdString(emuenv.io.title_id)));

    int32_t main_module_id = 0;
    if (load_app(main_module_id, emuenv) != Success) {
        abort_boot(tr("Failed to load app modules."));
        return;
    }

    emuenv.renderer->set_app(emuenv.io.title_id.c_str(), emuenv.self_name.c_str());

    {
        const auto bg_path = fs::path(emuenv.pref_path) / "ux0/app" / emuenv.io.app_path / "sce_sys/pic0.png";
        if (fs::exists(bg_path))
            emuenv.renderer->precompile_bg_path = bg_path.string();
    }

    if (renderer::get_shaders_cache_hashs(*emuenv.renderer) && emuenv.cfg.shader_cache) {
        const auto &hashes = emuenv.renderer->shaders_cache_hashs;
        emuenv.renderer->precompile_queue = hashes;
        emuenv.renderer->precompile_progress.store(0, std::memory_order_relaxed);
        emuenv.renderer->precompile_complete.store(false, std::memory_order_relaxed);
        emuenv.renderer->precompile_requested.store(true, std::memory_order_release);
    }

    if (run_app(emuenv, main_module_id) != Success) {
        abort_boot(tr("Failed to start game threads."));
        return;
    }

    auto *kb_filter = new CtrlKeyboardFilter(emuenv, m_game_window);
    m_game_window->installEventFilter(kb_filter);
    m_kb_filter = kb_filter;

    auto *ime_filter = new ImeKeyboardFilter(emuenv, m_game_window);
    m_game_window->installEventFilter(ime_filter);

    connect(m_kb_filter, &CtrlKeyboardFilter::ps_button_pressed,
        this, &MainWindow::on_ps_button);

    connect(m_kb_filter, &CtrlKeyboardFilter::fullscreen_toggled,
        this, [this]() { if (m_game_window) m_game_window->toggle_fullscreen(); });

    connect(m_kb_filter, &CtrlKeyboardFilter::texture_replacement_toggled,
        this, [this]() { toggle_texture_replacement(emuenv); });

    connect(m_kb_filter, &CtrlKeyboardFilter::screenshot_requested,
        this, [this]() { take_screenshot(emuenv); });

    if (emuenv.backend_renderer == renderer::Backend::OpenGL) {
        m_game_window->done_current();
        m_game_window->prepare_gl_for_render_thread();
    }

    renderer::start_render_thread(*emuenv.renderer, emuenv.display, emuenv.gxm, emuenv.mem, emuenv.cfg);

    if (emuenv.backend_renderer == renderer::Backend::OpenGL)
        m_game_window->complete_gl_migration();

    if (emuenv.renderer->precompile_requested.load(std::memory_order_acquire)) {
        while (!emuenv.renderer->precompile_complete.load(std::memory_order_acquire)
            && !emuenv.renderer->render_abort.load(std::memory_order_relaxed)) {
            QApplication::processEvents(QEventLoop::ExcludeUserInputEvents, 50);
        }
    }

    m_game_window->start_ui_updates();

    m_game_window->setTitle(tr("%1 | %2 (%3) | Please wait, loading...")
                                .arg(QString::fromStdString(window_title),
                                    QString::fromStdString(emuenv.current_app_title),
                                    QString::fromStdString(emuenv.io.title_id)));

    LOG_INFO("Game started: {} ({})", emuenv.current_app_title, title_id);

    refresh_emulation_actions();
    refresh_status_bar();
}

void MainWindow::on_game_closed() {
    if (!m_game_window)
        return;

    LOG_INFO("Game closed: {}", emuenv.current_app_title);

    app::update_app_time_used(emuenv, emuenv.io.app_path);

    m_game_window->stop_ui_updates();

    renderer::stop_render_thread(*emuenv.renderer);

    if (emuenv.backend_renderer == renderer::Backend::OpenGL)
        m_game_window->make_current();

    app::deinit(emuenv);
    app::destroy(emuenv);

    if (emuenv.backend_renderer == renderer::Backend::OpenGL) {
        // try to destroy qopenglcontext immediately, not sure if this is working
        m_game_window->destroy_gl_context();
    }

    QWindow *closed_window = m_game_container;
    if (closed_window)
        closed_window->removeEventFilter(this);

    m_game_window = nullptr;
    m_game_container = nullptr;

    if (closed_window)
        closed_window->deleteLater();

    m_kb_filter = nullptr;
    m_fullscreen = false;
    m_ui->toolbar_fullscreen->setIcon(m_icon_fullscreen_on);
    m_ui->toolbar_fullscreen->setText(tr("Fullscreen"));

    refresh_emulation_actions();

    config::set_current_config(emuenv.cfg, emuenv.config_path, {});
    refresh_status_bar();

    raise();
    activateWindow();
}

bool MainWindow::confirm_missing_firmware_warning() {
    if (!emuenv.cfg.warn_missing_firmware)
        return true;

    const app::FirmwareState firmware = app::get_firmware_state(emuenv);
    if (firmware.pd0 && firmware.vs0 && firmware.sa0)
        return true;

    QStringList missing_components;
    if (!firmware.pd0)
        missing_components << tr("Pre-install package");
    if (!firmware.vs0)
        missing_components << tr("Main firmware");
    if (!firmware.sa0)
        missing_components << tr("Font package");

    QMessageBox warning_box(this);
    warning_box.setIcon(QMessageBox::Warning);
    warning_box.setWindowTitle(tr("Missing Firmware"));
    warning_box.setText(tr("Firmware is not fully installed."));
    warning_box.setInformativeText(tr(
        "The following firmware components are missing:\n- %1\n\n"
        "Games may fail to boot or render correctly until they are installed.")
                                       .arg(missing_components.join(QStringLiteral("\n- "))));

    auto *dont_show_again = new QCheckBox(tr("Don't show this warning again"), &warning_box);
    warning_box.setCheckBox(dont_show_again);

    auto *launch_button = warning_box.addButton(tr("Launch Anyway"), QMessageBox::AcceptRole);
    warning_box.addButton(QMessageBox::Cancel);
    warning_box.setDefaultButton(QMessageBox::Cancel);
    warning_box.exec();

    if (dont_show_again->isChecked()) {
        emuenv.cfg.warn_missing_firmware = false;
        config::serialize_config(emuenv.cfg, emuenv.cfg.config_path);
    }

    return warning_box.clickedButton() == launch_button;
}

void MainWindow::show_live_area(const std::string &title_id) {
    if (m_live_area_widget || m_game_window)
        return;

    if (!app::set_app_info(emuenv, title_id)) {
        QMessageBox::critical(this, tr("Error"),
            tr("Could not find app '%1' in apps list.").arg(QString::fromStdString(title_id)));
        return;
    }

    app::set_current_config(emuenv, title_id);
    get_license(emuenv, emuenv.io.title_id, emuenv.io.content_id);
    app::update_last_time_app_used(emuenv, title_id);
    m_live_area_title_id = title_id;

    m_live_area_widget = new LiveAreaWidget(emuenv, emuenv.io.app_path);
    m_game_container = m_live_area_widget;

    m_live_area_widget->setTitle(tr("%1 | %2 (%3)")
                                     .arg(QString::fromStdString(window_title),
                                         QString::fromStdString(emuenv.current_app_title),
                                         QString::fromStdString(emuenv.io.title_id)));
    m_live_area_widget->setIcon(windowIcon());
    m_live_area_widget->installEventFilter(this);
    m_live_area_widget->show();

    if (emuenv.cfg.boot_apps_full_screen)
        m_live_area_widget->showFullScreen();

    m_fullscreen = emuenv.cfg.boot_apps_full_screen;
    m_ui->toolbar_fullscreen->setIcon(m_fullscreen ? m_icon_fullscreen_off : m_icon_fullscreen_on);
    m_ui->toolbar_fullscreen->setText(m_fullscreen ? tr("Exit Fullscreen") : tr("Fullscreen"));

    connect(m_live_area_widget, &LiveAreaWidget::play_clicked,
        this, &MainWindow::on_live_area_play);
}

void MainWindow::on_live_area_play() {
    const auto title_id = m_live_area_title_id;
    m_live_area_title_id.clear();
    boot_game(title_id);
}

void MainWindow::on_live_area_closed() {
    QWindow *closed_window = m_game_container;
    if (closed_window)
        closed_window->removeEventFilter(this);

    if (m_live_area_widget) {
        m_live_area_widget->deleteLater();
        m_live_area_widget = nullptr;
    }
    m_live_area_title_id.clear();
    m_game_container = nullptr;
    m_fullscreen = false;
    m_ui->toolbar_fullscreen->setIcon(m_icon_fullscreen_on);
    m_ui->toolbar_fullscreen->setText(tr("Fullscreen"));

    config::set_current_config(emuenv.cfg, emuenv.config_path, {});

    raise();
    activateWindow();
}

bool MainWindow::eventFilter(QObject *watched, QEvent *event) {
    if (watched == m_game_container) {
        if (event->type() == QEvent::Close) {
            event->ignore();

            if (m_live_area_widget && !m_game_window) {
                on_live_area_closed();
                return true;
            }

            const int result = QMessageBox::question(this,
                tr("Exit Game?"),
                tr("Do you really want to exit the game?\n\nAny unsaved progress will be lost!"),
                QMessageBox::Yes | QMessageBox::No, QMessageBox::No);

            if (result == QMessageBox::Yes) {
                on_game_closed();
            } else {
                m_game_container->raise();
                m_game_container->requestActivate();
            }

            return true;
        }
    }

    return QMainWindow::eventFilter(watched, event);
}

void MainWindow::pump_sdl_events() {
#if USE_DISCORD
    discordrpc::run_callbacks();
#endif

    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        switch (event.type) {
        case SDL_EVENT_GAMEPAD_ADDED:
        case SDL_EVENT_GAMEPAD_REMOVED:
            refresh_controllers(emuenv.ctrl, emuenv);
            break;

        case SDL_EVENT_GAMEPAD_TOUCHPAD_DOWN:
        case SDL_EVENT_GAMEPAD_TOUCHPAD_MOTION:
        case SDL_EVENT_GAMEPAD_TOUCHPAD_UP:
            handle_touchpad_event(event.gtouchpad);
            break;

        case SDL_EVENT_GAMEPAD_SENSOR_UPDATE:
            handle_motion_event(emuenv, event.gsensor.sensor, event.gsensor);
            break;

        case SDL_EVENT_SENSOR_UPDATE:
            handle_motion_event(emuenv, SDL_GetSensorTypeForID(event.sensor.which), event.sensor);
            break;

        case SDL_EVENT_FINGER_DOWN:
        case SDL_EVENT_FINGER_MOTION:
        case SDL_EVENT_FINGER_UP:
            handle_touch_event(event.tfinger);
            break;

        case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
            if (event.gbutton.button == SDL_GAMEPAD_BUTTON_GUIDE)
                on_ps_button();
            if (m_game_window && !emuenv.kernel.is_threads_paused()
                && event.gbutton.button == SDL_GAMEPAD_BUTTON_TOUCHPAD)
                toggle_touchscreen();
            break;

        default:
            break;
        }
    }
}

void MainWindow::on_pause_triggered() {
    if (!m_game_window)
        return;

    const bool is_paused = emuenv.kernel.is_threads_paused();
    app::switch_state(emuenv, !is_paused);

    m_game_window->set_paused(!is_paused);
    refresh_emulation_actions();
}

void MainWindow::on_stop_triggered() {
    if (!m_game_window)
        return;

    if (emuenv.kernel.is_threads_paused()) {
        app::switch_state(emuenv, false);
        m_game_window->set_paused(false);
    }

    on_game_closed();
}

void MainWindow::on_ps_button() {
    on_pause_triggered();
}

void MainWindow::refresh_emulation_actions() {
    const bool running = m_game_window != nullptr;
    const bool paused = running && emuenv.kernel.is_threads_paused();

    m_ui->stop_action->setEnabled(running);
    m_ui->stop_action->setIcon(m_icon_stop);
    m_ui->toolbar_stop->setEnabled(running);

    if (running) {
        if (paused) {
            m_ui->pause_action->setText(tr("Resume"));
            m_ui->pause_action->setIcon(m_icon_play);
            m_ui->pause_action->setEnabled(true);
            m_ui->toolbar_start->setIcon(m_icon_play);
            m_ui->toolbar_start->setText(tr("Resume"));
            m_ui->toolbar_start->setEnabled(true);
        } else {
            m_ui->pause_action->setText(tr("Pause"));
            m_ui->pause_action->setIcon(m_icon_pause);
            m_ui->pause_action->setEnabled(true);
            m_ui->toolbar_start->setIcon(m_icon_pause);
            m_ui->toolbar_start->setText(tr("Pause"));
            m_ui->toolbar_start->setEnabled(true);
        }
    } else {
        m_ui->pause_action->setText(tr("Play"));
        m_ui->pause_action->setIcon(m_icon_play);
        m_ui->pause_action->setEnabled(m_app_selected);
        m_ui->toolbar_start->setIcon(m_icon_play);
        m_ui->toolbar_start->setText(tr("Play"));
        m_ui->toolbar_start->setEnabled(m_app_selected);
    }
}

void MainWindow::open_trophy_collection() {
    if (!m_trophy_dialog) {
        m_trophy_dialog = new TrophyCollectionDialog(emuenv, this);
        connect(m_trophy_dialog, &QObject::destroyed, this, [this] {
            m_trophy_dialog = nullptr;
        });
    }
    m_trophy_dialog->show();
    m_trophy_dialog->raise();
    m_trophy_dialog->activateWindow();
}

void MainWindow::open_user_management() {
    if (m_game_window) {
        return;
    }

    if (!m_user_mgmt_dialog) {
        m_user_mgmt_dialog = new UserManagementDialog(emuenv, this);
        connect(m_user_mgmt_dialog, &UserManagementDialog::user_changed,
            m_apps_list_widget, [this] { m_apps_list_widget->refresh(); });
        connect(m_user_mgmt_dialog, &UserManagementDialog::user_changed,
            this, [this] {
                if (m_trophy_dialog)
                    m_trophy_dialog->reload();
            });
        connect(m_user_mgmt_dialog, &QObject::destroyed, this, [this] {
            m_user_mgmt_dialog = nullptr;
        });
    }
    m_user_mgmt_dialog->show();
    m_user_mgmt_dialog->raise();
    m_user_mgmt_dialog->activateWindow();
}

void MainWindow::open_debug_widget(int tab) {
    if (!m_debug_widget) {
        m_debug_widget = new DebugWidget(emuenv, this);
        m_debug_widget->setFloating(true);
        connect(m_debug_widget, &QObject::destroyed, this, [this] {
            m_debug_widget = nullptr;
        });
    }
    m_debug_widget->show_tab(tab);
}

void MainWindow::setup_toolbar() {
    auto *tb = m_ui->toolBar;
    tb->setObjectName(QStringLiteral("mw_toolbar"));

    connect(m_ui->toolbar_open, &QAction::triggered, this, [this] {
        auto *tb = m_ui->toolBar;
        auto *w = tb->widgetForAction(m_ui->toolbar_open);
        const QPoint pos = w ? w->mapToGlobal(QPoint(0, w->height())) : QCursor::pos();

        QMenu menu(this);
        menu.addAction(tr("Open Emulated Storage Path"), this, [this] {
            gui::utils::open_dir(emuenv.pref_path.string());
        });

        const auto patch_path = emuenv.shared_path / "patch";
        menu.addAction(tr("Open Patch Path"), this, [patch_path] {
            gui::utils::open_dir(patch_path.string());
        });

        const auto textures_path = emuenv.shared_path / "textures";
        menu.addAction(tr("Open Textures Path"), this, [textures_path] {
            if (!fs::exists(textures_path))
                fs::create_directories(textures_path);
            gui::utils::open_dir(textures_path.string());
        });

        menu.exec(pos);
    });

    connect(m_ui->toolbar_refresh, &QAction::triggered,
        this, &MainWindow::on_toolbar_refresh);

    connect(m_ui->toolbar_fullscreen, &QAction::triggered,
        this, &MainWindow::on_toolbar_fullscreen);

    connect(m_ui->toolbar_stop, &QAction::triggered,
        this, &MainWindow::on_stop_triggered);

    connect(m_ui->toolbar_start, &QAction::triggered,
        this, &MainWindow::on_toolbar_start);

    connect(m_ui->toolbar_config, &QAction::triggered,
        this, [this] { open_settings(0); });

    connect(m_ui->toolbar_controls, &QAction::triggered,
        this, &MainWindow::on_controls_action_triggered);

    m_slider_container = new QWidget(this);
    auto *slider_layout = new QHBoxLayout(m_slider_container);
    slider_layout->setContentsMargins(14, 0, 14, 0);
    slider_layout->setSpacing(0);

    m_icon_size_slider = new QSlider(Qt::Horizontal, m_slider_container);
    m_icon_size_slider->setRange(0, 100);
    m_icon_size_slider->setValue(25);
    m_icon_size_slider->setFocusPolicy(Qt::ClickFocus);
    slider_layout->addWidget(m_icon_size_slider);

    tb->addWidget(m_slider_container);

    connect(m_icon_size_slider, &QSlider::valueChanged,
        this, &MainWindow::resize_icons);

    m_search_bar = new QLineEdit(this);
    m_search_bar->setPlaceholderText(tr("Search..."));
    m_search_bar->setFocusPolicy(Qt::ClickFocus);
    m_search_bar->setClearButtonEnabled(true);
    tb->addWidget(m_search_bar);

    connect(m_search_bar, &QLineEdit::textChanged,
        m_apps_list_widget, &AppsList::set_search_text);

    connect(m_apps_list_widget, &AppsList::app_selection_changed,
        this, &MainWindow::on_app_selection_changed);

    repaint_toolbar_icons();

    resize_icons(m_icon_size_slider->value());
    refresh_emulation_actions();
}

void MainWindow::show_title_bars(bool show) const {
    if (m_apps_list_widget)
        m_apps_list_widget->set_title_bar_visible(show);
    if (m_log_widget)
        m_log_widget->set_title_bar_visible(show);
}

void MainWindow::repaint_gui() {
    repaint_toolbar_icons();
    if (m_log_widget)
        m_log_widget->repaint_text_colors();
    if (m_apps_list_widget)
        m_apps_list_widget->update();
    update();
}

void MainWindow::init_default_style() {
    if (QStyleFactory::keys().contains(QStringLiteral("Fusion"), Qt::CaseInsensitive))
        m_default_style = QStringLiteral("Fusion");

    if (m_default_style.isEmpty()) {
        if (const QStyle *style = QApplication::style())
            m_default_style = style->name();
    }
    if (m_default_style.isEmpty()) {
        if (const QStringList styles = QStyleFactory::keys(); !styles.empty())
            m_default_style = styles.front();
    }
}

void MainWindow::init_first_run_stylesheet() {
    if (!m_gui_settings || m_gui_settings->contains_value(gui::m_currentStylesheet))
        return;

    const QString stylesheet = gui::utils::dark_mode_active()
        ? gui::DarkStylesheet
        : gui::DefaultStylesheet;

    m_gui_settings->set_value(gui::m_currentStylesheet, stylesheet);
}

void MainWindow::apply_stylesheet() {
    if (!m_gui_settings)
        return;

    const QString stylesheet_name = m_gui_settings->get_value(gui::m_currentStylesheet).toString();

    if (QStyle *style = QStyleFactory::create(m_default_style)) {
        QApplication::setStyle(style);
        QApplication::setPalette(style->standardPalette());
    }

    const QString native_prefix = gui::NativeStylesheet + QStringLiteral(" (");
    const auto extract_native_style = [&]() -> QString {
        if (stylesheet_name.startsWith(native_prefix) && stylesheet_name.endsWith(QLatin1Char(')')))
            return stylesheet_name.mid(native_prefix.size(), stylesheet_name.size() - native_prefix.size() - 1);
        return {};
    };

    if (stylesheet_name.isEmpty() || stylesheet_name == gui::DefaultStylesheet) {
        LOG_INFO("Using default (bright) stylesheet");
        if (QStyle *fusion = QStyleFactory::create(QStringLiteral("Fusion"))) {
            QApplication::setStyle(fusion);
            QApplication::setPalette(fusion->standardPalette());
        }
        qApp->setStyleSheet(gui::stylesheets::default_style_sheet);
    } else if (stylesheet_name == gui::DarkStylesheet) {
        LOG_INFO("Using default (dark) stylesheet");
        if (QStyle *fusion = QStyleFactory::create(QStringLiteral("Fusion"))) {
            QApplication::setStyle(fusion);
            QApplication::setPalette(fusion->standardPalette());
        }
        QApplication::setPalette(make_dark_palette());
        qApp->setStyleSheet(gui::stylesheets::dark_style_sheet);
    } else if (stylesheet_name == gui::NoStylesheet) {
        LOG_INFO("Using empty stylesheet");
        qApp->setStyleSheet(QStringLiteral("/* none */"));
    } else if (const QString native_style = extract_native_style(); !native_style.isEmpty()) {
        if (QStyle *style = QStyleFactory::create(native_style)) {
            LOG_INFO("Using native style '{}'", native_style.toStdString());
            qApp->setStyleSheet(QStringLiteral("/* none */"));
            QApplication::setStyle(style);
            QApplication::setPalette(style->standardPalette());
        } else {
            LOG_ERROR("Native style '{}' not available, falling back to default", native_style.toStdString());
            qApp->setStyleSheet(gui::stylesheets::default_style_sheet);
        }
    } else {
        const QString settings_dir = m_gui_settings->get_settings_dir();
        const QString stylesheet_path = settings_dir + QStringLiteral("/") + stylesheet_name + QStringLiteral(".qss");

        if (QFile file(stylesheet_path); file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QString qss = file.readAll();
            file.close();

            qss.replace(
                QStringLiteral("url(\"gui-configs/"),
                QStringLiteral("url(\"") + settings_dir + QStringLiteral("/"));

            LOG_INFO("Loaded stylesheet '{}' from {}", stylesheet_name.toStdString(), stylesheet_path.toStdString());
            qApp->setStyleSheet(qss);
        } else {
            LOG_ERROR("Could not find stylesheet '{}', falling back to default", stylesheet_name.toStdString());
            qApp->setStyleSheet(gui::stylesheets::default_style_sheet);
        }
    }

    repaint_gui();
}

void MainWindow::repaint_toolbar_icons() {
    auto *tb = m_ui->toolBar;
    const QColor fg = gui::utils::get_label_color(
        QStringLiteral("toolbar_icon_color"),
        QColor(QStringLiteral("#5b5b5b")),
        QColor(QStringLiteral("#f3f3f3")));

    std::map<QIcon::Mode, QColor> new_colors;
    new_colors[QIcon::Normal] = gui::utils::get_label_color(fg, fg);
    new_colors[QIcon::Disabled] = gui::utils::get_label_color(Qt::gray, Qt::darkGray);
    new_colors[QIcon::Active] = gui::utils::get_label_color(fg, fg);
    new_colors[QIcon::Selected] = gui::utils::get_label_color(fg, fg);

    const auto icon = [&new_colors](const QString &path) {
        return gui::utils::get_colorized_icon(QIcon(path), Qt::black, new_colors);
    };

    m_icon_play = icon(QStringLiteral(":/icons/play.png"));
    m_icon_pause = icon(QStringLiteral(":/icons/pause.png"));
    m_icon_stop = icon(QStringLiteral(":/icons/stop.png"));
    m_icon_fullscreen_on = icon(QStringLiteral(":/icons/fullscreen.png"));
    m_icon_fullscreen_off = icon(QStringLiteral(":/icons/exit_fullscreen.png"));

    m_ui->toolbar_open->setIcon(icon(QStringLiteral(":/icons/open.png")));
    m_ui->toolbar_refresh->setIcon(icon(QStringLiteral(":/icons/refresh.png")));
    m_ui->toolbar_fullscreen->setIcon(m_fullscreen ? m_icon_fullscreen_off : m_icon_fullscreen_on);
    m_ui->toolbar_stop->setIcon(m_icon_stop);
    m_ui->toolbar_config->setIcon(icon(QStringLiteral(":/icons/configure.png")));
    m_ui->toolbar_controls->setIcon(icon(QStringLiteral(":/icons/controllers.png")));

    const bool running = m_game_window != nullptr;
    const bool paused = running && emuenv.kernel.is_threads_paused();
    if (running && !paused)
        m_ui->toolbar_start->setIcon(m_icon_pause);
    else
        m_ui->toolbar_start->setIcon(m_icon_play);

    const int tool_bar_height = tb->sizeHint().height();

    for (const auto &act : tb->actions()) {
        if (act->isSeparator())
            continue;
        if (auto *w = tb->widgetForAction(act))
            w->setMinimumWidth(tool_bar_height);
    }

    if (m_slider_container)
        m_slider_container->setFixedWidth(tool_bar_height * 4);
    if (m_search_bar)
        m_search_bar->setFixedWidth(tool_bar_height * 5);

    const QColor &handle_color = new_colors[QIcon::Normal];
    if (m_icon_size_slider) {
        m_icon_size_slider->setStyleSheet(
            QStringLiteral("QSlider::handle:horizontal{ background: rgba(%1, %2, %3, %4); }")
                .arg(handle_color.red())
                .arg(handle_color.green())
                .arg(handle_color.blue())
                .arg(handle_color.alpha()));
    }
}

void MainWindow::on_toolbar_refresh() {
    m_apps_list_widget->refresh(true);
}

void MainWindow::on_toolbar_fullscreen() {
    if (m_fullscreen) {
        if (m_game_container)
            m_game_container->showNormal();
        else
            showNormal();
        m_fullscreen = false;
        m_ui->toolbar_fullscreen->setIcon(m_icon_fullscreen_on);
        m_ui->toolbar_fullscreen->setText(tr("Fullscreen"));
    } else {
        if (m_game_container)
            m_game_container->showFullScreen();
        else
            showFullScreen();
        m_fullscreen = true;
        m_ui->toolbar_fullscreen->setIcon(m_icon_fullscreen_off);
        m_ui->toolbar_fullscreen->setText(tr("Exit Fullscreen"));
    }
}

void MainWindow::on_toolbar_start() {
    if (m_game_window) {
        on_pause_triggered();
    } else {
        const app::AppEntry *app = m_apps_list_widget->selected_app();
        if (app && !app->title_id.empty()) {
            if (emuenv.cfg.show_live_area_screen && !m_live_area_widget)
                show_live_area(app->title_id);
            else
                boot_game(app->title_id);
        }
    }
}

void MainWindow::on_app_selection_changed(const app::AppEntry *app) {
    m_app_selected = app && !app->title_id.empty();
    refresh_emulation_actions();
}

void MainWindow::on_context_menu_requested(const QPoint &global_pos, const std::vector<const app::AppEntry *> &apps) {
    AppsListContextMenu menu(emuenv, apps, m_game_compat, m_game_window != nullptr, this);

    connect(&menu, &AppsListContextMenu::boot_requested,
        this, [this](const app::AppEntry &app) {
            boot_game(app.title_id);
        });
    connect(&menu, &AppsListContextMenu::view_live_area_requested,
        this, [this](const app::AppEntry &app) {
            show_live_area(app.title_id);
        });
    connect(&menu, &AppsListContextMenu::open_settings_requested,
        this, [this](const std::string &app_path, int tab) {
            SettingsDialog dlg(emuenv, m_gui_settings, app_path, tab, this);
            dlg.exec();
            if (!app_path.empty() && !m_game_window) {
                config::set_current_config(emuenv.cfg, emuenv.config_path, {});
            }
            refresh_status_bar();
            m_apps_list_widget->refresh();
        });
    connect(&menu, &AppsListContextMenu::refresh_requested,
        this, [this] { m_apps_list_widget->refresh(); });

    menu.exec(global_pos);
}

void MainWindow::resize_icons(int index) {
    if (m_icon_size_slider->value() != index) {
        m_icon_size_slider->setSliderPosition(index);
        return;
    }
    m_apps_list_widget->resize_icons(index);
}

void MainWindow::setup_status_bar() {
    auto *sb = statusBar();
    sb->setObjectName(QStringLiteral("emulatorStatusBar"));

    m_renderer_button = new QPushButton(this);
    m_renderer_button->setObjectName(QStringLiteral("status_button"));
    m_renderer_button->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_renderer_button, &QPushButton::clicked, this, [this] {
        auto &cc = emuenv.cfg.current_config;
        cc.backend_renderer = (cc.backend_renderer == "Vulkan") ? "OpenGL" : "Vulkan";
        emuenv.cfg.backend_renderer = cc.backend_renderer;
        save_config();
        update_renderer_button();
        update_accuracy_button();
        update_screen_filter_button();
    });
    connect(m_renderer_button, &QPushButton::customContextMenuRequested, this, [this](const QPoint &pos) {
        QMenu menu(this);
        auto *vulkan_action = menu.addAction(QStringLiteral("Vulkan"));
        auto *opengl_action = menu.addAction(QStringLiteral("OpenGL"));
        QAction *chosen = menu.exec(m_renderer_button->mapToGlobal(pos));
        if (!chosen)
            return;
        auto &cc = emuenv.cfg.current_config;
        if (chosen == vulkan_action) {
            cc.backend_renderer = "Vulkan";
        } else if (chosen == opengl_action) {
            cc.backend_renderer = "OpenGL";
        }
        emuenv.cfg.backend_renderer = cc.backend_renderer;
        save_config();
        update_renderer_button();
        update_accuracy_button();
        update_screen_filter_button();
    });
    sb->addWidget(m_renderer_button);

    m_accuracy_button = new QPushButton(this);
    m_accuracy_button->setObjectName(QStringLiteral("status_button"));
    m_accuracy_button->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_accuracy_button, &QPushButton::clicked, this, [this] {
        auto &cc = emuenv.cfg.current_config;
        cc.high_accuracy = !cc.high_accuracy;
        save_config();
        update_accuracy_button();
    });
    connect(m_accuracy_button, &QPushButton::customContextMenuRequested, this, [this](const QPoint &pos) {
        QMenu menu(this);
        auto *standard_action = menu.addAction(tr("Standard"));
        auto *high_action = menu.addAction(tr("High"));
        QAction *chosen = menu.exec(m_accuracy_button->mapToGlobal(pos));
        if (!chosen)
            return;
        auto &cc = emuenv.cfg.current_config;
        cc.high_accuracy = (chosen == high_action);
        save_config();
        update_accuracy_button();
    });
    sb->addWidget(m_accuracy_button);

    auto get_filter_names = [this]() -> QStringList {
        if (emuenv.cfg.current_config.backend_renderer == "Vulkan")
            return { QStringLiteral("Nearest"), QStringLiteral("Bilinear"),
                QStringLiteral("Bicubic"), QStringLiteral("FXAA"), QStringLiteral("FSR") };
        else
            return { QStringLiteral("Bilinear"), QStringLiteral("FXAA") };
    };

    auto apply_screen_filter = [this](const std::string &filter) {
        emuenv.cfg.current_config.screen_filter = filter;
        save_config();
        if (emuenv.renderer)
            emuenv.renderer->set_screen_filter(filter);
        update_screen_filter_button();
    };

    m_screen_filter_button = new QPushButton(this);
    m_screen_filter_button->setObjectName(QStringLiteral("status_button"));
    m_screen_filter_button->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_screen_filter_button, &QPushButton::clicked, this, [this, get_filter_names, apply_screen_filter] {
        const QStringList filters = get_filter_names();
        const QString current = QString::fromStdString(emuenv.cfg.current_config.screen_filter);
        int idx = filters.indexOf(current);
        idx = (idx + 1) % filters.size();
        apply_screen_filter(filters[idx].toStdString());
    });
    connect(m_screen_filter_button, &QPushButton::customContextMenuRequested, this, [this, get_filter_names, apply_screen_filter](const QPoint &pos) {
        const QStringList filters = get_filter_names();
        QMenu menu(this);
        for (const auto &name : filters)
            menu.addAction(name);
        QAction *chosen = menu.exec(m_screen_filter_button->mapToGlobal(pos));
        if (!chosen)
            return;
        apply_screen_filter(chosen->text().toStdString());
    });
    sb->addWidget(m_screen_filter_button);

    m_audio_backend_button = new QPushButton(this);
    m_audio_backend_button->setObjectName(QStringLiteral("status_button"));
    m_audio_backend_button->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_audio_backend_button, &QPushButton::clicked, this, [this] {
        auto &cc = emuenv.cfg.current_config;
        cc.audio_backend = (cc.audio_backend == "SDL") ? "Cubeb" : "SDL";
        save_config();
        update_audio_backend_button();
    });
    connect(m_audio_backend_button, &QPushButton::customContextMenuRequested, this, [this](const QPoint &pos) {
        QMenu menu(this);
        auto *sdl_action = menu.addAction(QStringLiteral("SDL"));
        auto *cubeb_action = menu.addAction(QStringLiteral("Cubeb"));
        QAction *chosen = menu.exec(m_audio_backend_button->mapToGlobal(pos));
        if (!chosen)
            return;
        auto &cc = emuenv.cfg.current_config;
        cc.audio_backend = (chosen == cubeb_action) ? "Cubeb" : "SDL";
        save_config();
        update_audio_backend_button();
    });
    sb->addWidget(m_audio_backend_button);

    m_ngs_button = new QPushButton(this);
    m_ngs_button->setObjectName(QStringLiteral("status_button"));
    m_ngs_button->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_ngs_button, &QPushButton::clicked, this, [this] {
        auto &cc = emuenv.cfg.current_config;
        cc.ngs_enable = !cc.ngs_enable;
        save_config();
        update_ngs_button();
    });
    connect(m_ngs_button, &QPushButton::customContextMenuRequested, this, [this](const QPoint &pos) {
        QMenu menu(this);
        auto *on_action = menu.addAction(tr("NGS: ON"));
        auto *off_action = menu.addAction(tr("NGS: OFF"));
        QAction *chosen = menu.exec(m_ngs_button->mapToGlobal(pos));
        if (!chosen)
            return;
        emuenv.cfg.current_config.ngs_enable = (chosen == on_action);
        save_config();
        update_ngs_button();
    });
    sb->addWidget(m_ngs_button);

    m_volume_button = new QPushButton(this);
    m_volume_button->setObjectName(QStringLiteral("status_button"));
    m_volume_button->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_volume_button, &QPushButton::clicked, this, [this] {
        QMenu menu(this);
        auto *container = new QWidget(&menu);
        auto *layout = new QHBoxLayout(container);
        layout->setContentsMargins(12, 8, 12, 8);

        auto *slider = new QSlider(Qt::Horizontal, container);
        slider->setRange(0, 100);
        slider->setValue(emuenv.cfg.current_config.audio_volume);
        slider->setMinimumWidth(180);

        layout->addWidget(slider);
        connect(slider, &QSlider::valueChanged, this, [this](int val) {
            emuenv.cfg.current_config.audio_volume = val;
            emuenv.audio.set_global_volume(val / 100.f);
            save_config();
            update_volume_button();
        });
        auto *action = new QWidgetAction(&menu);
        action->setDefaultWidget(container);
        menu.addAction(action);
        menu.exec(m_volume_button->mapToGlobal(QPoint(0, -menu.sizeHint().height())));
    });
    connect(m_volume_button, &QPushButton::customContextMenuRequested, this, [this](const QPoint &pos) {
        QMenu menu(this);
        auto *mute_action = menu.addAction(tr("Mute"));
        auto *reset_action = menu.addAction(tr("Reset to 100%"));
        QAction *chosen = menu.exec(m_volume_button->mapToGlobal(pos));
        if (!chosen)
            return;
        if (chosen == mute_action) {
            emuenv.cfg.current_config.audio_volume = 0;
        } else if (chosen == reset_action) {
            emuenv.cfg.current_config.audio_volume = 100;
        }
        emuenv.audio.set_global_volume(emuenv.cfg.current_config.audio_volume / 100.f);
        save_config();
        update_volume_button();
    });
    sb->addWidget(m_volume_button);

    update_renderer_button();
    update_accuracy_button();
    update_screen_filter_button();
    update_audio_backend_button();
    update_ngs_button();
    update_volume_button();
}

void MainWindow::update_renderer_button() {
    const auto &renderer = emuenv.cfg.current_config.backend_renderer;
    if (renderer == "Vulkan") {
        m_renderer_button->setText(QStringLiteral("VULKAN"));
        update_status_button_accent(m_renderer_button, QStringLiteral("renderer_vulkan"));
    } else {
        m_renderer_button->setText(QStringLiteral("OPENGL"));
        update_status_button_accent(m_renderer_button, QStringLiteral("renderer_opengl"));
    }
}

void MainWindow::update_accuracy_button() {
    const bool high = emuenv.cfg.current_config.high_accuracy;
    const bool is_vulkan = emuenv.cfg.current_config.backend_renderer == "Vulkan";
    m_accuracy_button->setVisible(is_vulkan);
    if (high) {
        m_accuracy_button->setText(tr("HIGH"));
        update_status_button_accent(m_accuracy_button, QStringLiteral("high"));
    } else {
        m_accuracy_button->setText(tr("STANDARD"));
        update_status_button_accent(m_accuracy_button, QStringLiteral("standard"));
    }
}

void MainWindow::update_screen_filter_button() {
    auto &cc = emuenv.cfg.current_config;
    const QStringList valid = (cc.backend_renderer == "Vulkan")
        ? QStringList{ QStringLiteral("Nearest"), QStringLiteral("Bilinear"),
              QStringLiteral("Bicubic"), QStringLiteral("FXAA"), QStringLiteral("FSR") }
        : QStringList{ QStringLiteral("Bilinear"), QStringLiteral("FXAA") };

    const QString current = QString::fromStdString(cc.screen_filter);
    if (!valid.contains(current)) {
        cc.screen_filter = valid.first().toStdString();
        save_config();
    }

    m_screen_filter_button->setText(QString::fromStdString(cc.screen_filter).toUpper());
    update_status_button_accent(m_screen_filter_button, QStringLiteral("neutral"));
}

void MainWindow::update_audio_backend_button() {
    const auto &backend = emuenv.cfg.current_config.audio_backend;
    if (backend == "SDL") {
        m_audio_backend_button->setText(QStringLiteral("SDL"));
    } else {
        m_audio_backend_button->setText(QStringLiteral("CUBEB"));
    }
    update_status_button_accent(m_audio_backend_button, QStringLiteral("audio"));
}

void MainWindow::update_ngs_button() {
    const bool ngs = emuenv.cfg.current_config.ngs_enable;
    if (ngs) {
        m_ngs_button->setText(tr("NGS: ON"));
        update_status_button_accent(m_ngs_button, QStringLiteral("neutral"));
    } else {
        m_ngs_button->setText(tr("NGS: OFF"));
        update_status_button_accent(m_ngs_button, QStringLiteral("muted"));
    }
}

void MainWindow::update_volume_button() {
    const int vol = emuenv.cfg.current_config.audio_volume;

    if (vol == 0) {
        m_volume_button->setText(tr("VOLUME: MUTED"));
        update_status_button_accent(m_volume_button, QStringLiteral("muted"));
    } else {
        m_volume_button->setText(tr("VOLUME: %1%").arg(vol));
        update_status_button_accent(m_volume_button, QStringLiteral("neutral"));
    }
}

void MainWindow::refresh_status_bar() {
    const bool running = m_game_window != nullptr;

    update_renderer_button();
    update_accuracy_button();
    update_screen_filter_button();
    update_audio_backend_button();
    update_ngs_button();
    update_volume_button();

    m_renderer_button->setEnabled(!running);
    m_accuracy_button->setEnabled(!running);
    m_audio_backend_button->setEnabled(!running);
    m_ngs_button->setEnabled(!running);
}

void MainWindow::save_config() {
    if (m_game_window && config::has_custom_config(emuenv.config_path, emuenv.io.app_path)) {
        config::save_custom_config(emuenv.cfg.current_config, emuenv.config_path, emuenv.io.app_path);
    } else {
        config::copy_current_config_to_global(emuenv.cfg);
    }
    config::serialize_config(emuenv.cfg, emuenv.cfg.config_path);
}

void MainWindow::init_discord() {
#if USE_DISCORD
    m_discord_rich_presence_old = emuenv.cfg.discord_rich_presence;
#endif
}

void MainWindow::sync_discord_presence() {
#if USE_DISCORD
    if (!emuenv.cfg.discord_rich_presence) {
        if (m_discord_rich_presence_old)
            discordrpc::clear_presence();
        m_discord_rich_presence_old = false;
        return;
    }

    if (!discordrpc::init()) {
        m_discord_rich_presence_old = false;
        return;
    }

    if (m_game_window)
        discordrpc::update_presence(emuenv.io.title_id, emuenv.current_app_title);
    else
        discordrpc::update_presence();

    m_discord_rich_presence_old = true;
#endif
}

void MainWindow::shutdown_discord() {
#if USE_DISCORD
    if (discordrpc::is_running())
        discordrpc::shutdown();
    m_discord_rich_presence_old = false;
#endif
}
