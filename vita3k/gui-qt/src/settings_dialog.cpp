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

#include "ui_settings_dialog.h"

#include <app/functions.h>
#include <app/state.h>
#include <audio/state.h>
#include <compat/state.h>
#include <config/functions.h>
#include <config/settings.h>
#include <emuenv/state.h>
#include <gui-qt/gui_language.h>
#include <gui-qt/gui_settings.h>
#include <gui-qt/settings_dialog.h>
#include <gui-qt/settings_dialog_tooltips.h>
#include <io/state.h>
#include <kernel/state.h>
#include <renderer/functions.h>
#include <util/log.h>
#include <util/system.h>

#include <QCheckBox>
#include <QColorDialog>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QRadioButton>
#include <QSlider>
#include <QSpinBox>
#include <QStyleFactory>

SettingsDialog::SettingsDialog(EmuEnvState &emuenv,
    std::shared_ptr<GuiSettings> gui_settings,
    const std::string &app_path,
    int initial_tab,
    QWidget *parent)
    : QDialog(parent)
    , emuenv(emuenv)
    , m_gui_settings(std::move(gui_settings))
    , m_tooltips(std::make_unique<SettingsDialogTooltips>(this))
    , m_app_path(app_path)
    , m_initial_tab(initial_tab)
    , m_ui(std::make_unique<Ui::vita3k_settings_dialog>()) {
    m_ui->setupUi(this);
    apply_text_overrides();

    setStyleSheet(QStringLiteral(
        "QGroupBox { font-weight: bold; border: 1px solid gray; border-radius: 4px; margin-top: 0.5em; padding-top: 0.4em; }"
        "QGroupBox::title { subcontrol-origin: margin; subcontrol-position: top center; padding: 0 4px; }"));

    for (auto *gb : { m_ui->gb_description_core, m_ui->gb_description_cpu,
             m_ui->gb_description_gpu, m_ui->gb_description_audio,
             m_ui->gb_description_camera,
             m_ui->gb_description_system, m_ui->gb_description_emulator,
             m_ui->gb_description_gui,
             m_ui->gb_description_network, m_ui->gb_description_debug }) {
        gb->setMinimumHeight(60);
    }

    m_button_boxes = {
        m_ui->button_box_core,
        m_ui->button_box_cpu,
        m_ui->button_box_gpu,
        m_ui->button_box_audio,
        m_ui->button_box_camera,
        m_ui->button_box_system,
        m_ui->button_box_emulator,
        m_ui->button_box_gui,
        m_ui->button_box_network,
        m_ui->button_box_debug,
    };

    if (!m_app_path.empty()) {
        QString title_name = QString::fromStdString(m_app_path);
        for (const auto &app : emuenv.app.apps_list.apps) {
            if (app.title_id == m_app_path) {
                title_name = QString::fromStdString(app.title);
                break;
            }
        }
        setWindowTitle(tr("Settings - %1").arg(title_name));
    }

    m_ui->tab_widget_settings->setCurrentIndex(m_initial_tab);

    load_config();
    setup_connections();
    update_gpu_visibility();
    update_camera_widgets();
    setup_dirty_tracking();

    if (!m_app_path.empty()) {
        auto *tabs = m_ui->tab_widget_settings;
        for (int i = tabs->count() - 1; i >= 0; --i) {
            QWidget *page = tabs->widget(i);
            if (page == m_ui->camera_tab
                || page == m_ui->gui_tab) {
                tabs->setTabVisible(i, false);
            }
        }

        m_ui->show_mode->setVisible(false);
        m_ui->demo_mode->setVisible(false);

        m_ui->boot_apps_fullscreen->setVisible(false);
        m_ui->show_live_area_screen->setVisible(false);
        m_ui->show_compile_shaders->setVisible(false);
        m_ui->check_for_updates->setVisible(false);
        m_ui->log_compat_warn->setVisible(false);
        m_ui->archive_log->setVisible(false);
        m_ui->discord_rich_presence->setVisible(false);
        m_ui->gb_log_level->setVisible(false);
        m_ui->gb_performance_overlay->setVisible(false);
        m_ui->gb_storage->setVisible(false);
        m_ui->gb_screenshot->setVisible(false);

        m_ui->gb_http->setVisible(false);
        m_ui->gb_adhoc->setVisible(false);
    }

    for (auto *bb : m_button_boxes)
        bb->button(QDialogButtonBox::Apply)->setEnabled(false);
}

SettingsDialog::~SettingsDialog() = default;

void SettingsDialog::apply_text_overrides() {
    for (auto *label : { m_ui->description_core, m_ui->description_cpu,
             m_ui->description_gpu, m_ui->description_audio,
             m_ui->description_camera, m_ui->description_system,
             m_ui->description_emulator, m_ui->description_gui,
             m_ui->description_network, m_ui->description_debug }) {
        label->setTextFormat(Qt::TextFormat::PlainText);
        label->setText(m_tooltips->default_description);
    }
}

void SettingsDialog::load_config() {
    if (!m_app_path.empty()) {
        if (!config::load_custom_config(m_config, emuenv.config_path, m_app_path)) {
            Config cfg_copy;
            cfg_copy = emuenv.cfg;
            config::set_current_config(cfg_copy, emuenv.config_path, m_app_path);
            m_config = cfg_copy.current_config;
        }
    } else {
        Config cfg_copy;
        cfg_copy = emuenv.cfg;
        config::set_current_config(cfg_copy, emuenv.config_path, {});
        m_config = cfg_copy.current_config;
    }

    switch (m_config.modules_mode) {
    case ModulesMode::AUTOMATIC:
        m_ui->rb_modules_automatic->setChecked(true);
        break;
    case ModulesMode::AUTO_MANUAL:
        m_ui->rb_modules_auto_manual->setChecked(true);
        break;
    case ModulesMode::MANUAL:
        m_ui->rb_modules_manual->setChecked(true);
        break;
    }
    populate_modules_list();

    m_ui->cpu_opt->setChecked(m_config.cpu_opt);

    m_ui->backend_renderer_box->clear();
#ifndef __APPLE__
    m_ui->backend_renderer_box->addItem(QStringLiteral("OpenGL"));
#endif
    m_ui->backend_renderer_box->addItem(QStringLiteral("Vulkan"));
    {
        const int idx = m_ui->backend_renderer_box->findText(
            QString::fromStdString(m_config.backend_renderer));
        if (idx >= 0)
            m_ui->backend_renderer_box->setCurrentIndex(idx);
    }

    m_ui->gpu_device_box->clear();
    if (emuenv.vulkan_device_info) {
        for (const auto &gpu : emuenv.vulkan_device_info->gpu_names)
            m_ui->gpu_device_box->addItem(QString::fromStdString(gpu));
    }

    {
        const int gpu_count = m_ui->gpu_device_box->count();
        const int idx = (m_config.gpu_idx >= 0 && m_config.gpu_idx < gpu_count)
            ? m_config.gpu_idx
            : 0;
        m_ui->gpu_device_box->setCurrentIndex(idx);
    }

    m_ui->renderer_accuracy_box->clear();
    m_ui->renderer_accuracy_box->addItem(tr("Standard"));
    m_ui->renderer_accuracy_box->addItem(tr("High"));
    m_ui->renderer_accuracy_box->setCurrentIndex(m_config.high_accuracy ? 1 : 0);

    m_ui->vsync->setChecked(m_config.v_sync);
    m_ui->disable_surface_sync->setChecked(m_config.disable_surface_sync);
    m_ui->async_pipeline_compilation->setChecked(m_config.async_pipeline_compilation);

    {
        m_ui->memory_mapping_box->clear();

        struct MappingEntry {
            const char *label;
            const char *value;
            int bit;
        };
        static const MappingEntry all_methods[] = {
            { "Disabled", "disabled", 0 },
            { "Double Buffer", "double-buffer", 1 },
            { "External Host", "external-host", 2 },
            { "Page Table", "page-table", 3 },
            { "Native Buffer", "native-buffer", 4 },
        };

        const int gpu_idx = m_config.gpu_idx;
        const int mask = app::get_supported_memory_mapping_mask(emuenv, gpu_idx);

        int current_idx = 0;
        int added = 0;
        for (const auto &m : all_methods) {
            if ((1 << m.bit) & mask) {
                m_ui->memory_mapping_box->addItem(tr(m.label), QString::fromLatin1(m.value));
                if (m_config.memory_mapping == m.value)
                    current_idx = added;
                added++;
            }
        }
        m_ui->memory_mapping_box->setCurrentIndex(current_idx);
    }

    m_ui->resolution_upscale->setMinimum(2);
    m_ui->resolution_upscale->setMaximum(32);
    m_ui->resolution_upscale->setSingleStep(1);
    m_ui->resolution_upscale->setValue(static_cast<int>(m_config.resolution_multiplier * 4.0f));
    update_resolution_label();

    m_ui->anisotropic_filter->setMinimum(0);
    m_ui->anisotropic_filter->setMaximum(4);
    m_ui->anisotropic_filter->setSingleStep(1);
    m_ui->anisotropic_filter->setValue(
        static_cast<int>(log2f(static_cast<float>(m_config.anisotropic_filtering))));
    update_aniso_label();

    m_ui->export_textures->setChecked(m_config.export_textures);
    m_ui->import_textures->setChecked(m_config.import_textures);

    m_ui->texture_export_format->clear();
    m_ui->texture_export_format->addItems({ QStringLiteral("PNG"), QStringLiteral("DDS") });
    m_ui->texture_export_format->setCurrentIndex(m_config.export_as_png ? 0 : 1);

    m_ui->shader_cache->setChecked(m_config.shader_cache);
    m_ui->spirv_shader->setChecked(m_config.spirv_shader);
    m_ui->fps_hack->setChecked(m_config.fps_hack);

    m_ui->audio_backend_box->clear();
    m_ui->audio_backend_box->addItems({ QStringLiteral("SDL"), QStringLiteral("Cubeb") });
    m_ui->audio_backend_box->setCurrentIndex(m_config.audio_backend == "Cubeb" ? 1 : 0);

    m_ui->audio_volume->setValue(m_config.audio_volume);
    m_ui->audio_volume_label->setText(tr("Current volume: %1%").arg(m_config.audio_volume));

    m_ui->ngs_enable->setChecked(m_config.ngs_enable);

    app::ensure_camera_defaults(emuenv.cfg);
    populate_cameras_list();

    if (m_config.sys_button == 0)
        m_ui->enter_button_circle->setChecked(true);
    else
        m_ui->enter_button_cross->setChecked(true);

    m_ui->pstv_mode->setChecked(m_config.pstv_mode);
    m_ui->show_mode->setChecked(emuenv.cfg.show_mode);
    m_ui->demo_mode->setChecked(emuenv.cfg.demo_mode);

    m_ui->boot_apps_fullscreen->setChecked(emuenv.cfg.boot_apps_full_screen);
    m_ui->show_live_area_screen->setChecked(emuenv.cfg.show_live_area_screen);
    m_ui->show_compile_shaders->setChecked(emuenv.cfg.show_compile_shaders);
    m_ui->check_for_updates->setChecked(emuenv.cfg.check_for_updates);
    m_ui->log_compat_warn->setChecked(emuenv.cfg.log_compat_warn);
    m_ui->texture_cache->setChecked(m_config.texture_cache);
    m_ui->archive_log->setChecked(emuenv.cfg.archive_log);
#ifdef USE_DISCORD
    m_ui->discord_rich_presence->setChecked(emuenv.cfg.discord_rich_presence);
#else
    m_ui->discord_rich_presence->setVisible(false);
#endif

    m_ui->log_level_box->clear();
    m_ui->log_level_box->addItems({ tr("Trace"), tr("Debug"), tr("Info"),
        tr("Warning"), tr("Error"), tr("Critical"), tr("Off") });
    m_ui->log_level_box->setCurrentIndex(emuenv.cfg.log_level);

    m_ui->perf_overlay_enabled->setChecked(emuenv.cfg.performance_overlay);
    m_ui->perf_overlay_detail_box->clear();
    m_ui->perf_overlay_detail_box->addItems({ tr("Minimum"), tr("Low"), tr("Medium"), tr("Maximum") });
    m_ui->perf_overlay_detail_box->setCurrentIndex(emuenv.cfg.performance_overlay_detail);
    m_ui->perf_overlay_position_box->clear();
    m_ui->perf_overlay_position_box->addItems({ tr("Top Left"), tr("Top Center"), tr("Top Right"),
        tr("Bottom Left"), tr("Bottom Center"), tr("Bottom Right") });
    m_ui->perf_overlay_position_box->setCurrentIndex(emuenv.cfg.performance_overlay_position);

    m_ui->perf_overlay_detail->setEnabled(emuenv.cfg.performance_overlay);
    m_ui->perf_overlay_position->setEnabled(emuenv.cfg.performance_overlay);

    m_ui->current_emu_path->setText(
        tr("Current emulator path: %1").arg(QString::fromStdString(emuenv.cfg.pref_path)));

    m_ui->screenshot_format->clear();
    m_ui->screenshot_format->addItems({ tr("None"), QStringLiteral("JPEG"), QStringLiteral("PNG") });
    m_ui->screenshot_format->setCurrentIndex(emuenv.cfg.screenshot_format);

    m_ui->stretch_the_display_area->setChecked(m_config.stretch_the_display_area);
    m_ui->fullscreen_hd_res_pixel_perfect->setChecked(m_config.fullscreen_hd_res_pixel_perfect);

    m_ui->file_loading_delay->setMinimum(0);
    m_ui->file_loading_delay->setMaximum(30);
    m_ui->file_loading_delay->setValue(m_config.file_loading_delay);
    update_file_loading_delay_label();

    m_ui->sys_lang_box->clear();
    m_ui->sys_lang_box->addItems({ tr("Japanese"), tr("English (US)"), tr("French"), tr("Spanish"),
        tr("German"), tr("Italian"), tr("Dutch"), tr("Portuguese (PT)"),
        tr("Russian"), tr("Korean"), tr("Chinese (Traditional)"),
        tr("Chinese (Simplified)"), tr("Finnish"), tr("Swedish"),
        tr("Danish"), tr("Norwegian"), tr("Polish"), tr("Portuguese (BR)"),
        tr("English (GB)"), tr("Turkish") });
    m_ui->sys_lang_box->setCurrentIndex(m_config.sys_lang);

    m_ui->sys_date_format_box->clear();
    m_ui->sys_date_format_box->addItems({ tr("YYYY/MM/DD"), tr("DD/MM/YYYY"), tr("MM/DD/YYYY") });
    m_ui->sys_date_format_box->setCurrentIndex(m_config.sys_date_format);

    m_ui->sys_time_format_box->clear();
    m_ui->sys_time_format_box->addItems({ tr("12-Hour"), tr("24-Hour") });
    m_ui->sys_time_format_box->setCurrentIndex(m_config.sys_time_format);

    {
        struct ImeLang {
            const char *name;
            uint64_t flag;
        };
        static const ImeLang ime_lang_list[] = {
            { "Danish", 0x00000001ULL },
            { "German", 0x00000002ULL },
            { "English (US)", 0x00000004ULL },
            { "Spanish", 0x00000008ULL },
            { "French", 0x00000010ULL },
            { "Italian", 0x00000020ULL },
            { "Dutch", 0x00000040ULL },
            { "Norwegian", 0x00000080ULL },
            { "Polish", 0x00000100ULL },
            { "Portuguese (PT)", 0x00000200ULL },
            { "Russian", 0x00000400ULL },
            { "Finnish", 0x00000800ULL },
            { "Swedish", 0x00001000ULL },
            { "Japanese", 0x00002000ULL },
            { "Korean", 0x00004000ULL },
            { "Simplified Chinese", 0x00008000ULL },
            { "Traditional Chinese", 0x00010000ULL },
            { "Portuguese (BR)", 0x00020000ULL },
            { "English (GB)", 0x00040000ULL },
            { "Turkish", 0x00080000ULL },
        };
        m_ui->ime_langs_list->clear();

        uint64_t enabled_mask = 0;
        for (const auto &v : m_config.ime_langs)
            enabled_mask |= v;
        for (const auto &lang : ime_lang_list) {
            auto *item = new QListWidgetItem(tr(lang.name), m_ui->ime_langs_list);
            item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
            item->setCheckState((enabled_mask & lang.flag) ? Qt::Checked : Qt::Unchecked);
            item->setData(Qt::UserRole, QVariant::fromValue(lang.flag));
        }
    }

    m_ui->psn_signed_in->setChecked(m_config.psn_signed_in);
    m_ui->http_enable->setChecked(emuenv.cfg.http_enable);
    m_ui->http_timeout_attempts->setValue(emuenv.cfg.http_timeout_attempts);
    m_ui->http_timeout_sleep->setValue(emuenv.cfg.http_timeout_sleep_ms);
    m_ui->http_read_end_attempts->setValue(emuenv.cfg.http_read_end_attempts);
    m_ui->http_read_end_sleep->setValue(emuenv.cfg.http_read_end_sleep_ms);
    populate_adhoc_list();

    m_ui->show_welcome->setChecked(emuenv.cfg.show_welcome);
    m_ui->warn_missing_firmware->setChecked(emuenv.cfg.warn_missing_firmware);
    m_ui->ui_language_box->clear();
    m_ui->ui_language_box->addItem(tr("System Default"), QStringLiteral(""));
    for (const auto &language : gui::i18n::ui_language_options()) {
        const QString tag = QString::fromUtf8(language.tag.data(), static_cast<int>(language.tag.size()));
        const QString native_name = QString::fromUtf8(language.native_name.data(), static_cast<int>(language.native_name.size()));
        m_ui->ui_language_box->addItem(native_name, tag);
    }
    {
        const QString current_lang = QString::fromStdString(emuenv.cfg.user_lang);
        const int index = m_ui->ui_language_box->findData(current_lang);
        m_ui->ui_language_box->setCurrentIndex(index >= 0 ? index : 0);
    }
    if (m_gui_settings) {
        const int buf_size = m_gui_settings->get_value(gui::l_bufferSize).toInt();
        m_ui->log_buffer_size->setValue(buf_size);
    }

    add_stylesheets();

    m_ui->log_imports->setChecked(emuenv.kernel.debugger.log_imports);
    m_ui->log_exports->setChecked(emuenv.kernel.debugger.log_exports);
    m_ui->log_active_shaders->setChecked(m_config.log_active_shaders);
    m_ui->log_uniforms->setChecked(m_config.log_uniforms);
    m_ui->color_surface_debug->setChecked(m_config.color_surface_debug);
    m_ui->validation_layer->setChecked(m_config.validation_layer);
    m_ui->dump_elfs->setChecked(emuenv.kernel.debugger.dump_elfs);
}

void SettingsDialog::save_config() {
    const std::string previous_user_lang = emuenv.cfg.user_lang;

    if (m_ui->rb_modules_automatic->isChecked())
        m_config.modules_mode = ModulesMode::AUTOMATIC;
    else if (m_ui->rb_modules_auto_manual->isChecked())
        m_config.modules_mode = ModulesMode::AUTO_MANUAL;
    else
        m_config.modules_mode = ModulesMode::MANUAL;

    m_config.lle_modules.clear();
    for (int i = 0; i < m_ui->modules_list->count(); ++i) {
        auto *item = m_ui->modules_list->item(i);
        if (item->checkState() == Qt::Checked)
            m_config.lle_modules.push_back(item->text().toStdString());
    }

    m_config.cpu_opt = m_ui->cpu_opt->isChecked();

    m_config.backend_renderer = m_ui->backend_renderer_box->currentText().toStdString();
    m_config.high_accuracy = m_ui->renderer_accuracy_box->currentIndex() == 1;
    m_config.v_sync = m_ui->vsync->isChecked();
    m_config.disable_surface_sync = m_ui->disable_surface_sync->isChecked();
    m_config.async_pipeline_compilation = m_ui->async_pipeline_compilation->isChecked();
    m_config.memory_mapping = m_ui->memory_mapping_box->currentData().toString().toStdString();
    m_config.screen_filter = m_ui->screen_filter_box->currentText().toStdString();
    m_config.resolution_multiplier = static_cast<float>(m_ui->resolution_upscale->value()) / 4.0f;
    m_config.anisotropic_filtering = 1 << m_ui->anisotropic_filter->value();
    m_config.export_textures = m_ui->export_textures->isChecked();
    m_config.import_textures = m_ui->import_textures->isChecked();
    m_config.export_as_png = m_ui->texture_export_format->currentIndex() == 0;
    m_config.fps_hack = m_ui->fps_hack->isChecked();

    m_config.shader_cache = m_ui->shader_cache->isChecked();
    m_config.spirv_shader = m_ui->spirv_shader->isChecked();
    m_config.gpu_idx = m_ui->gpu_device_box->currentIndex();

    m_config.audio_backend = m_ui->audio_backend_box->currentText().toStdString();
    m_config.audio_volume = m_ui->audio_volume->value();
    m_config.ngs_enable = m_ui->ngs_enable->isChecked();

    {
        const int front_idx = m_ui->front_camera_box->currentIndex();
        if (front_idx == 0) {
            emuenv.cfg.front_camera_type = 0;
        } else if (front_idx == 1) {
            emuenv.cfg.front_camera_type = 1;
        } else {
            emuenv.cfg.front_camera_type = 2;
            emuenv.cfg.front_camera_id = m_ui->front_camera_box->currentText().toStdString();
        }

        const int back_idx = m_ui->back_camera_box->currentIndex();
        if (back_idx == 0) {
            emuenv.cfg.back_camera_type = 0;
        } else if (back_idx == 1) {
            emuenv.cfg.back_camera_type = 1;
        } else {
            emuenv.cfg.back_camera_type = 2;
            emuenv.cfg.back_camera_id = m_ui->back_camera_box->currentText().toStdString();
        }
    }

    m_config.sys_button = m_ui->enter_button_cross->isChecked() ? 1 : 0;
    m_config.pstv_mode = m_ui->pstv_mode->isChecked();
    emuenv.cfg.show_mode = m_ui->show_mode->isChecked();
    emuenv.cfg.demo_mode = m_ui->demo_mode->isChecked();

    emuenv.cfg.boot_apps_full_screen = m_ui->boot_apps_fullscreen->isChecked();
    emuenv.cfg.show_live_area_screen = m_ui->show_live_area_screen->isChecked();
    emuenv.cfg.show_compile_shaders = m_ui->show_compile_shaders->isChecked();
    emuenv.cfg.check_for_updates = m_ui->check_for_updates->isChecked();
    emuenv.cfg.log_compat_warn = m_ui->log_compat_warn->isChecked();
    emuenv.compat.log_compat_warn = emuenv.cfg.log_compat_warn;
    m_config.texture_cache = m_ui->texture_cache->isChecked();
    emuenv.cfg.archive_log = m_ui->archive_log->isChecked();
#ifdef USE_DISCORD
    emuenv.cfg.discord_rich_presence = m_ui->discord_rich_presence->isChecked();
#endif

    const int new_log_level = m_ui->log_level_box->currentIndex();
    if (new_log_level != emuenv.cfg.log_level) {
        emuenv.cfg.log_level = new_log_level;
        logging::set_level(static_cast<spdlog::level::level_enum>(new_log_level));
    }

    emuenv.cfg.performance_overlay = m_ui->perf_overlay_enabled->isChecked();
    emuenv.cfg.performance_overlay_detail = m_ui->perf_overlay_detail_box->currentIndex();
    emuenv.cfg.performance_overlay_position = m_ui->perf_overlay_position_box->currentIndex();
    emuenv.cfg.screenshot_format = m_ui->screenshot_format->currentIndex();

    m_config.stretch_the_display_area = m_ui->stretch_the_display_area->isChecked();
    m_config.fullscreen_hd_res_pixel_perfect = m_ui->fullscreen_hd_res_pixel_perfect->isChecked();
    m_config.file_loading_delay = m_ui->file_loading_delay->value();

    m_config.sys_lang = m_ui->sys_lang_box->currentIndex();
    m_config.sys_date_format = m_ui->sys_date_format_box->currentIndex();
    m_config.sys_time_format = m_ui->sys_time_format_box->currentIndex();

    {
        m_config.ime_langs.clear();
        for (int i = 0; i < m_ui->ime_langs_list->count(); ++i) {
            auto *item = m_ui->ime_langs_list->item(i);
            if (item->checkState() == Qt::Checked) {
                const uint64_t flag = item->data(Qt::UserRole).toULongLong();
                m_config.ime_langs.push_back(flag);
            }
        }
        if (m_config.ime_langs.empty())
            m_config.ime_langs.push_back(0x00000004ULL); // English (US)
    }

    m_config.psn_signed_in = m_ui->psn_signed_in->isChecked();
    emuenv.cfg.http_enable = m_ui->http_enable->isChecked();
    emuenv.cfg.http_timeout_attempts = m_ui->http_timeout_attempts->value();
    emuenv.cfg.http_timeout_sleep_ms = m_ui->http_timeout_sleep->value();
    emuenv.cfg.http_read_end_attempts = m_ui->http_read_end_attempts->value();
    emuenv.cfg.http_read_end_sleep_ms = m_ui->http_read_end_sleep->value();
    emuenv.cfg.adhoc_addr = m_ui->adhoc_address_box->currentIndex();

    emuenv.cfg.show_welcome = m_ui->show_welcome->isChecked();
    emuenv.cfg.warn_missing_firmware = m_ui->warn_missing_firmware->isChecked();
    emuenv.cfg.user_lang = m_ui->ui_language_box->currentData().toString().toStdString();
    if (m_gui_settings) {
        m_gui_settings->set_value(gui::l_bufferSize, m_ui->log_buffer_size->value());
    }

    emuenv.kernel.debugger.log_imports = m_ui->log_imports->isChecked();
    emuenv.kernel.debugger.log_exports = m_ui->log_exports->isChecked();
    m_config.log_active_shaders = m_ui->log_active_shaders->isChecked();
    m_config.log_uniforms = m_ui->log_uniforms->isChecked();
    m_config.color_surface_debug = m_ui->color_surface_debug->isChecked();
    m_config.validation_layer = m_ui->validation_layer->isChecked();
    emuenv.kernel.debugger.dump_elfs = m_ui->dump_elfs->isChecked();

    const bool applies_to_active_profile_now = applies_to_active_profile();
    const auto previous_current = emuenv.cfg.current_config;

    if (!m_app_path.empty()) {
        if (applies_to_active_profile_now)
            emuenv.cfg.current_config = m_config;
        config::save_custom_config(m_config, emuenv.config_path, m_app_path);
    } else {
        emuenv.cfg.current_config = m_config;
        config::copy_current_config_to_global(emuenv.cfg);
        if (!applies_to_active_profile_now)
            emuenv.cfg.current_config = previous_current;
    }
    config::serialize_config(emuenv.cfg, emuenv.cfg.config_path);

    if (m_app_path.empty() && previous_user_lang != emuenv.cfg.user_lang)
        Q_EMIT ui_language_request(QString::fromStdString(emuenv.cfg.user_lang));
}

void SettingsDialog::apply_config() {
    if (!applies_to_active_profile())
        return;

    emuenv.cfg.current_config = m_config;
    emuenv.audio.set_global_volume(m_config.audio_volume / 100.f);
    if (emuenv.renderer)
        app::apply_renderer_config(emuenv);
}

void SettingsDialog::populate_modules_list() {
    m_ui->modules_list->clear();
    const auto modules = config::get_modules_list(emuenv.pref_path, m_config.lle_modules);
    for (const auto &[name, enabled] : modules) {
        auto *item = new QListWidgetItem(QString::fromStdString(name), m_ui->modules_list);
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        item->setCheckState(enabled ? Qt::Checked : Qt::Unchecked);
    }
    update_modules_list_enabled();
}

void SettingsDialog::update_modules_list_enabled() {
    const bool automatic = m_ui->rb_modules_automatic->isChecked();
    m_ui->modules_list->setEnabled(!automatic);
    m_ui->modules_search_bar->setEnabled(!automatic);
    m_ui->clear_modules_list->setEnabled(!automatic);
    m_ui->refresh_modules_list->setEnabled(!automatic);
}

void SettingsDialog::populate_cameras_list() {
    m_ui->front_camera_box->clear();
    m_ui->back_camera_box->clear();

    m_ui->front_camera_box->addItem(tr("Solid Color"));
    m_ui->front_camera_box->addItem(tr("Static Image"));
    m_ui->back_camera_box->addItem(tr("Solid Color"));
    m_ui->back_camera_box->addItem(tr("Static Image"));

    for (const auto &camera_name : app::get_available_camera_names()) {
        const auto camera_text = QString::fromStdString(camera_name);
        m_ui->front_camera_box->addItem(camera_text);
        m_ui->back_camera_box->addItem(camera_text);
    }

    if (emuenv.cfg.front_camera_type == 0) {
        m_ui->front_camera_box->setCurrentIndex(0);
    } else if (emuenv.cfg.front_camera_type == 1) {
        m_ui->front_camera_box->setCurrentIndex(1);
    } else {
        const int idx = m_ui->front_camera_box->findText(
            QString::fromStdString(emuenv.cfg.front_camera_id));
        m_ui->front_camera_box->setCurrentIndex(idx >= 0 ? idx : 0);
    }

    if (emuenv.cfg.back_camera_type == 0) {
        m_ui->back_camera_box->setCurrentIndex(0);
    } else if (emuenv.cfg.back_camera_type == 1) {
        m_ui->back_camera_box->setCurrentIndex(1);
    } else {
        const int idx = m_ui->back_camera_box->findText(
            QString::fromStdString(emuenv.cfg.back_camera_id));
        m_ui->back_camera_box->setCurrentIndex(idx >= 0 ? idx : 0);
    }

    m_ui->front_camera_image_path->setText(tr("No image selected"));
    m_ui->back_camera_image_path->setText(tr("No image selected"));

    if (!emuenv.cfg.front_camera_image.empty())
        m_ui->front_camera_image_path->setText(QString::fromStdString(emuenv.cfg.front_camera_image));
    if (!emuenv.cfg.back_camera_image.empty())
        m_ui->back_camera_image_path->setText(QString::fromStdString(emuenv.cfg.back_camera_image));
}

void SettingsDialog::populate_adhoc_list() {
    m_ui->adhoc_address_box->clear();
    m_ui->subnet_mask_box->clear();

    const auto addrs = app::get_available_adhoc_address_options();
    for (const auto &addr : addrs) {
        m_ui->adhoc_address_box->addItem(QString::fromStdString(addr.label));
        m_ui->subnet_mask_box->addItem(QString::fromStdString(addr.subnet_mask));
    }

    if (emuenv.cfg.adhoc_addr >= static_cast<int>(addrs.size())) {
        emuenv.cfg.adhoc_addr = 0;
        LOG_WARN("Invalid adhoc address index, resetting to 0");
    }

    if (!addrs.empty()) {
        m_ui->adhoc_address_box->setCurrentIndex(emuenv.cfg.adhoc_addr);
        m_ui->subnet_mask_box->setCurrentIndex(emuenv.cfg.adhoc_addr);
    }
}

void SettingsDialog::update_camera_widgets() {
    const int front_idx = m_ui->front_camera_box->currentIndex();
    m_ui->label_front_camera_color->setVisible(front_idx == 0);
    m_ui->front_camera_color_preview->setVisible(front_idx == 0);
    m_ui->front_camera_color_btn->setVisible(front_idx == 0);
    m_ui->label_front_camera_image->setVisible(front_idx == 1);
    m_ui->front_camera_image_path->setVisible(front_idx == 1);
    m_ui->front_camera_image_btn->setVisible(front_idx == 1);

    const int back_idx = m_ui->back_camera_box->currentIndex();
    m_ui->label_back_camera_color->setVisible(back_idx == 0);
    m_ui->back_camera_color_preview->setVisible(back_idx == 0);
    m_ui->back_camera_color_btn->setVisible(back_idx == 0);
    m_ui->label_back_camera_image->setVisible(back_idx == 1);
    m_ui->back_camera_image_path->setVisible(back_idx == 1);
    m_ui->back_camera_image_btn->setVisible(back_idx == 1);

    update_camera_color_preview();
}

void SettingsDialog::update_camera_color_preview() {
    const QColor front_color = QColor::fromRgba(emuenv.cfg.front_camera_color);
    m_ui->front_camera_color_preview->setStyleSheet(
        QStringLiteral("background-color: %1; border: 1px solid #888;").arg(front_color.name()));

    const QColor back_color = QColor::fromRgba(emuenv.cfg.back_camera_color);
    m_ui->back_camera_color_preview->setStyleSheet(
        QStringLiteral("background-color: %1; border: 1px solid #888;").arg(back_color.name()));
}

void SettingsDialog::update_file_loading_delay_label() {
    m_ui->file_loading_delay_label->setText(
        tr("File Loading Delay: %1 ms").arg(m_ui->file_loading_delay->value()));
}

bool SettingsDialog::applies_to_active_profile() const {
    if (!m_app_path.empty())
        return m_app_path == emuenv.io.app_path;

    return emuenv.io.app_path.empty()
        || !config::has_custom_config(emuenv.config_path, emuenv.io.app_path);
}

void SettingsDialog::setup_connections() {
    for (auto *bb : m_button_boxes) {
        connect(bb->button(QDialogButtonBox::Save), &QPushButton::clicked,
            this, &SettingsDialog::on_save_clicked);
        connect(bb->button(QDialogButtonBox::Apply), &QPushButton::clicked,
            this, &SettingsDialog::on_apply_clicked);
        connect(bb->button(QDialogButtonBox::Close), &QPushButton::clicked,
            this, &SettingsDialog::on_close_clicked);
    }

    connect(m_ui->backend_renderer_box, &QComboBox::currentIndexChanged,
        this, &SettingsDialog::update_gpu_visibility);

    connect(m_ui->btn_apply_stylesheet, &QPushButton::clicked, this, [this]() {
        apply_stylesheet();
    });

    connect(m_ui->rb_modules_automatic, &QRadioButton::toggled,
        this, &SettingsDialog::update_modules_list_enabled);
    connect(m_ui->rb_modules_auto_manual, &QRadioButton::toggled,
        this, &SettingsDialog::update_modules_list_enabled);
    connect(m_ui->rb_modules_manual, &QRadioButton::toggled,
        this, &SettingsDialog::update_modules_list_enabled);

    connect(m_ui->resolution_upscale, &QSlider::valueChanged,
        this, &SettingsDialog::update_resolution_label);
    connect(m_ui->resolution_upscale_reset, &QPushButton::clicked, this, [this] {
        m_ui->resolution_upscale->setValue(4); // 1.0x
    });

    connect(m_ui->anisotropic_filter, &QSlider::valueChanged,
        this, &SettingsDialog::update_aniso_label);
    connect(m_ui->anisotropic_filter_reset, &QPushButton::clicked, this, [this] {
        m_ui->anisotropic_filter->setValue(0); // 1x
    });

    connect(m_ui->audio_volume, &QSlider::valueChanged, this, [this](int val) {
        m_ui->audio_volume_label->setText(tr("Current volume: %1%").arg(val));
    });

    connect(m_ui->perf_overlay_enabled, &QCheckBox::toggled, this, [this](bool on) {
        m_ui->perf_overlay_detail->setEnabled(on);
        m_ui->perf_overlay_position->setEnabled(on);
    });

    connect(m_ui->file_loading_delay, &QSlider::valueChanged,
        this, &SettingsDialog::update_file_loading_delay_label);

    connect(m_ui->modules_search_bar, &QLineEdit::textChanged, this, [this](const QString &text) {
        for (int i = 0; i < m_ui->modules_list->count(); ++i) {
            auto *item = m_ui->modules_list->item(i);
            item->setHidden(!item->text().contains(text, Qt::CaseInsensitive));
        }
    });

    connect(m_ui->clear_modules_list, &QPushButton::clicked, this, [this] {
        for (int i = 0; i < m_ui->modules_list->count(); ++i)
            m_ui->modules_list->item(i)->setCheckState(Qt::Unchecked);
    });
    connect(m_ui->refresh_modules_list, &QPushButton::clicked,
        this, &SettingsDialog::populate_modules_list);

    connect(m_ui->front_camera_box, &QComboBox::currentIndexChanged,
        this, &SettingsDialog::update_camera_widgets);
    connect(m_ui->back_camera_box, &QComboBox::currentIndexChanged,
        this, &SettingsDialog::update_camera_widgets);

    connect(m_ui->front_camera_color_btn, &QPushButton::clicked, this, [this] {
        const QColor color = QColorDialog::getColor(
            QColor::fromRgba(emuenv.cfg.front_camera_color), this, tr("Front Camera Color"));
        if (color.isValid()) {
            emuenv.cfg.front_camera_color = color.rgba();
            update_camera_color_preview();
        }
    });
    connect(m_ui->back_camera_color_btn, &QPushButton::clicked, this, [this] {
        const QColor color = QColorDialog::getColor(
            QColor::fromRgba(emuenv.cfg.back_camera_color), this, tr("Back Camera Color"));
        if (color.isValid()) {
            emuenv.cfg.back_camera_color = color.rgba();
            update_camera_color_preview();
        }
    });

    connect(m_ui->front_camera_image_btn, &QPushButton::clicked, this, [this] {
        const QString file = QFileDialog::getOpenFileName(
            this, tr("Select Front Camera Image"),
            QString::fromStdString(emuenv.cfg.front_camera_image),
            tr("Images (*.png *.jpg *.jpeg *.bmp)"));
        if (!file.isEmpty()) {
            emuenv.cfg.front_camera_image = file.toStdString();
            m_ui->front_camera_image_path->setText(file);
        }
    });
    connect(m_ui->back_camera_image_btn, &QPushButton::clicked, this, [this] {
        const QString file = QFileDialog::getOpenFileName(
            this, tr("Select Back Camera Image"),
            QString::fromStdString(emuenv.cfg.back_camera_image),
            tr("Images (*.png *.jpg *.jpeg *.bmp)"));
        if (!file.isEmpty()) {
            emuenv.cfg.back_camera_image = file.toStdString();
            m_ui->back_camera_image_path->setText(file);
        }
    });

    connect(m_ui->adhoc_address_box, &QComboBox::currentIndexChanged, this, [this](int idx) {
        m_ui->subnet_mask_box->setCurrentIndex(idx);
    });

    connect(m_ui->change_emu_path, &QPushButton::clicked, this, [this] {
        const QString dir = QFileDialog::getExistingDirectory(
            this, tr("Select Emulator Storage Folder"),
            QString::fromStdString(emuenv.cfg.pref_path));
        if (!dir.isEmpty()) {
            if (!app::switch_emulator_path(emuenv, fs::path(dir.toStdString()))) {
                QMessageBox::critical(this, tr("Error"),
                    tr("Failed to switch the emulator storage folder."));
                return;
            }

            m_storage_path_switched = true;
            populate_modules_list();
            m_ui->current_emu_path->setText(
                tr("Current emulator path: %1").arg(QString::fromStdString(emuenv.cfg.pref_path)));
        }
    });
    connect(m_ui->reset_emu_path, &QPushButton::clicked, this, [this] {
        if (emuenv.default_path != emuenv.pref_path) {
            if (!app::switch_emulator_path(emuenv, emuenv.default_path)) {
                QMessageBox::critical(this, tr("Error"),
                    tr("Failed to reset the emulator storage folder."));
                return;
            }

            m_storage_path_switched = true;
            populate_modules_list();
            m_ui->current_emu_path->setText(
                tr("Current emulator path: %1").arg(QString::fromStdString(emuenv.cfg.pref_path)));
        }
    });

    connect(m_ui->clear_custom_config, &QPushButton::clicked, this, [this] {
        const auto config_dir = emuenv.config_path / "config";
        if (fs::exists(config_dir)) {
            fs::remove_all(config_dir);
            LOG_INFO("Cleared all custom config settings.");
        }
    });

    connect(m_ui->clean_shaders_cache, &QPushButton::clicked, this, [this] {
        const auto shaders_cache_path = emuenv.cache_path / "shaders";
        fs::remove_all(shaders_cache_path);
        fs::remove_all(emuenv.cache_path / "shaderlog");
        fs::remove_all(emuenv.log_path / "shaderlog");
        LOG_INFO("Shaders cache cleaned.");
    });

    connect(m_ui->watch_code, &QPushButton::clicked, this, [this] {
        emuenv.kernel.debugger.watch_code = !emuenv.kernel.debugger.watch_code;
        emuenv.kernel.debugger.update_watches();
        m_ui->watch_code->setText(emuenv.kernel.debugger.watch_code ? tr("Unwatch Code") : tr("Watch Code"));
    });
    connect(m_ui->watch_memory, &QPushButton::clicked, this, [this] {
        emuenv.kernel.debugger.watch_memory = !emuenv.kernel.debugger.watch_memory;
        emuenv.kernel.debugger.update_watches();
        m_ui->watch_memory->setText(emuenv.kernel.debugger.watch_memory ? tr("Unwatch Memory") : tr("Watch Memory"));
    });
    connect(m_ui->watch_import_calls, &QPushButton::clicked, this, [this] {
        emuenv.kernel.debugger.watch_import_calls = !emuenv.kernel.debugger.watch_import_calls;
        emuenv.kernel.debugger.update_watches();
        m_ui->watch_import_calls->setText(emuenv.kernel.debugger.watch_import_calls ? tr("Unwatch Import Calls") : tr("Watch Import Calls"));
    });

    m_ui->watch_code->setText(emuenv.kernel.debugger.watch_code ? tr("Unwatch Code") : tr("Watch Code"));
    m_ui->watch_memory->setText(emuenv.kernel.debugger.watch_memory ? tr("Unwatch Memory") : tr("Watch Memory"));
    m_ui->watch_import_calls->setText(emuenv.kernel.debugger.watch_import_calls ? tr("Unwatch Import Calls") : tr("Watch Import Calls"));

    const std::vector<std::pair<QWidget *, QString>> desc_widgets = {
        // Core
        { m_ui->rb_modules_automatic, m_tooltips->modules_automatic },
        { m_ui->rb_modules_auto_manual, m_tooltips->modules_auto_manual },
        { m_ui->rb_modules_manual, m_tooltips->modules_manual },
        // CPU
        { m_ui->cpu_opt, m_tooltips->cpu_opt },
        // GPU
        { m_ui->backend_renderer_box, m_tooltips->backend_renderer },
        { m_ui->renderer_accuracy_box, m_tooltips->renderer_accuracy },
        { m_ui->vsync, m_tooltips->vsync },
        { m_ui->disable_surface_sync, m_tooltips->disable_surface_sync },
        { m_ui->async_pipeline_compilation, m_tooltips->async_pipeline },
        { m_ui->memory_mapping_box, m_tooltips->memory_mapping },
        { m_ui->screen_filter_box, m_tooltips->screen_filter },
        { m_ui->resolution_upscale, m_tooltips->resolution_upscaling },
        { m_ui->anisotropic_filter, m_tooltips->anisotropic_filtering },
        { m_ui->export_textures, m_tooltips->export_textures },
        { m_ui->import_textures, m_tooltips->import_textures },
        { m_ui->shader_cache, m_tooltips->shader_cache },
        { m_ui->spirv_shader, m_tooltips->spirv_shader },
        { m_ui->fps_hack, m_tooltips->fps_hack },
        // Audio
        { m_ui->audio_backend_box, m_tooltips->audio_backend },
        { m_ui->audio_volume, m_tooltips->audio_volume },
        { m_ui->ngs_enable, m_tooltips->ngs_enable },
        // Camera
        { m_ui->front_camera_box, m_tooltips->front_camera },
        { m_ui->back_camera_box, m_tooltips->back_camera },
        // System
        { m_ui->enter_button_circle, m_tooltips->enter_button },
        { m_ui->enter_button_cross, m_tooltips->enter_button },
        { m_ui->pstv_mode, m_tooltips->pstv_mode },
        { m_ui->show_mode, m_tooltips->show_mode },
        { m_ui->demo_mode, m_tooltips->demo_mode },
        { m_ui->sys_lang_box, m_tooltips->sys_lang },
        { m_ui->sys_date_format_box, m_tooltips->sys_date_format },
        { m_ui->sys_time_format_box, m_tooltips->sys_time_format },
        { m_ui->ime_langs_list, m_tooltips->ime_langs },
        // Emulator
        { m_ui->boot_apps_fullscreen, m_tooltips->boot_fullscreen },
        { m_ui->show_live_area_screen, m_tooltips->show_live_area },
        { m_ui->show_compile_shaders, m_tooltips->show_compile_shaders },
        { m_ui->check_for_updates, m_tooltips->check_for_updates },
        { m_ui->log_compat_warn, m_tooltips->log_compat_warn },
        { m_ui->texture_cache, m_tooltips->texture_cache },
        { m_ui->archive_log, m_tooltips->archive_log },
        { m_ui->discord_rich_presence, m_tooltips->discord_rpc },
        { m_ui->perf_overlay_enabled, m_tooltips->perf_overlay },
        { m_ui->stretch_the_display_area, m_tooltips->stretch_display },
        { m_ui->fullscreen_hd_res_pixel_perfect, m_tooltips->pixel_perfect },
        { m_ui->file_loading_delay, m_tooltips->file_loading_delay },
        // GUI
        { m_ui->show_welcome, m_tooltips->show_welcome_screen },
        { m_ui->warn_missing_firmware, m_tooltips->warn_missing_firmware },
        { m_ui->ui_language_box, m_tooltips->ui_language },
        { m_ui->log_buffer_size, m_tooltips->log_buffer_size },
        { m_ui->combo_stylesheets, m_tooltips->gui_stylesheet },
        // Network
        { m_ui->psn_signed_in, m_tooltips->psn_signed_in },
        { m_ui->http_enable, m_tooltips->http_enable },
        { m_ui->adhoc_address_box, m_tooltips->adhoc_address },
        // Debug
        { m_ui->log_imports, m_tooltips->log_imports },
        { m_ui->log_exports, m_tooltips->log_exports },
        { m_ui->log_active_shaders, m_tooltips->log_active_shaders },
        { m_ui->log_uniforms, m_tooltips->log_uniforms },
        { m_ui->color_surface_debug, m_tooltips->color_surface_debug },
        { m_ui->validation_layer, m_tooltips->validation_layer },
        { m_ui->dump_elfs, m_tooltips->dump_elfs },
    };

    for (const auto &[widget, description] : desc_widgets) {
        widget->setProperty("_desc", description);
        widget->installEventFilter(this);
    }
}

bool SettingsDialog::eventFilter(QObject *watched, QEvent *event) {
    if (event->type() == QEvent::Enter) {
        auto *w = qobject_cast<QWidget *>(watched);
        if (w) {
            const QString description = w->property("_desc").toString();
            if (!description.isEmpty()) {
                QWidget *tab = w;
                while (tab && m_ui->tab_widget_settings->indexOf(tab) == -1)
                    tab = tab->parentWidget();
                if (tab)
                    set_description(tab, description);
            }
        }
    } else if (event->type() == QEvent::Leave) {
        auto *w = qobject_cast<QWidget *>(watched);
        if (w && !w->property("_desc").toString().isEmpty()) {
            QWidget *tab = w;
            while (tab && m_ui->tab_widget_settings->indexOf(tab) == -1)
                tab = tab->parentWidget();
            if (tab)
                set_description(tab, m_tooltips->default_description);
        }
    }
    return QDialog::eventFilter(watched, event);
}

void SettingsDialog::set_description(QWidget *tab, const QString &text) {
    const std::pair<QWidget *, QLabel *> map[] = {
        { m_ui->core_tab, m_ui->description_core },
        { m_ui->cpu_tab, m_ui->description_cpu },
        { m_ui->gpu_tab, m_ui->description_gpu },
        { m_ui->audio_tab, m_ui->description_audio },
        { m_ui->camera_tab, m_ui->description_camera },
        { m_ui->system_tab, m_ui->description_system },
        { m_ui->emulator_tab, m_ui->description_emulator },
        { m_ui->gui_tab, m_ui->description_gui },
        { m_ui->network_tab, m_ui->description_network },
        { m_ui->debug_tab, m_ui->description_debug },
    };
    for (const auto &[t, label] : map) {
        if (t == tab) {
            label->setText(text);
            return;
        }
    }
}

void SettingsDialog::update_resolution_label() {
    const float mult = static_cast<float>(m_ui->resolution_upscale->value()) / 4.0f;
    const int w = static_cast<int>(960 * mult);
    const int h = static_cast<int>(544 * mult);
    m_ui->resolution_upscale_val->setText(
        QStringLiteral("%1x%2 (%3x)").arg(w).arg(h).arg(static_cast<double>(mult), 0, 'g', 3));
}

void SettingsDialog::update_aniso_label() {
    const int val = 1 << m_ui->anisotropic_filter->value();
    m_ui->anisotropic_filter_val->setText(QStringLiteral("%1x").arg(val));
}

void SettingsDialog::on_save_clicked() {
    save_config();
    apply_config();
    apply_stylesheet();
    m_dirty = false;
    accept();
}

void SettingsDialog::on_apply_clicked() {
    save_config();
    apply_config();
    apply_stylesheet();
    m_dirty = false;
    for (auto *bb : m_button_boxes)
        bb->button(QDialogButtonBox::Apply)->setEnabled(false);
}

void SettingsDialog::on_close_clicked() {
    reject();
}

void SettingsDialog::reject() {
    if (m_dirty) {
        const int result = QMessageBox::warning(this,
            tr("Unsaved Changes"),
            tr("You have unsaved changes. What would you like to do?"),
            QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel,
            QMessageBox::Save);

        switch (result) {
        case QMessageBox::Save:
            save_config();
            apply_config();
            m_dirty = false;
            QDialog::accept();
            return;
        case QMessageBox::Discard:
            QDialog::reject();
            return;
        default: // Cancel
            return;
        }
    }
    QDialog::reject();
}

void SettingsDialog::mark_dirty() {
    if (!m_dirty) {
        m_dirty = true;
        for (auto *bb : m_button_boxes)
            bb->button(QDialogButtonBox::Apply)->setEnabled(true);
    }
}

void SettingsDialog::setup_dirty_tracking() {
    for (auto *cb : findChildren<QCheckBox *>())
        connect(cb, &QCheckBox::toggled, this, [this] { mark_dirty(); });
    for (auto *rb : findChildren<QRadioButton *>())
        connect(rb, &QRadioButton::toggled, this, [this] { mark_dirty(); });
    for (auto *combo : findChildren<QComboBox *>())
        connect(combo, &QComboBox::currentIndexChanged, this, [this] { mark_dirty(); });
    for (auto *slider : findChildren<QSlider *>())
        connect(slider, &QSlider::valueChanged, this, [this] { mark_dirty(); });
    for (auto *list : findChildren<QListWidget *>())
        connect(list, &QListWidget::itemChanged, this, [this] { mark_dirty(); });
}

void SettingsDialog::update_gpu_visibility() {
    const bool is_vulkan = m_ui->backend_renderer_box->currentText() == QStringLiteral("Vulkan");

    const int gpu_idx = m_ui->gpu_device_box->currentIndex();
    const int mask = app::get_supported_memory_mapping_mask(emuenv, gpu_idx);
    const bool has_mapping = is_vulkan && (mask > 1);

    // Vulkan-only widgets
    m_ui->gb_gpu_device->setVisible(is_vulkan);
    m_ui->gb_renderer_accuracy->setVisible(is_vulkan);
    m_ui->gb_vulkan_options->setVisible(is_vulkan);
    m_ui->spirv_shader->setVisible(is_vulkan);

    m_ui->label_memory_mapping->setVisible(has_mapping);
    m_ui->memory_mapping_box->setVisible(has_mapping);

    // OpenGL-only widgets
    m_ui->gb_opengl_options->setVisible(!is_vulkan);

    {
        const QString previous = m_ui->screen_filter_box->currentText().isEmpty()
            ? QString::fromStdString(m_config.screen_filter)
            : m_ui->screen_filter_box->currentText();

        m_ui->screen_filter_box->clear();
        if (is_vulkan) {
            m_ui->screen_filter_box->addItems({ QStringLiteral("Nearest"), QStringLiteral("Bilinear"),
                QStringLiteral("Bicubic"), QStringLiteral("FXAA"), QStringLiteral("FSR") });
        } else {
            m_ui->screen_filter_box->addItems({ QStringLiteral("Bilinear"), QStringLiteral("FXAA") });
        }

        const int idx = m_ui->screen_filter_box->findText(previous);
        m_ui->screen_filter_box->setCurrentIndex(idx >= 0 ? idx : 0);
    }
}

void SettingsDialog::add_stylesheets() {
    m_ui->combo_stylesheets->blockSignals(true);
    m_ui->combo_stylesheets->clear();

    m_ui->combo_stylesheets->addItem(tr("None", "Stylesheets"), gui::NoStylesheet);

    for (const QString &key : QStyleFactory::keys()) {
        m_ui->combo_stylesheets->addItem(
            tr("Native (%1)", "Stylesheets").arg(key),
            QStringLiteral("%1 (%2)").arg(gui::NativeStylesheet, key));
    }

    m_ui->combo_stylesheets->addItem(tr("Default (Bright)", "Stylesheets"), gui::DefaultStylesheet);
    m_ui->combo_stylesheets->addItem(tr("Default (Dark)", "Stylesheets"), gui::DarkStylesheet);

    if (m_gui_settings) {
        for (const QString &entry : m_gui_settings->get_stylesheet_entries()) {
            if (entry != gui::DefaultStylesheet && entry != gui::DarkStylesheet) {
                m_ui->combo_stylesheets->addItem(entry, entry);
            }
        }
    }

    m_current_stylesheet = m_gui_settings
        ? m_gui_settings->get_value(gui::m_currentStylesheet).toString()
        : gui::DarkStylesheet;

    const int index = m_ui->combo_stylesheets->findData(m_current_stylesheet);
    if (index >= 0) {
        m_ui->combo_stylesheets->setCurrentIndex(index);
    }
    m_ui->combo_stylesheets->blockSignals(false);
}

void SettingsDialog::apply_stylesheet(bool reset) {
    if (reset) {
        m_current_stylesheet = gui::DarkStylesheet;
        m_ui->combo_stylesheets->setCurrentIndex(
            m_ui->combo_stylesheets->findData(gui::DarkStylesheet));
    }

    m_current_stylesheet = m_ui->combo_stylesheets->currentData().toString();

    if (m_gui_settings && m_current_stylesheet != m_gui_settings->get_value(gui::m_currentStylesheet).toString()) {
        m_gui_settings->set_value(gui::m_currentStylesheet, m_current_stylesheet);
        Q_EMIT gui_stylesheet_request();
    }
}
