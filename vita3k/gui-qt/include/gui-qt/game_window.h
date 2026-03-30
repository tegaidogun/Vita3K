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

#include <emuenv/state.h>
#include <renderer/types.h>

#include <QWindow>

#include <chrono>
#include <future>

class QOpenGLContext;
class QSurfaceFormat;
class QThread;
class QTimer;
class GameWindow : public QWindow {
    Q_OBJECT

public:
    explicit GameWindow(EmuEnvState &state, renderer::Backend backend, QScreen *screen = nullptr);
    ~GameWindow();

    void start_ui_updates();
    void stop_ui_updates();

    renderer::Backend backend() const { return m_backend; }

    const QSurfaceFormat &surface_format() const { return m_format; }
    bool create_gl_context();
    bool make_current();
    void swap_buffers();
    void done_current();
    void destroy_gl_context();
    void prepare_gl_for_render_thread();
    void complete_gl_migration();

    QOpenGLContext *gl_context() const { return m_gl_context; }

    int client_width_px() const;
    int client_height_px() const;
    void set_paused(bool paused);
    bool set_vsync_enabled(bool enabled);
    void toggle_fullscreen();

signals:
    void closed();

protected:
    bool event(QEvent *e) override;
    void keyPressEvent(QKeyEvent *e) override;
    void keyReleaseEvent(QKeyEvent *e) override;
    void update_mouse_position(QMouseEvent *e);
    void mousePressEvent(QMouseEvent *e) override;
    void mouseReleaseEvent(QMouseEvent *e) override;
    void mouseMoveEvent(QMouseEvent *e) override;
    void focusInEvent(QFocusEvent *e) override;
    void focusOutEvent(QFocusEvent *e) override;

private slots:
    void ui_tick();

private:
    void update_window_title();

    EmuEnvState &m_emuenv;
    renderer::Backend m_backend;
    QSurfaceFormat m_format;

    QOpenGLContext *m_gl_context = nullptr;
    bool m_gl_migration_pending = false;
    std::promise<QThread *> m_render_thread_id;
    std::future<QThread *> m_render_thread_id_future;
    std::promise<void> m_gl_migration_done;
    std::shared_future<void> m_gl_migration_done_future;
    QTimer *m_ui_timer = nullptr;
    std::chrono::steady_clock::time_point m_last_fps_time{};
    bool m_fps_tracking_started = false;
    bool m_ime_was_active = false;

    std::string m_title_backend_renderer;
};
