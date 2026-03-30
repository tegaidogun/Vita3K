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

#include <gui-qt/log_widget.h>
#include <gui-qt/qt_utils.h>

#include <util/log.h>

#include <spdlog/common.h>

#include <QFont>
#include <QPlainTextEdit>
#include <QPointer>
#include <QScrollBar>
#include <QVBoxLayout>

#include <deque>
#include <mutex>
#include <utility>

LogWidget::LogWidget(QWidget *parent)
    : custom_dock_widget(tr("Log"), parent) {
    auto *container = new QWidget(this);
    auto *layout = new QVBoxLayout(container);
    layout->setContentsMargins(0, 0, 0, 0);

    m_log = new QPlainTextEdit(container);
    m_log->setReadOnly(true);
    m_log->setFont(QFont(QStringLiteral("Monospace"), 9));
    m_log->document()->setMaximumBlockCount(1000);
    m_log->setObjectName(QStringLiteral("log_frame"));
    layout->addWidget(m_log);
    setWidget(container);

    connect(this, &LogWidget::log_message_received, this, &LogWidget::on_log_message, Qt::QueuedConnection);
}
static struct {
    std::mutex mutex;
    std::function<void(std::string, int)> callback;
    std::deque<std::pair<std::string, int>> backlog;
} s_log_state;

static constexpr size_t MAX_BACKLOG_LINES = 2000;

void LogWidget::register_callback() {
    logging::set_log_callback([](std::string msg, int level) {
        const std::lock_guard<std::mutex> lock(s_log_state.mutex);
        if (s_log_state.callback) {
            s_log_state.callback(std::move(msg), level);
            return;
        }

        if (s_log_state.backlog.size() >= MAX_BACKLOG_LINES)
            s_log_state.backlog.pop_front();
        s_log_state.backlog.emplace_back(std::move(msg), level);
    });
}

void LogWidget::attach() {
    QPointer<LogWidget> self(this);
    std::deque<std::pair<std::string, int>> backlog;

    {
        const std::lock_guard<std::mutex> lock(s_log_state.mutex);
        s_log_state.callback = [self](std::string msg, int level) {
            if (!self)
                return;
            emit self->log_message_received(QString::fromStdString(msg).trimmed(), level);
        };
        backlog = std::move(s_log_state.backlog);
        s_log_state.backlog.clear();
    }

    for (const auto &[msg, level] : backlog) {
        const auto qmsg = QString::fromStdString(msg).trimmed();
        QMetaObject::invokeMethod(
            this, [this, qmsg, level]() { on_log_message(qmsg, level); }, Qt::QueuedConnection);
    }
}

void LogWidget::on_log_message(const QString &msg, int level) {
    m_entries.emplace_back(msg, level);
    trim_entries();
    append_log_entry(msg, level);
}

void LogWidget::append_log_entry(const QString &msg, int level, bool preserve_scroll) {
    QScrollBar *bar = m_log->verticalScrollBar();
    const bool at_bottom = !preserve_scroll || bar->value() == bar->maximum();

    QTextCursor cursor = m_log->textCursor();
    cursor.movePosition(QTextCursor::End);

    QTextCharFormat fmt;
    fmt.setForeground(color_for_level(level));

    cursor.insertText(msg + "\n", fmt);

    if (at_bottom)
        bar->setValue(bar->maximum());
}

QColor LogWidget::color_for_level(int level) const {
    using L = spdlog::level::level_enum;

    switch (static_cast<L>(level)) {
    case L::critical:
        return gui::utils::get_label_color(
            QStringLiteral("log_level_fatal"),
            QColor(QStringLiteral("#ff00ff")),
            QColor(QStringLiteral("#ff66ff")));
    case L::err:
        return gui::utils::get_label_color(
            QStringLiteral("log_level_error"),
            QColor(QStringLiteral("#C02F1D")),
            QColor(QStringLiteral("#ff8080")));
    case L::warn:
        return gui::utils::get_label_color(
            QStringLiteral("log_level_warning"),
            QColor(QStringLiteral("#BA8745")),
            QColor(QStringLiteral("#ffd27f")));
    case L::info:
        return gui::utils::get_label_color(
            QStringLiteral("log_level_notice"),
            QColor(Qt::black),
            QColor(Qt::white));
    default:
        return gui::utils::get_label_color(
            QStringLiteral("log_level_trace"),
            QColor(QStringLiteral("#808080")),
            QColor(Qt::white));
    }
}

void LogWidget::trim_entries() {
    const int max_blocks = m_log->document()->maximumBlockCount();
    while (max_blocks > 0 && static_cast<int>(m_entries.size()) > max_blocks)
        m_entries.pop_front();
}

void LogWidget::set_log_buffer_size(int size) {
    m_log->document()->setMaximumBlockCount(size);
    trim_entries();
}

int LogWidget::log_buffer_size() const {
    return m_log->document()->maximumBlockCount();
}

void LogWidget::repaint_text_colors() {
    const QScrollBar *bar = m_log->verticalScrollBar();
    const int old_value = bar->value();
    const bool at_bottom = old_value == bar->maximum();

    m_log->clear();

    for (const auto &[msg, level] : m_entries)
        append_log_entry(msg, level, false);

    if (!at_bottom)
        m_log->verticalScrollBar()->setValue(old_value);
}
