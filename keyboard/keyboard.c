// keyboard.c -- Search keyboard & results for Luther Bibel 1912

#include "keyboard.h"
#include "../font/font.h"
#include <gui/elements.h>
#include <string.h>
#include <stdio.h>

// Keyboard page tables

const char kb_page0[KB_NROWS][KB_NCOLS] = {
    { 'q','w','e','r','t','y','u','i','o','p','1','2','3' },
    { 'a','s','d','f','g','h','j','k','l',':','4','5','6' },
    { 'z','x','c','v','b','n','m',',','.','_','7','8','9' },
};

const char kb_page1[KB_NROWS][KB_NCOLS] = {
    { '!','@','#','$','%','^','&','*','(',')','{','}','[' },
    { ']','<','>','?','/',';',':','\'','"','~','`','\\','|' },
    { '+','=','0',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ' },
};

// Page 2: umlauts and special characters (matches reference implementation)
typedef struct { const char* label; } SpecialKey;
static const SpecialKey kb_page2[KB_NROWS][KB_NCOLS] = {
    { {"\xC3\x84"},{"\xC3\xA4"},{"\xC3\x96"},{"\xC3\xB6"},{"\xC3\x9C"},{"\xC3\xBC"},
      {"\xC3\x9F"},{"\xC2\xA1"},{"\xC2\xBF"},{"\xC2\xAB"},{"\xC2\xBB"},
      {"\xE2\x80\x98"},{"\xE2\x80\x99"} },
    { {"\xE2\x80\x9C"},{"\xE2\x80\x9D"},{"\xE2\x80\x93"},{"\xE2\x80\x94"},
      {"\xC3\xA9"},{"\xC3\xA8"},{"\xC3\xAA"},{"\xC3\xAB"},{"\xC3\xAF"},
      {"\xC3\xAE"},{"\xC3\xA0"},{"\xC3\xA2"},{"\xC3\xB5"} },
    { {"\xC3\xB1"},{"\xC3\xA7"},{"\xC3\xBF"},{"\xC3\xB8"},{"\xC3\xA5"},
      {"\xC3\xA6"},{"\xC3\x90"},{"\xC3\xBE"},{"\xC2\xA3"},{"\xC2\xA5"},
      {"\xC2\xA9"},{"\xC2\xAE"},{"\xC2\xB0"} },
};

// Maps each of the 13 keyboard columns to the nearest special button (0-4)
static const uint8_t col_to_btn[KB_NCOLS] = { 0,0,0, 1,1, 2,2,2, 3,3,3, 4,4 };
// Maps each special button back to a representative keyboard column
static const uint8_t btn_to_col[5]        = { 1, 3, 6, 9, 11 };

const char* kb_key_label(App* app, uint8_t row, uint8_t col) {
    static char buf[4];
    if(app->kb_page == 0) {
        char ch = kb_page0[row][col];
        if(app->kb_caps && ch >= 'a' && ch <= 'z') ch = (char)(ch - 32);
        buf[0] = ch; buf[1] = '\0'; return buf;
    }
    if(app->kb_page == 1) { buf[0] = kb_page1[row][col]; buf[1] = '\0'; return buf; }
    return kb_page2[row][col].label;
}

// UTF-8 safe backspace: removes the last character (which may be multi-byte)
static void search_buf_backspace(App* app) {
    if(!app->search_len) return;
    // Walk back over any UTF-8 continuation bytes (0x80–0xBF)
    while(app->search_len > 0 && (app->search_buf[app->search_len - 1] & 0xC0) == 0x80)
        app->search_buf[--app->search_len] = '\0';
    // Remove the leading byte of the character
    if(app->search_len > 0)
        app->search_buf[--app->search_len] = '\0';
}

// draw_search_input

void draw_search_input(Canvas* canvas, App* app) {
    static const char* const ptitles[] = { "Search", "Search: Sym", "Search: Uml" };
    const char* title = ptitles[app->kb_page < 3 ? app->kb_page : 0];
    draw_hdr(canvas, title);

    // Input field
    canvas_set_font_custom(canvas, FONT_SIZE_MEDIUM);
    canvas_draw_frame(canvas, 2, HDR_H + 1, SCREEN_W - 4, 12);
    char disp[MAX_SEARCH_LEN + 28];
    // Show ghost suggestion: "typed_remainder" so the user sees what Hold-OK will accept
    bool has_suggestion = app->kb_suggestion[0] != '\0' &&
                          app->api_input_active &&
                          app->search_len < (uint8_t)strlen(app->kb_suggestion);
    if(has_suggestion) {
        snprintf(disp, sizeof(disp), "%s_%s", app->search_buf,
                 app->kb_suggestion + app->search_len);
    } else {
        snprintf(disp, sizeof(disp), "%s_", app->search_buf);
    }
    canvas_draw_str(canvas, 4, HDR_H + 10, disp);

    // Keyboard grid
    // Special row is 8px tall, pinned to the very bottom (y=56..63).
    // The 3 key rows fill y=26..55 equally: 30px / 3 = 10px each.
    const uint8_t bkh = 8;
    const uint8_t by  = SCREEN_H - bkh;          // 56 -- special row, flush to bottom
    const uint8_t kw  = 9;
    const uint8_t kh  = 10;                       // 3 rows x 10px = 30px
    const uint8_t ky  = by - KB_NROWS * kh;       // 26 -- first key row
    for(uint8_t r = 0; r < KB_NROWS; r++) {
        for(uint8_t c = 0; c < KB_NCOLS; c++) {
            uint8_t x = 4 + c * kw, y = ky + r * kh;
            bool sel = (r == app->kb_row && c == app->kb_col);
            if(sel) {
                canvas_set_color(canvas, ColorBlack);
                canvas_draw_box(canvas, x, y, kw - 1, kh - 1);
                canvas_set_color(canvas, ColorWhite);
            } else {
                canvas_set_color(canvas, ColorBlack);
            }
            canvas_draw_str(canvas, x + 1, y + 7, kb_key_label(app, r, c));
            canvas_set_color(canvas, ColorBlack);
        }
    }

    // Special buttons row: DEL | SPC | CAP | SYM/ABC | GO!
    // Pinned flush to the bottom of the screen.
    const char* btns[5] = {
        "DEL", "SPC",
        (app->kb_page == 0) ? "CAP" : "---",  // CAP only on page 0
        (app->kb_page == 0) ? "SYM" : (app->kb_page == 1) ? "UML" : "ABC",
        "GO!"
    };
    const uint8_t bx[5] = {  2, 27, 52, 77, 102 };
    const uint8_t bw[5] = { 23, 23, 23, 23,  23 };
    for(uint8_t i = 0; i < 5; i++) {
        bool btn_sel  = (app->kb_row == KB_NROWS && app->kb_col == i);
        bool caps_lit = (i == 2 && app->kb_page == 0 && app->kb_caps);  // CAP only lights on page 0
        bool fill = btn_sel || caps_lit;
        if(fill) {
            canvas_set_color(canvas, ColorBlack);
            canvas_draw_box(canvas, bx[i], by, bw[i], bkh);
            canvas_set_color(canvas, ColorWhite);
        } else {
            canvas_set_color(canvas, ColorBlack);
            canvas_draw_frame(canvas, bx[i], by, bw[i], bkh);
        }
        canvas_set_font_custom(canvas, FONT_SIZE_SMALL);   // smallest custom font
        canvas_draw_str_aligned(canvas, bx[i] + bw[i] / 2, by + bkh - 1,
                                AlignCenter, AlignBottom, btns[i]);
        canvas_set_font(canvas, FontSecondary);   // restore default for next iteration
        canvas_set_color(canvas, ColorBlack);
    }
}

// draw_search_results

void draw_search_results(Canvas* canvas, App* app) {
    char hdr_buf[24];
    if(app->hits.count == 0)
        snprintf(hdr_buf, sizeof(hdr_buf), "Not found");
    else
        snprintf(hdr_buf, sizeof(hdr_buf), "Found: %d", (int)app->hits.count);
    draw_hdr(canvas, hdr_buf);

    canvas_set_font(canvas, FontSecondary);

    if(app->hits.count == 0) {
        canvas_draw_str_aligned(canvas, SCREEN_W / 2, 34,
                                AlignCenter, AlignCenter, "No matches");
        canvas_draw_str_aligned(canvas, SCREEN_W / 2, 46,
                                AlignCenter, AlignCenter, "Try different words");
        return;
    }

    const uint8_t vis = (uint8_t)((SCREEN_H - HDR_H - 2) / LINE_H);

    // Auto-scroll so selected item stays visible
    if(app->hits.sel < app->hits.scroll)
        app->hits.scroll = app->hits.sel;
    if(app->hits.sel >= app->hits.scroll + vis)
        app->hits.scroll = (uint8_t)(app->hits.sel - vis + 1);

    for(uint8_t i = 0; i < vis && (app->hits.scroll + i) < app->hits.count; i++) {
        uint8_t si = app->hits.scroll + i;
        uint8_t y  = HDR_H + 2 + i * LINE_H;
        bool    sel = (si == app->hits.sel);

        if(sel) {
            canvas_set_color(canvas, ColorBlack);
            canvas_draw_box(canvas, 0, y - 1, SCREEN_W - SB_W - 2, LINE_H);
            canvas_set_color(canvas, ColorWhite);
        } else {
            canvas_set_color(canvas, ColorBlack);
        }

        canvas_draw_str(canvas, 4, y + 8, app->index[app->hits.idx[si]].ref);
        canvas_set_color(canvas, ColorBlack);
    }

    draw_scrollbar(canvas, app->hits.scroll, app->hits.count, vis);
}

// on_search  (input handler for the keyboard view)

void on_search(App* app, InputEvent* ev) {

    // ── Hold OK: accept book suggestion if available, otherwise type opposite-case ─
    if(ev->type == InputTypeLong && ev->key == InputKeyOk) {
        // When a suggestion is active, Hold-OK accepts it (copies full name + space)
        if(app->api_input_active && app->kb_suggestion[0] != '\0') {
            uint8_t slen = (uint8_t)strlen(app->kb_suggestion);
            if(slen + 1 < MAX_SEARCH_LEN) {
                memcpy(app->search_buf, app->kb_suggestion, slen);
                app->search_buf[slen]     = ' ';
                app->search_buf[slen + 1] = '\0';
                app->search_len = slen + 1;
            }
            kb_update_suggestion(app);
            app->kb_long_consumed = true;
            return;
        }
        // No suggestion: type the opposite-case version of the current letter
        if(app->kb_row < KB_NROWS && app->kb_page == 0) {
            char ch = kb_page0[app->kb_row][app->kb_col];
            if(ch >= 'a' && ch <= 'z') {
                // When caps is off, normal press gives lower → hold gives upper
                // When caps is on,  normal press gives upper → hold gives lower
                if(!app->kb_caps) ch = (char)(ch - 32);
                // (if kb_caps is true, ch is already lower-case, keep as-is)
                if(app->search_len < MAX_SEARCH_LEN - 1) {
                    app->search_buf[app->search_len++] = ch;
                    app->search_buf[app->search_len]   = '\0';
                }
            }
        }
        app->kb_long_consumed = true;
        return;
    }
    // Release clears the flag so the next short press works normally
    if(ev->type == InputTypeRelease && ev->key == InputKeyOk) {
        app->kb_long_consumed = false;
        return;
    }
    // Suppress the repeat/short event that fires after a long-press
    if(ev->type == InputTypeRepeat && ev->key == InputKeyOk && app->kb_long_consumed) return;

    if(ev->type != InputTypeShort && ev->type != InputTypeRepeat) return;

    switch(ev->key) {
    // ── Navigation ─────────────────────────────────────────────────────────
    // Up/Down wrap between the letter grid and the special-button row.
    // Left/Right wrap within each row.
    case InputKeyUp:
        if(app->kb_row == KB_NROWS)       // special row → top letter row, map col
            { app->kb_row = KB_NROWS - 1; app->kb_col = btn_to_col[app->kb_col]; }
        else if(app->kb_row > 0)
            app->kb_row--;
        else                              // top letter row → wrap down to special row
            { app->kb_row = KB_NROWS;    app->kb_col = col_to_btn[app->kb_col]; }
        break;

    case InputKeyDown:
        if(app->kb_row < KB_NROWS - 1)
            app->kb_row++;
        else if(app->kb_row == KB_NROWS - 1)  // last letter row → special row
            { app->kb_row = KB_NROWS;         app->kb_col = col_to_btn[app->kb_col]; }
        else                                   // special row → wrap to top letter row
            { app->kb_row = 0;                app->kb_col = btn_to_col[app->kb_col]; }
        break;

    case InputKeyLeft:
        if(app->kb_row == KB_NROWS)
            app->kb_col = (app->kb_col == 0) ? 4           : app->kb_col - 1;
        else
            app->kb_col = (app->kb_col == 0) ? KB_NCOLS - 1 : app->kb_col - 1;
        break;

    case InputKeyRight:
        if(app->kb_row == KB_NROWS)
            app->kb_col = (app->kb_col == 4)          ? 0 : app->kb_col + 1;
        else
            app->kb_col = (app->kb_col == KB_NCOLS - 1) ? 0 : app->kb_col + 1;
        break;

    // ── OK: type or activate special button ────────────────────────────────
    case InputKeyOk:
        if(app->kb_row < KB_NROWS) {
            // Type a character from the grid (may be multi-byte UTF-8 on page 2)
            const char* s = kb_key_label(app, app->kb_row, app->kb_col);
            if(s && s[0]) {
                size_t slen = strlen(s);
                if(app->search_len + slen < MAX_SEARCH_LEN) {
                    memcpy(app->search_buf + app->search_len, s, slen);
                    app->search_len += (uint8_t)slen;
                    app->search_buf[app->search_len] = '\0';
                }
                kb_update_suggestion(app);
            }
        } else {
            // Special button row
            switch(app->kb_col) {
            case 0: // DEL
                search_buf_backspace(app);
                kb_update_suggestion(app);
                break;
            case 1: // SPC
                if(app->search_len < MAX_SEARCH_LEN - 1) {
                    app->search_buf[app->search_len++] = ' ';
                    app->search_buf[app->search_len]   = '\0';
                }
                kb_update_suggestion(app);
                break;
            case 2: // CAP (only active on page 0; "---" on page 1)
                if(app->kb_page == 0) app->kb_caps = !app->kb_caps;
                break;
            case 3: // SYM / UML / ABC -- cycle pages 0 -> 1 -> 2 -> 0
                app->kb_page = (app->kb_page == 0) ? 1 : (app->kb_page == 1) ? 2 : 0;
                break;
            case 4: // GO! -- run search or API lookup depending on mode
                if(app->search_len > 0) kb_submit(app);
                break;
            }
        }
        break;

    // ── Back: backspace if buffer non-empty, else exit to menu ────────────
    // Matches reference behaviour: Back acts as DEL while there is text,
    // and only navigates away once the buffer is fully cleared.
    case InputKeyBack:
        if(app->search_len > 0) {
            search_buf_backspace(app);
            kb_update_suggestion(app);
        } else {
            app->view = ViewMainMenu;
        }
        break;

    default: break;
    }
}

// on_search_results  (input handler for the results view)

void on_search_results(App* app, InputEvent* ev) {
    if(ev->type != InputTypeShort && ev->type != InputTypeRepeat) return;

    const uint8_t vis = (uint8_t)((SCREEN_H - HDR_H - 2) / LINE_H);

    switch(ev->key) {
    case InputKeyUp:
        if(app->hits.sel > 0) app->hits.sel--;
        break;
    case InputKeyDown:
        if(app->hits.sel < app->hits.count - 1) app->hits.sel++;
        break;
    case InputKeyOk:
        if(app->hits.count == 0) break;
        open_verse(app, app->hits.idx[app->hits.sel], ViewSearchResults);
        break;
    case InputKeyBack:
        app->view = ViewSearchInput;
        break;
    default: break;
    }

    // Keep scroll in sync
    if(app->hits.count > 0) {
        if(app->hits.sel < app->hits.scroll)
            app->hits.scroll = app->hits.sel;
        if(app->hits.sel >= app->hits.scroll + vis)
            app->hits.scroll = (uint8_t)(app->hits.sel - vis + 1);
    }
}
