// ============================================================
// TUMO agent GUI: a native C + raylib front-end for the agent.
//
// This file is just the window + main loop. The work is split across:
//   paths.c   figuring out where the project lives
//   chat.c    the message transcript (the data model)
//   bridge.c  launching and talking to the Node agent (src/serve.ts)
//   ui.c      fonts and drawing the chat with raylib
//
// The agent core never changes; this whole directory is one more adapter,
// like the CLI in agent.ts.
// ============================================================

#include <stdio.h>
#include <string.h>
#include <math.h>

#include "raylib.h"
#include "chat.h"
#include "bridge.h"
#include "ui.h"
#include "paths.h"

int main(void) {
    resolve_paths();
    bridge_start();

    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_MSAA_4X_HINT);
    InitWindow(900, 640, "TUMO agent");
    SetTargetFPS(60);
    ui_load_font();
    ui_load_settings();

    char input[4096] = {0};
    int input_len = 0;
    int busy = 0;
    float scroll = 0;
    int stick = 1; // auto-scroll to the newest message

    // The input bar shows "connecting to agent..." until the bridge is ready,
    // so there's no need for a startup line lingering in the transcript.

    while (!WindowShouldClose()) {
        // 1. drain agent output into the chat model
        bridge_poll(&busy);

        int ctrl = IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL);
        int shift = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);

        // theme toggle: ctrl+t
        if (ctrl && !shift && IsKeyPressed(KEY_T)) {
            ui_toggle_theme();
            ui_save_settings();
        }

        // copy: ctrl+shift+C  (copy selected or newest non-system message)
        if (ctrl && shift && IsKeyPressed(KEY_C)) {
            const Msg *m = NULL;
            if (ui_selected_msg >= 0 && ui_selected_msg < chat_count())
                m = chat_get(ui_selected_msg);
            if (!m || m->role == ROLE_SYSTEM || !m->text || !m->text[0]) {
                // fallback: newest non-system message
                for (int i = chat_count() - 1; i >= 0; i--) {
                    m = chat_get(i);
                    if (m->role != ROLE_SYSTEM && m->text && m->text[0]) break;
                }
            }
            if (m && m->text && m->text[0])
                SetClipboardText(m->text);
        }

        // paste: ctrl+V
        if (ctrl && !shift && IsKeyPressed(KEY_V)) {
            const char *clip = GetClipboardText();
            if (clip) {
                size_t clen = strlen(clip);
                size_t room = sizeof(input) - (size_t)input_len - 1;
                if (clen > room) clen = room;
                memcpy(input + input_len, clip, clen);
                input_len += (int)clen;
                input[input_len] = 0;
            }
        }

        // zoom: ctrl + plus / minus (main row or keypad), held-to-repeat
        if (ctrl && !shift) {
            if (IsKeyPressed(KEY_EQUAL) || IsKeyPressedRepeat(KEY_EQUAL) ||
                IsKeyPressed(KEY_KP_ADD) || IsKeyPressedRepeat(KEY_KP_ADD)) {
                int old = ui_font_size();
                ui_zoom(+1);
                if (ui_font_size() != old) ui_save_settings();
            }
            if (IsKeyPressed(KEY_MINUS) || IsKeyPressedRepeat(KEY_MINUS) ||
                IsKeyPressed(KEY_KP_SUBTRACT) || IsKeyPressedRepeat(KEY_KP_SUBTRACT)) {
                int old = ui_font_size();
                ui_zoom(-1);
                if (ui_font_size() != old) ui_save_settings();
            }
        }

        // ctrl+wheel zoom also saves
        if (GetMouseWheelMove() != 0 && ctrl) {
            // will be handled below, but save happens there
        }

        // 2. input

        // 2. input
        if (bridge_confirm_pending()) {
            // a dangerous tool is waiting: answer y / n, ignore typing
            int c;
            while ((c = GetCharPressed()) > 0) { (void)c; }
            if (IsKeyPressed(KEY_Y)) { bridge_answer_confirm(1); stick = 1; }
            else if (IsKeyPressed(KEY_N) || IsKeyPressed(KEY_ESCAPE)) {
                bridge_answer_confirm(0);
                stick = 1;
            }
        } else {
            int c;
            while ((c = GetCharPressed()) > 0) {
                if (!ctrl && c >= 32 && c < 127 && input_len < (int)sizeof(input) - 1) {
                    input[input_len++] = (char)c;
                    input[input_len] = 0;
                }
            }
            if ((IsKeyPressed(KEY_BACKSPACE) || IsKeyPressedRepeat(KEY_BACKSPACE)) &&
                input_len > 0) {
                input[--input_len] = 0;
            }
            if (IsKeyPressed(KEY_ENTER) && !busy && input_len > 0 &&
                bridge_connected()) {
                if (strcmp(input, "/exit") == 0 || strcmp(input, "exit") == 0 ||
                    strcmp(input, "quit") == 0) {
                    break; // close the window
                } else if (input[0] == '/') {
                    // a slash command: show it, hand it to the agent bridge
                    chat_push(ROLE_USER, input);
                    bridge_send("command", input);
                    input_len = 0;
                    input[0] = 0;
                    stick = 1;
                } else {
                    chat_push(ROLE_USER, input);
                    bridge_send("user", input);
                    input_len = 0;
                    input[0] = 0;
                    busy = 1;
                    stick = 1;
                }
            }
        }

        int sw = GetScreenWidth();
        int sh = GetScreenHeight();
        float view_top = PAD;
        float view_h = sh - INPUT_H - PAD - view_top;

        // 3. scrolling
        float content_h = ui_layout(0, sw, 0);
        float max_scroll = content_h - view_h;
        if (max_scroll < 0) max_scroll = 0;

        // click to select a message
        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            Vector2 mp = GetMousePosition();
            if (mp.y >= view_top && mp.y < view_top + view_h) {
                // screen_y is in the chat content area
                float screen_y = mp.y;
                int idx = ui_msg_at_y(screen_y);
                if (idx >= 0) {
                    ui_selected_msg = (ui_selected_msg == idx) ? -1 : idx;
                } else {
                    ui_selected_msg = -1;
                }
            }
        }

        // right-click to copy the selected (or clicked) message
        if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT)) {
            Vector2 mp = GetMousePosition();
            int idx = ui_msg_at_y(mp.y);
            if (idx >= 0) {
                const Msg *m = chat_get(idx);
                if (m->text && m->text[0]) SetClipboardText(m->text);
            } else if (ui_selected_msg >= 0 && ui_selected_msg < chat_count()) {
                const Msg *m = chat_get(ui_selected_msg);
                if (m->text && m->text[0]) SetClipboardText(m->text);
            }
        }

        float wheel = GetMouseWheelMove();
        if (wheel != 0) {
            if (ctrl) {
                int old = ui_font_size();
                ui_zoom(wheel > 0 ? +1 : -1); // ctrl + wheel = zoom
                if (ui_font_size() != old) ui_save_settings();
            } else {
                scroll -= wheel * 40;
                stick = 0;
            }
        }
        if (stick) scroll = max_scroll;
        if (scroll < 0) scroll = 0;
        if (scroll > max_scroll) scroll = max_scroll;
        if (scroll >= max_scroll - 1.0f) stick = 1;

        // 4. draw
        Palette pal = ui_palette();
        BeginDrawing();
        ClearBackground(pal.bg);

        BeginScissorMode(0, (int)view_top, sw, (int)view_h);
        ui_layout(view_top - scroll, sw, 1);
        EndScissorMode();

        // scrollbar
        if (max_scroll > 0) {
            float bar_h = view_h * (view_h / content_h);
            float bar_y = view_top + (scroll / max_scroll) * (view_h - bar_h);
            DrawRectangle(sw - 8, (int)bar_y, 6, (int)bar_h, pal.scrollbar);
        }

        // gear / settings button (top-right corner)
        {
            int btn_size = 28;
            int bx = sw - btn_size - PAD;
            int by = PAD;
            Color btn_col = pal.input_bg;
            Vector2 mp = GetMousePosition();
            Rectangle btn_rect = {(float)bx, (float)by, (float)btn_size, (float)btn_size};
            int hover = CheckCollisionPointRec(mp, btn_rect);
            if (hover) btn_col = ui_shade(btn_col, 20);
            DrawRectangleRounded(btn_rect, 0.3f, 6, btn_col);
            // gear symbol
            ui_draw_text("\xE2\x9A\x99", (float)bx + 5, (float)by + 3, pal.input_text);
            if (hover && IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
                ui_settings_active = 1;
        }

        // input bar
        Rectangle ib = {PAD, sh - INPUT_H, sw - 2 * PAD, INPUT_H - 10};
        DrawRectangleRounded(ib, 0.3f, 8, pal.input_bg);
        float ty = ib.y + (ib.height - ui_font_size()) / 2;
        if (bridge_confirm_pending()) {
            char prompt[192];
            snprintf(prompt, sizeof(prompt), "run %s ?   [y] yes    [n] no",
                     bridge_confirm_name());
            ui_draw_text(prompt, ib.x + 12, ty, (Color){240, 190, 90, 255});
        } else if (input_len > 0) {
            ui_draw_text(input, ib.x + 12, ty, pal.input_text);
            if (fmodf(GetTime(), 1.0f) < 0.5f) {
                float tw = ui_measure_text(input);
                DrawRectangle((int)(ib.x + 14 + tw), (int)ty, 2, ui_font_size(), pal.cursor);
            }
        } else {
            const char *hint = !bridge_connected() ? "connecting to agent..."
                              : busy ? "working..."
                              : "type a message or /help, press Enter";
            ui_draw_text(hint, ib.x + 12, ty, pal.placeholder);
        }

        // settings overlay (on top of everything)
        if (ui_settings_active) {
            ui_settings_active = ui_draw_settings(sw, sh);
        }

        EndDrawing();
    }

    ui_save_settings();
    bridge_shutdown();
    ui_unload_font();
    CloseWindow();
    return 0;
}
