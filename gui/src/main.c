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

        // zoom: ctrl + plus / minus (main row or keypad), held-to-repeat
        if (ctrl) {
            if (IsKeyPressed(KEY_EQUAL) || IsKeyPressedRepeat(KEY_EQUAL) ||
                IsKeyPressed(KEY_KP_ADD) || IsKeyPressedRepeat(KEY_KP_ADD))
                ui_zoom(+1);
            if (IsKeyPressed(KEY_MINUS) || IsKeyPressedRepeat(KEY_MINUS) ||
                IsKeyPressed(KEY_KP_SUBTRACT) || IsKeyPressedRepeat(KEY_KP_SUBTRACT))
                ui_zoom(-1);
        }

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

        float wheel = GetMouseWheelMove();
        if (wheel != 0) {
            if (ctrl) {
                ui_zoom(wheel > 0 ? +1 : -1); // ctrl + wheel = zoom
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
        BeginDrawing();
        ClearBackground((Color){24, 24, 27, 255});

        BeginScissorMode(0, (int)view_top, sw, (int)view_h);
        ui_layout(view_top - scroll, sw, 1);
        EndScissorMode();

        // input bar
        Rectangle ib = {PAD, sh - INPUT_H, sw - 2 * PAD, INPUT_H - 10};
        DrawRectangleRounded(ib, 0.3f, 8, (Color){38, 38, 44, 255});
        float ty = ib.y + (ib.height - ui_font_size()) / 2;
        if (bridge_confirm_pending()) {
            char prompt[192];
            snprintf(prompt, sizeof(prompt), "run %s ?   [y] yes    [n] no",
                     bridge_confirm_name());
            ui_draw_text(prompt, ib.x + 12, ty, (Color){240, 190, 90, 255});
        } else if (input_len > 0) {
            ui_draw_text(input, ib.x + 12, ty, WHITE);
            if (fmodf(GetTime(), 1.0f) < 0.5f) {
                float tw = ui_measure_text(input);
                DrawRectangle((int)(ib.x + 14 + tw), (int)ty, 2, ui_font_size(), WHITE);
            }
        } else {
            const char *hint = !bridge_connected() ? "connecting to agent..."
                             : busy ? "working..."
                             : "type a message or /help, press Enter";
            ui_draw_text(hint, ib.x + 12, ty, (Color){120, 120, 130, 255});
        }

        EndDrawing();
    }

    bridge_shutdown();
    ui_unload_font();
    CloseWindow();
    return 0;
}
