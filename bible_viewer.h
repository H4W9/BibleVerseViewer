// bible_viewer.h — Shared types, constants, and declarations for
//                  Bible Verse Viewer (Flipper Zero)
// All modules include this header.
#pragma once
#include <stdint.h>
#include <stdbool.h>
#include <gui/gui.h>
#include <gui/canvas.h>
#include <gui/view_port.h>
#include <input/input.h>
#include <storage/storage.h>
#include <furi.h>
#include <furi_hal.h>

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================
// Screen / layout constants
// ============================================================

#define SCREEN_W          128
#define SCREEN_H           64
#define HDR_H              12
#define BODY_Y             14   // HDR_H + 2
#define LINE_H             10
#define VISIBLE_LINES       5
#define SB_W                3
#define SB_X               (SCREEN_W - SB_W - 1)

// ============================================================
// Data / buffer sizes
// ============================================================

#define MAX_SEARCH_LEN     64
#define MAX_SEARCH_RESULTS 50
#define MAX_BOOKMARKS      75
#define MAX_VERSES        600
#define WRAP_MAX_LINES      8
#define WRAP_LINE_LEN      32
#define REF_LEN            24
#define LINE_BUF_LEN      320

// ============================================================
// Keyboard layout constants
// ============================================================

#define KB_NROWS   3
#define KB_NCOLS  13
#define KB_NPAGES  3

// ============================================================
// API / misc constants
// ============================================================

#define API_RESULT_FOOTER_H  9
#define API_TRANS_COUNT      9
#define BIBLE_BOOKS_COUNT   66
#define API_MENU_ITEMS       7
#define FONT_COUNT           5

// ============================================================
// File system paths
// ============================================================

#define DATA_DIR      "/ext/apps_data/bible_viewer"
#define BM_PATH       DATA_DIR "/bookmarks.txt"
#define SETTINGS_PATH DATA_DIR "/settings.txt"

// Index cache format
#define IDX_MAGIC    "BVIX"
#define IDX_VERSION  ((uint8_t)2)

#define APP_VERSION  "1.4"

// ============================================================
// Enums
// ============================================================

typedef enum {
    ViewMainMenu,
    ViewBrowseList,
    ViewVerseRead,
    ViewSearchInput,
    ViewSearchResults,
    ViewRandomVerse,
    ViewDailyVerse,
    ViewBookmarks,
    ViewSettings,
    ViewAbout,
    ViewLoading,
    ViewError,
    // Bible API (online)
    ViewApiMenu,
    ViewApiLoading,
    ViewApiResult,
    ViewApiError,
    ViewApiTrans,
    ViewApiStatus,
} AppView;

typedef enum {
    MenuBrowse,
    MenuSearch,
    MenuRandom,
    MenuDaily,
    MenuBookmarks,
    MenuApi,
    MenuSettings,
    MenuAbout,
    MenuItemCount,
} MenuChoice;

typedef enum {
    FONT_SECONDARY = 0,  // Flipper built-in
    FONT_SMALL     = 1,  // custom 4x6
    FONT_MEDIUM    = 2,  // custom 5x8
    FONT_LARGE     = 3,  // custom 6x10
    FONT_XLARGE    = 4,  // custom 9x15
} FontChoice;

// ============================================================
// FlipperHTTP types (full definition required here)
// ============================================================
#include "flipper_http/flipper_http.h"

// ============================================================
// Structs
// ============================================================

typedef struct {
    char    lines[WRAP_MAX_LINES][WRAP_LINE_LEN + 1];
    uint8_t count;
    uint8_t scroll;
} WrapState;

typedef struct {
    uint16_t idx[MAX_SEARCH_RESULTS];
    uint8_t  count;
    uint8_t  sel;
    uint8_t  scroll;   // scroll offset for the results list
} SearchHits;

typedef struct {
    uint16_t idx[MAX_BOOKMARKS];
    uint8_t  count;
    uint8_t  sel;
} BookmarkList;

// One entry per verse: byte offset in the source file + cached reference string
typedef struct {
    uint32_t offset;
    char     ref[REF_LEN];
} VerseIndex;

// A discovered verse file on the SD card
typedef struct {
    char label[24];
    char path[96];
} VerseFile;

typedef struct App {
    Gui*              gui;
    ViewPort*         view_port;
    FuriMessageQueue* queue;
    Storage*          storage;

    bool     running;
    AppView  view;
    AppView  return_view;

    // Verse index — heap-allocated at startup
    VerseIndex* index;
    uint16_t    verse_count;

    // Verse files available on SD
    VerseFile vfiles[8];
    uint8_t   vfile_count;
    uint8_t   vfile_sel;

    // Open file handle (kept open for fast seeking)
    File*     vfile;

    // Currently displayed verse
    int16_t   cur_verse;
    char      cur_ref[REF_LEN];
    WrapState wrap;

    // Main menu
    uint8_t  menu_sel;
    uint8_t  menu_scroll;

    // Browse
    uint16_t browse_sel;
    uint16_t browse_scroll;

    // Search keyboard state
    char        search_buf[MAX_SEARCH_LEN];
    uint8_t     search_len;
    SearchHits  hits;
    uint8_t     kb_row;
    uint8_t     kb_col;
    bool        kb_caps;
    uint8_t     kb_page;
    bool        kb_long_consumed;
    char        kb_suggestion[24];  // auto-suggested book name
    bool        api_input_active;   // true = keyboard feeds API lookup

    // Bookmarks
    BookmarkList bmarks;

    // Settings
    uint8_t settings_sel;
    uint8_t settings_sec;   // 0=Version, 1=Font

    // Active font
    FontChoice font_choice;

    // Error / loading
    char error_msg[48];
    char loading_msg[48];

    // RNG
    uint32_t rng;

    // Verse of the Day
    uint16_t daily_verse_idx;
    uint32_t daily_verse_day;

    // Bible API (online)
    FlipperHTTP* fhttp;
    uint8_t      api_trans_sel;
    char         api_query[64];
    uint8_t      api_query_len;
    char         api_result_ref[48];
    char         api_result_text[512];
    WrapState    api_wrap;
    uint8_t      api_menu_sel;
    uint8_t      api_menu_scroll;
    bool         wifi_connected;
    char         api_status_ssid[33];
    char         api_status_ip[16];
    uint8_t      api_trans_scroll;
    uint8_t      api_book_sel;
    uint8_t      api_chapter_sel;
    uint8_t      api_verse_sel;
    uint8_t      about_scroll;
} App;

// ============================================================
// Shared function declarations
// (defined in bible_viewer.c, called by keyboard.c and others)
// ============================================================

// Drawing primitives
void draw_hdr(Canvas* canvas, const char* title);
void draw_scrollbar(Canvas* canvas, uint16_t pos, uint16_t total, uint8_t vis);
void draw_list_item(Canvas* canvas, uint8_t y, const char* text, bool sel);

// Search
void do_search(App* app);

// Keyboard callbacks (called by keyboard.c)
void kb_submit(App* app);          // GO! pressed on keyboard
void kb_go_back(App* app);         // Back pressed on empty keyboard buffer
void kb_update_suggestion(App* app); // update book-name ghost hint

// Verse navigation (called by keyboard.c results handler)
void open_verse(App* app, uint16_t vi, AppView ret);

#ifdef __cplusplus
}
#endif
