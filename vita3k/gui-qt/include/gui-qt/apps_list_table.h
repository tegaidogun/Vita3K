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
#include <compat/state.h>
#include <gui-qt/apps_list_columns.h>
#include <util/fs.h>

#include <QTableWidget>

#include <vector>

class EmuEnvState;

class AppsListTable : public QTableWidget {
    Q_OBJECT

public:
    explicit AppsListTable(QWidget *parent = nullptr);

    void populate(const std::vector<app::AppEntry> &apps,
        const std::map<std::string, app::AppTime> &user_times,
        const CompatState &compat,
        const fs::path &pref_path,
        const fs::path &config_path);

    void adjust_icon_column();

    void resize_columns_to_contents(int spacing = 20);

    void restore_layout(const QByteArray &state);

    void sort(int column, Qt::SortOrder order);
    int sort_column() const { return m_sort_column; }
    Qt::SortOrder sort_order() const { return m_sort_order; }

    void set_icon_size(QSize size);
    void repaint_icons();
    QSize icon_size() const { return m_icon_size; }
    const app::AppEntry *selected_app() const;
    std::vector<const app::AppEntry *> selected_apps() const;

Q_SIGNALS:
    void app_boot_requested(const app::AppEntry &app);
    void app_selection_changed(const app::AppEntry *app);
    void context_menu_requested(const QPoint &global_pos, const std::vector<const app::AppEntry *> &apps);
    void list_refreshed();

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;

private Q_SLOTS:
    void on_column_header_clicked(int col);

private:
    app::AppEntry app_from_row(int row) const;
    void setup_header();
    QString compat_text(const CompatibilityState state) const;
    QColor compat_color(const CompatibilityState state) const;
    QPixmap compat_pixmap(const CompatibilityState state) const;

    QSize m_icon_size{ 75, 75 };
    Qt::SortOrder m_sort_order{ Qt::AscendingOrder };
    int m_sort_column{ static_cast<int>(AppsListColumn::Title) };
};
