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

#include <gui-qt/custom_dock_widget.h>

#include <QDockWidget>
#include <QPlainTextEdit>

#include <deque>

class LogWidget : public custom_dock_widget {
    Q_OBJECT

public:
    explicit LogWidget(QWidget *parent = nullptr);

    void attach();
    static void register_callback();
    void set_log_buffer_size(int size);
    int log_buffer_size() const;
    void repaint_text_colors();

signals:
    void log_message_received(QString msg, int level);

private slots:
    void on_log_message(const QString &msg, int level);

private:
    void append_log_entry(const QString &msg, int level, bool preserve_scroll = true);
    QColor color_for_level(int level) const;
    void trim_entries();

    QPlainTextEdit *m_log;
    std::deque<std::pair<QString, int>> m_entries;
};
