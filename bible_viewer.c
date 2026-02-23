// ============================================================
// Bible Viewer for Flipper Zero
// Verses loaded from SD card text files — very low RAM usage.
//
// SD card layout:
//   /ext/apps_data/bible_viewer/verses_en.txt  (KJV English)
//   /ext/apps_data/bible_viewer/verses_de.txt  (Luther 1912)
//   /ext/apps_data/bible_viewer/bookmarks.txt  (saved bookmarks)
//
// Verse file format (one verse per line):
//   Reference|Book|Verse text
//   e.g.  John 3:16|John|For God so loved the world...
//
// To add a new language: create any verses_XX.txt file in the
// same folder and it will appear automatically in Settings.
//
// Controls:
//   Main Menu:   Up/Down navigate, OK select, Back exit
//   Browse:      Up/Down scroll, Left/Right page, OK view
//   Verse View:  Up/Down scroll, Left/Right prev/next, Long-OK bookmark
//   Search:      D-pad moves keyboard, OK types, Back=backspace
//   Settings:    Up/Down select version, OK apply, Back return
// ============================================================

#include "font/font.h"
#include "flipper_http/flipper_http.h"
#include <furi.h>
#include <furi_hal.h>
#include <gui/gui.h>
#include <gui/elements.h>
#include <input/input.h>
#include <storage/storage.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

// ============================================================
// Constants
// ============================================================

#define SCREEN_W           128
#define SCREEN_H            64
#define MAX_SEARCH_LEN      64
#define MAX_SEARCH_RESULTS  50
#define MAX_BOOKMARKS       75
#define MAX_VERSES         600   // upper bound on verse count per file
#define VISIBLE_LINES        5
#define LINE_H              10
#define HDR_H               12
#define BODY_Y              14
#define WRAP_MAX_LINES       8
#define WRAP_LINE_LEN       32   // wide enough for any font (max ~30 chars)
#define REF_LEN             24   // chars stored for reference display
#define LINE_BUF_LEN       320   // max bytes in one verse line on disk

#define DATA_DIR      "/ext/apps_data/bible_viewer"
#define BM_PATH       DATA_DIR "/bookmarks.txt"
#define SETTINGS_PATH DATA_DIR "/settings.txt"

#define APP_VERSION   "1.3"

#define TAG "BibleViewer"

// ============================================================
// Bible API constants  (bible-api.com — no key required)
// ============================================================

#define API_BASE_URL  "https://bible-api.com/"
#define API_TRANS_COUNT  9
#define BIBLE_BOOKS_COUNT 66
#define API_MENU_ITEMS     7   // number of rows in the API sub-menu

typedef struct {
    const char* code;   // URL param  e.g. "kjv"
    const char* label;  // display    e.g. "King James"
} ApiTranslation;

typedef struct {
    const char* name;   // e.g. "Genesis"
    uint8_t     chapters;
} BibleBook;

static const ApiTranslation API_TRANSLATIONS[API_TRANS_COUNT] = {
    { "web",    "World English"    },
    { "kjv",    "King James"       },
    { "asv",    "American Std"     },
    { "bbe",    "Basic English"    },
    { "darby",  "Darby Bible"      },
    { "dra",    "Douay-Rheims"     },
    { "ylt",    "Young's Literal"  },
    { "webbe",  "WEB British"      },
    { "oeb-us", "Open English US"  },
};

static const BibleBook BIBLE_BOOKS[BIBLE_BOOKS_COUNT] = {
    { "Genesis",          50 }, { "Exodus",           40 }, { "Leviticus",        27 },
    { "Numbers",          36 }, { "Deuteronomy",      34 }, { "Joshua",           24 },
    { "Judges",           21 }, { "Ruth",              4 }, { "1 Samuel",         31 },
    { "2 Samuel",         24 }, { "1 Kings",          22 }, { "2 Kings",          25 },
    { "1 Chronicles",     29 }, { "2 Chronicles",     36 }, { "Ezra",             10 },
    { "Nehemiah",         13 }, { "Esther",           10 }, { "Job",              42 },
    { "Psalms",          150 }, { "Proverbs",         31 }, { "Ecclesiastes",     12 },
    { "Song of Solomon",   8 }, { "Isaiah",           66 }, { "Jeremiah",         52 },
    { "Lamentations",      5 }, { "Ezekiel",          48 }, { "Daniel",           12 },
    { "Hosea",            14 }, { "Joel",              3 }, { "Amos",              9 },
    { "Obadiah",           1 }, { "Jonah",             4 }, { "Micah",             7 },
    { "Nahum",             3 }, { "Habakkuk",          3 }, { "Zephaniah",         3 },
    { "Haggai",            2 }, { "Zechariah",        14 }, { "Malachi",           4 },
    { "Matthew",          28 }, { "Mark",             16 }, { "Luke",             24 },
    { "John",             21 }, { "Acts",             28 }, { "Romans",           16 },
    { "1 Corinthians",    16 }, { "2 Corinthians",    13 }, { "Galatians",         6 },
    { "Ephesians",         6 }, { "Philippians",       4 }, { "Colossians",        4 },
    { "1 Thessalonians",   5 }, { "2 Thessalonians",   3 }, { "1 Timothy",         6 },
    { "2 Timothy",         4 }, { "Titus",             3 }, { "Philemon",          1 },
    { "Hebrews",          13 }, { "James",             5 }, { "1 Peter",           5 },
    { "2 Peter",           3 }, { "1 John",            5 }, { "2 John",            1 },
    { "3 John",            1 }, { "Jude",              1 }, { "Revelation",       22 },
};

// Verse counts for every chapter of every book (1189 entries, one per chapter).
// Stored as a flat uint8_t array; VERSE_COUNT_OFFSET[] gives the start index
// for each book so we can look up any chapter in O(1).
static const uint8_t VERSE_COUNTS[] = {
    // Genesis
    31,25,24,26,32,22,24,22,29,32,
    32,20,18,24,21,16,27,33,38,18,
    34,24,20,67,34,35,46,22,35,43,
    55,32,20,31,29,43,36,30,23,23,
    57,38,34,34,28,34,31,22,33,26,
    // Exodus
    22,25,22,31,23,30,25,32,35,29,
    10,51,22,31,27,36,16,27,25,26,
    36,31,33,18,40,37,21,43,46,38,
    18,35,23,35,35,38,29,31,43,38,
    // Leviticus
    17,16,17,35,19,30,38,36,24,20,
    47,8,59,57,33,34,16,30,24,16,
    15,49,52,45,23,26,20,
    // Numbers
    54,34,51,49,31,27,89,26,23,36,
    35,16,33,45,41,50,13,32,22,29,
    35,41,30,25,18,65,23,31,40,16,
    54,42,56,29,34,13,
    // Deuteronomy
    46,37,29,49,33,25,26,20,29,22,
    32,32,18,29,23,22,20,22,21,20,
    23,30,25,22,19,19,26,68,29,20,
    30,52,29,12,
    // Joshua
    18,24,17,24,15,27,26,35,27,43,
    23,24,33,15,63,10,18,28,51,9,
    45,34,16,33,
    // Judges
    36,23,31,24,31,40,25,35,57,18,
    40,15,25,20,20,31,13,31,30,48,
    25,
    // Ruth
    22,23,18,22,
    // 1 Samuel
    28,36,21,22,12,21,17,22,27,27,
    15,25,23,52,35,23,58,30,24,42,
    15,23,29,22,44,25,12,25,11,31,
    13,
    // 2 Samuel
    27,32,39,12,25,23,29,18,13,19,
    27,31,39,33,37,23,29,33,43,26,
    22,51,39,25,
    // 1 Kings
    53,46,28,34,18,38,51,66,28,29,
    43,33,34,31,34,34,24,46,21,43,
    29,53,
    // 2 Kings
    18,25,27,44,27,33,20,29,37,36,
    21,21,25,29,38,20,41,37,37,21,
    26,20,37,20,30,
    // 1 Chronicles
    54,55,24,43,26,81,40,40,44,14,
    47,40,14,17,29,43,27,17,19,8,
    30,19,32,31,31,32,34,21,30,
    // 2 Chronicles
    17,18,17,22,14,42,22,18,31,19,
    23,16,22,15,19,14,19,34,11,37,
    20,12,21,27,28,23,9,27,36,27,
    21,33,25,33,27,23,
    // Ezra
    11,70,13,24,17,22,28,36,15,44,
    // Nehemiah
    11,20,32,23,19,19,73,18,38,39,
    36,47,31,
    // Esther
    22,23,15,17,14,14,10,17,32,3,
    // Job
    22,13,26,21,27,30,21,22,35,22,
    20,25,28,22,35,22,16,21,29,29,
    34,30,17,25,6,14,23,28,25,31,
    40,22,33,37,16,33,24,41,30,24,
    34,17,
    // Psalms
    6,12,8,8,12,10,17,9,20,18,
    7,8,6,7,5,11,15,50,14,9,
    13,31,6,10,22,12,14,9,11,12,
    24,11,22,22,28,12,40,22,13,17,
    13,11,5,20,28,22,35,22,46,18,
    16,18,12,5,12,20,12,23,11,13,
    12,9,9,5,8,28,22,35,45,48,
    43,13,31,7,10,10,9,8,18,19,
    2,29,18,7,8,9,4,8,5,6,
    5,6,8,8,3,18,3,3,21,26,
    9,8,24,13,10,7,12,15,21,10,
    20,14,9,6,7,23,11,13,176,9,
    8,9,4,8,5,6,5,3,8,8,
    3,18,3,3,21,26,9,8,24,13,
    10,7,12,15,21,10,20,14,9,6,
    // Proverbs
    33,22,35,27,23,35,27,36,18,32,
    31,28,25,35,33,33,28,24,29,30,
    31,29,35,34,28,28,27,28,27,33,
    31,
    // Ecclesiastes
    18,26,22,16,20,12,29,17,18,20,
    10,14,
    // Song of Solomon
    17,17,11,16,16,13,13,14,
    // Isaiah
    31,22,26,6,30,13,25,22,21,34,
    16,6,22,32,9,14,14,7,25,6,
    17,25,18,23,12,21,13,29,24,33,
    9,20,24,17,10,22,38,22,8,31,
    29,25,28,28,25,13,15,22,26,11,
    23,15,12,17,13,12,21,14,21,22,
    11,12,19,12,25,24,
    // Jeremiah
    19,37,25,31,31,30,34,22,26,25,
    23,17,27,22,21,21,27,23,15,18,
    14,30,40,10,38,24,22,17,32,24,
    40,44,26,22,19,32,21,28,18,16,
    18,22,13,30,5,28,7,47,39,46,
    64,34,
    // Lamentations
    22,22,66,22,22,
    // Ezekiel
    28,10,27,17,17,14,27,18,11,22,
    25,28,23,23,8,63,24,32,14,49,
    32,31,49,27,17,21,36,26,21,26,
    18,32,33,31,15,38,28,23,29,49,
    26,20,27,31,25,24,23,35,
    // Daniel
    21,49,30,37,31,28,28,27,27,21,
    45,13,
    // Hosea
    11,23,5,19,15,11,16,14,17,15,
    12,14,16,9,
    // Joel
    20,32,21,
    // Amos
    15,16,15,13,27,14,17,14,15,
    // Obadiah
    21,
    // Jonah
    17,10,10,11,
    // Micah
    16,13,12,13,15,16,20,
    // Nahum
    15,13,19,
    // Habakkuk
    17,20,19,
    // Zephaniah
    18,15,20,
    // Haggai
    15,23,
    // Zechariah
    21,13,10,14,11,15,14,23,17,12,
    17,14,9,21,
    // Malachi
    14,17,18,6,
    // Matthew
    25,23,17,25,48,34,29,34,38,42,
    30,50,58,36,39,28,27,35,30,34,
    46,46,39,51,46,75,66,20,
    // Mark
    45,28,35,41,43,56,37,38,50,52,
    33,44,37,72,47,20,
    // Luke
    80,52,38,44,39,49,50,56,62,42,
    54,59,35,35,32,31,37,43,48,47,
    38,71,56,53,
    // John
    51,25,36,54,47,71,53,59,41,42,
    57,50,38,31,27,33,26,40,42,31,
    25,
    // Acts
    26,47,26,37,42,15,60,40,43,48,
    30,25,52,28,41,40,34,28,41,38,
    40,30,35,27,27,32,44,31,
    // Romans
    32,29,31,25,21,23,25,39,33,21,
    36,21,14,26,33,24,
    // 1 Corinthians
    31,16,23,21,13,20,40,13,27,33,
    34,31,13,40,58,24,
    // 2 Corinthians
    24,17,18,18,21,18,16,24,15,18,
    33,21,14,
    // Galatians
    24,21,29,31,26,18,
    // Ephesians
    23,22,21,28,30,14,
    // Philippians
    30,30,21,23,
    // Colossians
    29,23,25,18,
    // 1 Thessalonians
    10,20,13,18,28,
    // 2 Thessalonians
    12,17,18,
    // 1 Timothy
    20,15,16,16,25,21,
    // 2 Timothy
    18,26,17,22,
    // Titus
    16,15,15,
    // Philemon
    25,
    // Hebrews
    14,18,19,16,14,20,28,13,28,39,
    40,29,25,
    // James
    27,26,18,17,20,
    // 1 Peter
    25,25,22,19,14,
    // 2 Peter
    21,22,18,
    // 1 John
    10,29,24,21,21,
    // 2 John
    13,
    // 3 John
    14,
    // Jude
    25,
    // Revelation
    20,29,22,11,14,17,17,13,21,11,
    19,17,18,20,8,21,18,24,21,15,
    27,21,
};

// Start index in VERSE_COUNTS[] for the first chapter of each book (66 entries)
static const uint16_t VERSE_COUNT_OFFSET[BIBLE_BOOKS_COUNT] = {
    0,    50,   90,   117,  153,  187,  211,  232,  236,  267,
    291,  313,  338,  367,  403,  413,  426,  436,  478,  628,
    659,  671,  679,  745,  797,  802,  850,  862,  876,  879,
    888,  889,  893,  900,  903,  906,  909,  911,  925,  929,
    957,  973,  997,  1018, 1046, 1062, 1078, 1091, 1097, 1103,
    1107, 1111, 1116, 1119, 1125, 1129, 1132, 1133, 1146, 1151,
    1156, 1159, 1164, 1165, 1166, 1167
};

// Return the number of verses in a given book (0-based) and chapter (1-based).
static inline uint8_t book_chapter_verses(uint8_t book, uint8_t chapter) {
    return VERSE_COUNTS[VERSE_COUNT_OFFSET[book] + chapter - 1];
}

// Static App pointer used by the zero-arg FlipperHTTP callbacks
static void* g_app_ptr = NULL;

// ============================================================
// Font configuration
//
// Each font needs its own:
//   chars_per_line  — how many characters fit across the verse body
//   line_h          — pixel height of one rendered line
//
// FontSecondary is the Flipper built-in 5x8 bitmap font.
// The four custom sizes come from font/font.c (u8g2 bitmaps):
//   SMALL  = 4x6,  MEDIUM = 5x8,  LARGE = 6x10,  XLARGE = 9x15
// ============================================================

typedef enum {
    FONT_SECONDARY = 0,  // Flipper built-in  (~5px wide, 8px tall)
    FONT_SMALL     = 1,  // custom 4x6
    FONT_MEDIUM    = 2,  // custom 5x8
    FONT_LARGE     = 3,  // custom 6x10
    FONT_XLARGE    = 4,  // custom 9x15
    FONT_COUNT     = 5,
} FontChoice;

// chars_per_line: how many ASCII chars fit in (SCREEN_W - 6) pixels
// line_h:         pixel height of one rendered line of verse text
static const uint8_t FONT_CHARS[FONT_COUNT]  = { 22, 30, 24, 20, 13 };
static const uint8_t FONT_LINE_H[FONT_COUNT] = { 10,  8, 10, 12, 16 };
static const char* const FONT_LABELS[FONT_COUNT] = {
    "Default (built-in)",
    "Tiny  (4x6)",
    "Small (5x8)",
    "Medium (6x10)",
    "Large (9x15)",
};

// How many verse lines fit on screen for each font (body = 50px)
static inline uint8_t font_visible_lines(FontChoice f) {
    return (uint8_t)((SCREEN_H - HDR_H - 2) / FONT_LINE_H[f]);
}

// Apply the chosen font to the canvas
static inline void apply_verse_font(Canvas* canvas, FontChoice f) {
    switch(f) {
    case FONT_SMALL:   canvas_set_font_custom(canvas, FONT_SIZE_SMALL);  break;
    case FONT_MEDIUM:  canvas_set_font_custom(canvas, FONT_SIZE_MEDIUM); break;
    case FONT_LARGE:   canvas_set_font_custom(canvas, FONT_SIZE_LARGE);  break;
    case FONT_XLARGE:  canvas_set_font_custom(canvas, FONT_SIZE_XLARGE); break;
    default:           canvas_set_font(canvas, FontSecondary);           break;
    }
}

// ============================================================
// Types
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
    // ── Bible API (online) ──────────────────────
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

typedef struct {
    char    lines[WRAP_MAX_LINES][WRAP_LINE_LEN + 1];
    uint8_t count;
    uint8_t scroll;
} WrapState;

typedef struct {
    uint16_t idx[MAX_SEARCH_RESULTS];
    uint8_t  count;
    uint8_t  sel;
} SearchHits;

typedef struct {
    uint16_t idx[MAX_BOOKMARKS];
    uint8_t  count;
    uint8_t  sel;
} BookmarkList;

// Verse index entry: one per verse, built by scanning the file once.
typedef struct {
    uint32_t offset;        // byte offset of this line in the file
    char     ref[REF_LEN];  // cached reference string e.g. "John 3:16"
} VerseIndex;

// A discovered verse file on the SD card.
typedef struct {
    char label[24];         // display name shown in Settings
    char path[96];          // full SD path
} VerseFile;

typedef struct {
    Gui*              gui;
    ViewPort*         view_port;
    FuriMessageQueue* queue;
    Storage*          storage;

    bool     running;
    AppView  view;
    AppView  return_view;

    // Verse index — allocated on heap at startup
    VerseIndex* index;       // MAX_VERSES entries
    uint16_t    verse_count;

    // Verse files available on SD
    VerseFile vfiles[8];
    uint8_t   vfile_count;
    uint8_t   vfile_sel;     // currently active index

    // Open file handle for verse data (kept open for fast seeking)
    File*     vfile;

    // Currently displayed verse
    int16_t   cur_verse;
    char      cur_ref[REF_LEN];
    WrapState wrap;

    // Menu
    uint8_t  menu_sel;
    uint8_t  menu_scroll;

    // Browse
    uint16_t browse_sel;
    uint16_t browse_scroll;

    // Search
    char        search_buf[MAX_SEARCH_LEN];
    uint8_t     search_len;
    SearchHits  hits;
    uint8_t     kb_row;
    uint8_t     kb_col;
    bool        kb_caps;
    uint8_t     kb_page;
    bool        kb_long_consumed;
    char        kb_suggestion[24]; // auto-suggested book name, or "" if none

    // Bookmarks
    BookmarkList bmarks;

    // Settings: highlighted row (split into two sections: version + font)
    uint8_t settings_sel;    // index within the currently focused section
    uint8_t settings_sec;    // 0 = Bible Version section, 1 = Font Size section

    // Active font for verse display
    FontChoice font_choice;

    // Error / loading display
    char error_msg[48];
    char loading_msg[48];   // shown on the loading screen

    // RNG
    uint32_t rng;

    // Verse of the Day — random index persisted so it stays stable across restarts
    uint16_t daily_verse_idx;   // saved verse index for today
    uint32_t daily_verse_day;   // day-counter when daily_verse_idx was chosen

    // ── Bible API (online) ──────────────────────────────────
    FlipperHTTP* fhttp;               // NULL when not in API section
    uint8_t      api_trans_sel;       // index into API_TRANSLATIONS[]
    char         api_query[64];       // reference typed by user
    uint8_t      api_query_len;       // chars used in api_query
    char         api_result_ref[48];  // "John 3:16" from JSON response
    char         api_result_text[512];// verse text from JSON response
    WrapState    api_wrap;            // wrapped api_result_text for display
    uint8_t      api_menu_sel;        // API sub-menu cursor position (0-6)
    uint8_t      api_menu_scroll;     // API sub-menu scroll offset
    // WiFi Status screen
    bool         wifi_connected;       // true only after a successful PING/PONG
    char         api_status_ssid[33]; // last fetched SSID (max 32 chars + NUL)
    char         api_status_ip[16];   // last fetched IP address
    uint8_t      api_trans_scroll;    // translation list scroll offset
    bool         api_input_active;    // true = keyboard feeds API lookup
    // Quick-picker state
    uint8_t      api_book_sel;        // 0-65, index into BIBLE_BOOKS
    uint8_t      api_chapter_sel;     // 1-based chapter number
    uint8_t      api_verse_sel;       // 1-based verse number
    // About screen
    uint8_t      about_scroll;        // scroll offset for about lines
} App;

// ============================================================
// Keyboard layout  (unchanged from previous version)
// ============================================================

static const char* const kb_page0[] = {
    "qwertyuiop789",
    "asdfghjkl:456",
    "zxcvbnm.-0123",
};
static const char* const kb_page1[] = {
    "!@#$%^&*()_+=",
    "<>?/\\|~`'\"{}",
    ";,[]- ...     ",
};
#define KB_NROWS   3
#define KB_NCOLS  13
#define KB_NPAGES  3

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

static const char* kb_key_label(App* app, uint8_t row, uint8_t col) {
    static char buf[4];
    if(app->kb_page == 0) {
        char ch = kb_page0[row][col];
        if(app->kb_caps && ch >= 'a' && ch <= 'z') ch -= 32;
        buf[0] = ch; buf[1] = '\0'; return buf;
    }
    if(app->kb_page == 1) { buf[0] = kb_page1[row][col]; buf[1] = '\0'; return buf; }
    return kb_page2[row][col].label;
}

static const char* kb_key_bytes(App* app, uint8_t row, uint8_t col) {
    static char buf[4];
    if(app->kb_page == 0) {
        char ch = kb_page0[row][col];
        if(app->kb_caps && ch >= 'a' && ch <= 'z') ch -= 32;
        buf[0] = ch; buf[1] = '\0'; return buf;
    }
    if(app->kb_page == 1) { buf[0] = kb_page1[row][col]; buf[1] = '\0'; return buf; }
    return kb_page2[row][col].label;
}

// ============================================================
// General utilities
// ============================================================

static bool icontains(const char* hay, const char* needle) {
    if(!hay || !needle || !needle[0]) return false;
    size_t nlen = strlen(needle), hlen = strlen(hay);
    if(nlen > hlen) return false;
    for(size_t i = 0; i <= hlen - nlen; i++) {
        bool ok = true;
        for(size_t j = 0; j < nlen; j++) {
            char a = hay[i+j], b = needle[j];
            if(a >= 'A' && a <= 'Z') a += 32;
            if(b >= 'A' && b <= 'Z') b += 32;
            if(a != b) { ok = false; break; }
        }
        if(ok) return true;
    }
    return false;
}

static uint32_t rng_next(uint32_t* s) {
    uint32_t x = *s;
    x ^= x << 13; x ^= x >> 17; x ^= x << 5;
    *s = x; return x;
}

static void word_wrap(WrapState* w, const char* text, uint8_t cols) {
    memset(w, 0, sizeof(WrapState));
    size_t len = strlen(text), pos = 0;
    if(cols < 1) cols = 1;
    if(cols > WRAP_LINE_LEN) cols = WRAP_LINE_LEN;
    while(pos < len && w->count < WRAP_MAX_LINES) {
        size_t rem = len - pos;
        if(rem <= cols) {
            memcpy(w->lines[w->count], text + pos, rem);
            w->lines[w->count++][rem] = '\0';
            break;
        }
        size_t brk = cols;
        while(brk > 0 && text[pos + brk] != ' ') brk--;
        if(!brk) brk = cols;
        memcpy(w->lines[w->count], text + pos, brk);
        w->lines[w->count++][brk] = '\0';
        pos += brk;
        if(pos < len && text[pos] == ' ') pos++;
    }
}

// ============================================================
// SD card I/O helpers
// ============================================================

// Read one line from the currently-open vfile, starting at the current
// seek position. Strips \r\n. Returns number of chars written (0 = EOF).
static uint16_t read_line(App* app, char* buf, uint16_t buf_sz) {
    uint16_t li = 0;
    while(li < buf_sz - 1) {
        char ch;
        if(storage_file_read(app->vfile, &ch, 1) == 0) break;
        if(ch == '\r') continue;
        if(ch == '\n') break;
        buf[li++] = ch;
    }
    buf[li] = '\0';
    return li;
}

// Parse a pipe-delimited verse line into its three fields.
// Returns false if malformed.
static bool parse_line(const char* line,
                       char* ref,  size_t ref_sz,
                       char* book, size_t book_sz,
                       char* text, size_t text_sz) {
    const char* p1 = strchr(line, '|');
    if(!p1) return false;
    const char* p2 = strchr(p1 + 1, '|');
    if(!p2) return false;

    size_t rlen = (size_t)(p1 - line);
    size_t blen = (size_t)(p2 - p1 - 1);
    size_t tlen = strlen(p2 + 1);

    if(ref)  { size_t n = rlen < ref_sz-1  ? rlen : ref_sz-1;  memcpy(ref,  line,   n); ref[n]  = '\0'; }
    if(book) { size_t n = blen < book_sz-1 ? blen : book_sz-1; memcpy(book, p1+1,   n); book[n] = '\0'; }
    if(text) { size_t n = tlen < text_sz-1 ? tlen : text_sz-1; memcpy(text, p2+1,   n); text[n] = '\0'; }
    return true;
}

// Open the file for the current vfile_sel index.
static bool open_verse_file(App* app) {
    if(app->vfile) {
        storage_file_close(app->vfile);
        storage_file_free(app->vfile);
        app->vfile = NULL;
    }
    if(!app->vfile_count) return false;

    app->vfile = storage_file_alloc(app->storage);
    if(!storage_file_open(app->vfile,
            app->vfiles[app->vfile_sel].path,
            FSAM_READ, FSOM_OPEN_EXISTING)) {
        storage_file_free(app->vfile);
        app->vfile = NULL;
        return false;
    }
    return true;
}

// Scan the file to build the offset index.
// O(N) single pass; never holds more than one line in RAM.
static bool build_index(App* app) {
    app->verse_count = 0;
    if(!app->vfile) return false;
    storage_file_seek(app->vfile, 0, true);

    char line[LINE_BUF_LEN];
    uint32_t offset = 0;

    while(app->verse_count < MAX_VERSES) {
        uint32_t line_start = offset;

        // Read byte-by-byte to track the exact start offset of each line
        uint16_t li = 0;
        bool eof = false;
        while(li < sizeof(line) - 1) {
            char ch;
            if(storage_file_read(app->vfile, &ch, 1) == 0) { eof = true; break; }
            offset++;
            if(ch == '\r') continue;
            if(ch == '\n') break;
            line[li++] = ch;
        }
        line[li] = '\0';
        if(li == 0 && eof) break;
        if(li == 0) continue; // blank line

        // Parse only the reference (first pipe-delimited field)
        const char* p = strchr(line, '|');
        if(!p) { if(eof) break; continue; }

        VerseIndex* vi = &app->index[app->verse_count];
        vi->offset = line_start;
        size_t rlen = (size_t)(p - line);
        if(rlen >= REF_LEN) rlen = REF_LEN - 1;
        memcpy(vi->ref, line, rlen);
        vi->ref[rlen] = '\0';
        app->verse_count++;

        if(eof) break;
    }
    return app->verse_count > 0;
}

// Read the full text of verse idx from the file by seeking to its offset.
static bool read_verse_text(App* app, uint16_t idx, char* buf, size_t buf_sz) {
    if(!app->vfile || idx >= app->verse_count) {
        if(buf_sz) buf[0] = '\0';
        return false;
    }
    storage_file_seek(app->vfile, app->index[idx].offset, true);
    char line[LINE_BUF_LEN];
    read_line(app, line, sizeof(line));
    char ref[REF_LEN], book[32];
    return parse_line(line, ref, sizeof(ref), book, sizeof(book), buf, buf_sz);
}

// Discover verse files on SD by checking for the two standard names,
// then scanning for any other verses_*.txt in the data directory.
static void discover_verse_files(App* app) {
    app->vfile_count = 0;

    // Check for the two canonical files first, in preferred order
    const struct { const char* path; const char* label; } known[] = {
        { DATA_DIR "/verses_en.txt",  "KJV (English)"    },
        { DATA_DIR "/verses_esv.txt", "ESV (English)"    },
        { DATA_DIR "/verses_de.txt",  "Luther 1912 (DE)" },
    };
    for(size_t i = 0; i < 3 && app->vfile_count < 8; i++) {
        File* f = storage_file_alloc(app->storage);
        bool ok = storage_file_open(f, known[i].path, FSAM_READ, FSOM_OPEN_EXISTING);
        storage_file_close(f);
        storage_file_free(f);
        if(ok) {
            VerseFile* vf = &app->vfiles[app->vfile_count++];
            strncpy(vf->label, known[i].label, sizeof(vf->label) - 1);
            strncpy(vf->path,  known[i].path,  sizeof(vf->path)  - 1);
        }
    }

    // Scan directory for any additional verses_*.txt files
    File* dir = storage_file_alloc(app->storage);
    if(storage_dir_open(dir, DATA_DIR)) {
        FileInfo fi;
        char fname[64];
        while(storage_dir_read(dir, &fi, fname, sizeof(fname))) {
            // Skip directories and already-added canonical files
            if(fi.flags & FSF_DIRECTORY) continue;
            if(strncmp(fname, "verses_", 7) != 0) continue;
            size_t flen = strlen(fname);
            if(flen < 11) continue; // "verses_x.txt" minimum
            if(strcmp(fname + flen - 4, ".txt") != 0) continue;
            // Skip the canonical files already added
            if(strcmp(fname, "verses_en.txt")  == 0) continue;
            if(strcmp(fname, "verses_esv.txt") == 0) continue;
            if(strcmp(fname, "verses_de.txt")  == 0) continue;
            if(app->vfile_count >= 8) break;

            VerseFile* vf = &app->vfiles[app->vfile_count++];
            // Build label from filename: "verses_xx.txt" -> "Custom (xx)"
            char code[8] = {0};
            size_t code_len = flen - 7 - 4; // between "verses_" and ".txt"
            if(code_len > 7) code_len = 7;
            memcpy(code, fname + 7, code_len);
            snprintf(vf->label, sizeof(vf->label), "Custom (%s)", code);
            snprintf(vf->path,  sizeof(vf->path),  "%s/%s", DATA_DIR, fname);
        }
        storage_dir_close(dir);
    }
    storage_file_free(dir);
}

// Switch to a different verse file, rebuilding the index.
static bool switch_verse_file(App* app, uint8_t new_sel) {
    if(new_sel >= app->vfile_count) return false;
    app->vfile_sel = new_sel;
    if(!open_verse_file(app)) return false;
    return build_index(app);
}

// ============================================================
// Bookmarks — persisted to /ext/apps_data/bible_viewer/bookmarks.txt
// ============================================================

static void bmarks_save(App* app) {
    File* f = storage_file_alloc(app->storage);
    if(!storage_file_open(f, BM_PATH, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        storage_file_free(f); return;
    }
    for(uint8_t i = 0; i < app->bmarks.count; i++) {
        char buf[8];
        int len = snprintf(buf, sizeof(buf), "%u\n", app->bmarks.idx[i]);
        if(len > 0) storage_file_write(f, buf, (uint16_t)len);
    }
    storage_file_close(f);
    storage_file_free(f);
}

static void bmarks_load(App* app) {
    app->bmarks.count = 0;
    File* f = storage_file_alloc(app->storage);
    if(!storage_file_open(f, BM_PATH, FSAM_READ, FSOM_OPEN_EXISTING)) {
        storage_file_free(f); return;
    }
    char buf[8]; uint8_t bi = 0;
    while(app->bmarks.count < MAX_BOOKMARKS) {
        char ch;
        if(storage_file_read(f, &ch, 1) == 0) {
            if(bi > 0) {
                buf[bi] = '\0';
                uint16_t v = (uint16_t)atoi(buf);
                if(v < app->verse_count) app->bmarks.idx[app->bmarks.count++] = v;
            }
            break;
        }
        if(ch == '\n' || ch == '\r') {
            if(bi > 0) {
                buf[bi] = '\0';
                uint16_t v = (uint16_t)atoi(buf);
                if(v < app->verse_count) app->bmarks.idx[app->bmarks.count++] = v;
                bi = 0;
            }
        } else if(bi < (uint8_t)(sizeof(buf) - 1)) {
            buf[bi++] = ch;
        }
    }
    storage_file_close(f);
    storage_file_free(f);
}

static bool is_bookmarked(App* app, uint16_t vi) {
    for(uint8_t i = 0; i < app->bmarks.count; i++)
        if(app->bmarks.idx[i] == vi) return true;
    return false;
}

static void toggle_bmark(App* app, uint16_t vi) {
    for(uint8_t i = 0; i < app->bmarks.count; i++) {
        if(app->bmarks.idx[i] == vi) {
            for(uint8_t j = i; j < app->bmarks.count - 1; j++)
                app->bmarks.idx[j] = app->bmarks.idx[j + 1];
            app->bmarks.count--;
            bmarks_save(app);
            return;
        }
    }
    if(app->bmarks.count < MAX_BOOKMARKS) {
        app->bmarks.idx[app->bmarks.count++] = vi;
        bmarks_save(app);
    }
}

// ============================================================
// Settings — persisted to /ext/apps_data/bible_viewer/settings.txt
//
// Format: one "key=value" pair per line.  Easy to extend with
// future settings by adding more keys.
//
//   verse_file=verses_en.txt
//
// ============================================================

static void settings_save(App* app) {
    File* f = storage_file_alloc(app->storage);
    if(!storage_file_open(f, SETTINGS_PATH, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        storage_file_free(f); return;
    }

    // Extract just the filename from the active path for portability
    const char* path = app->vfiles[app->vfile_sel].path;
    const char* slash = strrchr(path, '/');
    const char* fname = slash ? slash + 1 : path;

    char buf[96];
    int len = snprintf(buf, sizeof(buf), "verse_file=%s\n", fname);
    if(len > 0) storage_file_write(f, buf, (uint16_t)len);

    len = snprintf(buf, sizeof(buf), "font_size=%d\n", (int)app->font_choice);
    if(len > 0) storage_file_write(f, buf, (uint16_t)len);

    len = snprintf(buf, sizeof(buf), "api_trans=%s\n",
        API_TRANSLATIONS[app->api_trans_sel].code);
    if(len > 0) storage_file_write(f, buf, (uint16_t)len);

    len = snprintf(buf, sizeof(buf), "api_book=%u\n",
        (unsigned)app->api_book_sel);
    if(len > 0) storage_file_write(f, buf, (uint16_t)len);

    len = snprintf(buf, sizeof(buf), "api_chapter=%u\n",
        (unsigned)app->api_chapter_sel);
    if(len > 0) storage_file_write(f, buf, (uint16_t)len);

    len = snprintf(buf, sizeof(buf), "api_verse=%u\n",
        (unsigned)app->api_verse_sel);
    if(len > 0) storage_file_write(f, buf, (uint16_t)len);

    len = snprintf(buf, sizeof(buf), "daily_idx=%u\n",
        (unsigned)app->daily_verse_idx);
    if(len > 0) storage_file_write(f, buf, (uint16_t)len);

    len = snprintf(buf, sizeof(buf), "daily_day=%lu\n",
        (unsigned long)app->daily_verse_day);
    if(len > 0) storage_file_write(f, buf, (uint16_t)len);

    // Future settings can be appended here, e.g.:
    // snprintf(buf, sizeof(buf), "brightness=%d\n", app->brightness);
    // storage_file_write(f, buf, strlen(buf));

    storage_file_close(f);
    storage_file_free(f);
}

// Loads all settings from SETTINGS_PATH, applying them directly to app.
// Safe to call before or after verse files are discovered (verse_file match
// is attempted; if not found app->vfile_sel stays 0).
static void settings_load(App* app) {
    File* f = storage_file_alloc(app->storage);
    if(!storage_file_open(f, SETTINGS_PATH, FSAM_READ, FSOM_OPEN_EXISTING)) {
        storage_file_free(f); return;
    }

    char line[96];
    bool done = false;

    while(!done) {
        uint16_t li = 0;
        while(li < sizeof(line) - 1) {
            char ch;
            if(storage_file_read(f, &ch, 1) == 0) { done = true; break; }
            if(ch == '\r') continue;
            if(ch == '\n') break;
            line[li++] = ch;
        }
        line[li] = '\0';
        if(li == 0) continue;

        char* eq = strchr(line, '=');
        if(!eq) continue;
        *eq = '\0';
        const char* key = line;
        const char* val = eq + 1;

        if(strcmp(key, "verse_file") == 0) {
            for(uint8_t i = 0; i < app->vfile_count; i++) {
                const char* p  = app->vfiles[i].path;
                const char* sl = strrchr(p, '/');
                const char* fn = sl ? sl + 1 : p;
                if(strcmp(fn, val) == 0) { app->vfile_sel = i; break; }
            }
        } else if(strcmp(key, "font_size") == 0) {
            int v = atoi(val);
            if(v >= 0 && v < FONT_COUNT)
                app->font_choice = (FontChoice)v;
        } else if(strcmp(key, "api_trans") == 0) {
            for(uint8_t i = 0; i < API_TRANS_COUNT; i++) {
                if(strcmp(val, API_TRANSLATIONS[i].code) == 0) {
                    app->api_trans_sel = i;
                    break;
                }
            }
        } else if(strcmp(key, "api_book") == 0) {
            int v = atoi(val);
            if(v >= 0 && v < BIBLE_BOOKS_COUNT)
                app->api_book_sel = (uint8_t)v;
        } else if(strcmp(key, "api_chapter") == 0) {
            int v = atoi(val);
            if(v >= 1 && v <= 150)
                app->api_chapter_sel = (uint8_t)v;
        } else if(strcmp(key, "api_verse") == 0) {
            int v = atoi(val);
            if(v >= 1 && v <= 176)
                app->api_verse_sel = (uint8_t)v;
        } else if(strcmp(key, "daily_idx") == 0) {
            int v = atoi(val);
            if(v >= 0 && v < MAX_VERSES)
                app->daily_verse_idx = (uint16_t)v;
        } else if(strcmp(key, "daily_day") == 0) {
            unsigned long v = strtoul(val, NULL, 10);
            app->daily_verse_day = (uint32_t)v;
        }
        // Future keys: add else-if blocks here
    }

    storage_file_close(f);
    storage_file_free(f);
}



static void open_verse(App* app, uint16_t vi, AppView ret) {
    app->cur_verse   = (int16_t)vi;
    app->return_view = ret;
    // Reference is already cached in the index
    strncpy(app->cur_ref, app->index[vi].ref, sizeof(app->cur_ref) - 1);
    // Fetch full text from SD (one seek + read)
    char text[LINE_BUF_LEN];
    if(!read_verse_text(app, vi, text, sizeof(text)))
        strncpy(text, "(read error)", sizeof(text));
    word_wrap(&app->wrap, text, FONT_CHARS[app->font_choice]);
    app->view = ViewVerseRead;
}

// ============================================================
// Search — streams file line-by-line, never holds all text in RAM
// ============================================================

static void do_search(App* app) {
    app->hits.count = 0;
    app->hits.sel   = 0;
    if(!app->search_len || !app->vfile) return;

    storage_file_seek(app->vfile, 0, true);
    char line[LINE_BUF_LEN];
    uint16_t verse_num = 0;

    while(verse_num < app->verse_count && app->hits.count < MAX_SEARCH_RESULTS) {
        uint16_t len = read_line(app, line, sizeof(line));
        if(len == 0) {
            // Try one more byte to distinguish blank line vs EOF
            char ch;
            if(storage_file_read(app->vfile, &ch, 1) == 0) break;
            // Blank line — just skip, don't count as a verse
            continue;
        }
        // icontains searches the whole raw line (ref|book|text), which is fine
        if(icontains(line, app->search_buf))
            app->hits.idx[app->hits.count++] = verse_num;
        verse_num++;
    }
}


// ============================================================
// Bible API helpers
// ============================================================

// Extract a JSON string value by key.
// Finds the LAST occurrence of "key":" to avoid hitting
// the same key inside nested objects (e.g. "text" inside "verses":[]).
// Copies the value into out, unescapes \n -> space, strips trailing whitespace.
static bool json_extract_str(const char* json, const char* key,
                              char* out, size_t out_sz) {
    if(!json || !key || !out || out_sz < 2) return false;

    char pat[64];
    snprintf(pat, sizeof(pat), "\"%s\":\"", key);
    size_t plen = strlen(pat);

    // Find the LAST occurrence of the pattern
    const char* found = NULL;
    const char* p = json;
    while((p = strstr(p, pat)) != NULL) {
        found = p;
        p += plen;
    }
    if(!found) return false;

    const char* start = found + plen;
    size_t wi = 0;
    bool escaped = false;
    for(const char* c = start; *c && wi < out_sz - 1; c++) {
        if(escaped) {
            if(*c == 'n' || *c == 'r') out[wi++] = ' ';
            else if(*c == '"')         out[wi++] = '"';
            else if(*c == '\\')        out[wi++] = '\\';
            else { if(wi < out_sz-2) { out[wi++] = '\\'; out[wi++] = *c; } }
            escaped = false;
        } else if(*c == '\\') {
            escaped = true;
        } else if(*c == '"') {
            break;
        } else {
            out[wi++] = *c;
        }
    }
    out[wi] = '\0';
    while(wi > 0 && (out[wi-1] == ' ' || out[wi-1] == '\n' || out[wi-1] == '\r'))
        out[--wi] = '\0';
    return wi > 0;
}

// Replace spaces with '+' for URL construction.
static void api_url_encode(const char* src, char* dst, size_t dst_sz) {
    size_t di = 0;
    for(size_t i = 0; src[i] && di < dst_sz - 1; i++)
        dst[di++] = (src[i] == ' ') ? '+' : src[i];
    dst[di] = '\0';
}

// Allocate FlipperHTTP if not already done.
static void api_ensure_fhttp(App* app) {
    if(!app->fhttp)
        app->fhttp = flipper_http_alloc();
    if(!app->fhttp) { app->wifi_connected = false; return; }
    // Detect board presence via PING/PONG.
    // send_data() always resets state=IDLE after transmitting, so we manually
    // force state=INACTIVE after the send — the rx callback will flip it to
    // IDLE only when a real [PONG] arrives from the board.
    flipper_http_send_data(app->fhttp, "[PING]");
    app->fhttp->state = INACTIVE; // override the IDLE set by send_data
    app->wifi_connected = false;
    for(uint8_t i = 0; i < 20; i++) {  // wait up to 1 s for PONG
        furi_delay_ms(50);
        if(app->fhttp->state == IDLE) { app->wifi_connected = true; break; }
    }
}

// Free FlipperHTTP if allocated.
static void api_release_fhttp(App* app) {
    if(app->fhttp) {
        flipper_http_free(app->fhttp);
        app->fhttp = NULL;
    }
}

// --- Async callbacks (no context param — use g_app_ptr) ---

static bool api_do_request(void) {
    App* app = (App*)g_app_ptr;
    if(!app || !app->fhttp) return false;

    char encoded[96];
    api_url_encode(app->api_query, encoded, sizeof(encoded));

    char url[200];
    snprintf(url, sizeof(url), "%s%s?translation=%s",
        API_BASE_URL, encoded,
        API_TRANSLATIONS[app->api_trans_sel].code);

    const char* headers = "{\"Content-Type\":\"application/json\"}";
    app->fhttp->save_received_data = false;
    return flipper_http_request(app->fhttp, GET, url, headers, NULL);
}

static bool api_do_parse(void) {
    App* app = (App*)g_app_ptr;
    if(!app || !app->fhttp) return false;

    const char* resp = app->fhttp->last_response;
    if(!resp || !resp[0]) return false;

    // API returns {"error":"..."} for bad references
    if(strstr(resp, "\"error\"")) return false;

    bool ref_ok  = json_extract_str(resp, "reference",
                       app->api_result_ref,  sizeof(app->api_result_ref));
    bool text_ok = json_extract_str(resp, "text",
                       app->api_result_text, sizeof(app->api_result_text));
    if(!ref_ok || !text_ok) return false;

    word_wrap(&app->api_wrap, app->api_result_text,
              FONT_CHARS[app->font_choice]);
    app->api_wrap.scroll = 0;
    return true;
}

// Orchestrate: switch to loading screen, block on HTTP, switch to result/error.
static void api_fetch(App* app) {
    api_ensure_fhttp(app);
    if(!app->fhttp) {
        strncpy(app->api_result_ref, "WiFi board not found",
                sizeof(app->api_result_ref) - 1);
        app->api_result_ref[sizeof(app->api_result_ref)-1] = '\0';
        app->view = ViewApiError;
        return;
    }

    app->view = ViewApiLoading;
    view_port_update(app->view_port);
    furi_delay_ms(30);

    g_app_ptr = app;
    bool ok = flipper_http_process_response_async(
        app->fhttp, api_do_request, api_do_parse);

    if(!ok || app->fhttp->state == ISSUE) {
        if(app->fhttp->state == INACTIVE) {
            strncpy(app->api_result_ref, "No WiFi connection",
                    sizeof(app->api_result_ref) - 1);
        } else if(!ok) {
            strncpy(app->api_result_ref, "Verse not found",
                    sizeof(app->api_result_ref) - 1);
        } else {
            strncpy(app->api_result_ref, "Request failed",
                    sizeof(app->api_result_ref) - 1);
        }
        app->api_result_ref[sizeof(app->api_result_ref)-1] = '\0';
        app->view = ViewApiError;
    } else {
        app->view = ViewApiResult;
    }
}

// Build query string from picker state and fetch.
// e.g. api_book_sel=43(John), api_chapter_sel=3, api_verse_sel=16 -> "John 3:16"
static void api_fetch_quick(App* app) {
    snprintf(app->api_query, sizeof(app->api_query), "%s %u:%u",
        BIBLE_BOOKS[app->api_book_sel].name,
        (unsigned)app->api_chapter_sel,
        (unsigned)app->api_verse_sel);
    app->api_query_len = (uint8_t)strlen(app->api_query);
    api_fetch(app);
}

// ============================================================
// Drawing helpers
// ============================================================

static void draw_hdr(Canvas* canvas, const char* title) {
    canvas_set_color(canvas, ColorBlack);
    canvas_draw_box(canvas, 0, 0, SCREEN_W, HDR_H);
    canvas_set_color(canvas, ColorWhite);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, SCREEN_W/2, 1, AlignCenter, AlignTop, title);
    canvas_set_color(canvas, ColorBlack);
}

static void draw_scrollbar(Canvas* canvas, uint16_t pos, uint16_t total, uint8_t vis) {
    if(total <= vis) return;
    uint8_t bh = SCREEN_H - HDR_H - 2;
    uint8_t th = (uint8_t)((bh * vis) / total);
    if(th < 3) th = 3;
    uint8_t ty = HDR_H + 1 + (uint8_t)(((uint32_t)(bh - th) * pos) / (total - vis));
    canvas_set_color(canvas, ColorBlack);
    canvas_draw_line(canvas, SCREEN_W-2, HDR_H+1, SCREEN_W-2, SCREEN_H-1);
    canvas_draw_box(canvas, SCREEN_W-3, ty, 3, th);
}

static void draw_list_item(Canvas* canvas, uint8_t y, const char* text, bool sel) {
    if(sel) {
        canvas_set_color(canvas, ColorBlack);
        canvas_draw_box(canvas, 0, y, SCREEN_W-4, LINE_H);
        canvas_set_color(canvas, ColorWhite);
        canvas_draw_str(canvas, 4, y+8, text);
        canvas_set_color(canvas, ColorBlack);
    } else {
        canvas_draw_str(canvas, 4, y+8, text);
    }
}

// ============================================================
// Scene drawing
// ============================================================

static void draw_loading(Canvas* canvas, App* app) {
    canvas_set_color(canvas, ColorBlack);
    canvas_draw_box(canvas, 0, 0, SCREEN_W, HDR_H);
    canvas_set_color(canvas, ColorWhite);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, SCREEN_W/2, 1, AlignCenter, AlignTop, "Bible Verse Viewer");
    canvas_set_color(canvas, ColorBlack);
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, SCREEN_W/2, 28, AlignCenter, AlignCenter, "Loading verses...");
    if(app->loading_msg[0]) {
        canvas_draw_str_aligned(canvas, SCREEN_W/2, 40, AlignCenter, AlignCenter, app->loading_msg);
    }
    canvas_draw_str_aligned(canvas, SCREEN_W/2, 54, AlignCenter, AlignCenter, "Please wait");
}

static void draw_error(Canvas* canvas, App* app) {
    canvas_set_color(canvas, ColorBlack);
    canvas_draw_box(canvas, 0, 0, SCREEN_W, HDR_H);
    canvas_set_color(canvas, ColorWhite);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, SCREEN_W/2, 1, AlignCenter, AlignTop, "Error");
    canvas_set_color(canvas, ColorBlack);
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, SCREEN_W/2, 22, AlignCenter, AlignCenter, app->error_msg);
    canvas_draw_str_aligned(canvas, SCREEN_W/2, 34, AlignCenter, AlignCenter, "Copy verse files to:");
    canvas_draw_str_aligned(canvas, SCREEN_W/2, 44, AlignCenter, AlignCenter, "apps_data/");
    canvas_draw_str_aligned(canvas, SCREEN_W/2, 54, AlignCenter, AlignCenter, "bible_viewer/");
}

static void draw_main_menu(Canvas* canvas, App* app) {
    static const char* items[] = {
        "Browse Verses", "Search Verses", "Random Verse",
        "Verse of the Day", "Bookmarks", "Bible API (FlipperHTTP)", "Settings", "About",
    };
    canvas_set_color(canvas, ColorBlack);
    canvas_draw_box(canvas, 0, 0, SCREEN_W, HDR_H);
    canvas_set_color(canvas, ColorWhite);
    // Cross icon
    canvas_draw_line(canvas, 8, 2, 8, 9);
    canvas_draw_line(canvas, 5, 5, 11, 5);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 16, 10, "Bible Verse Viewer");
    canvas_set_color(canvas, ColorBlack);
    canvas_set_font(canvas, FontSecondary);
    for(uint8_t i = 0; i < 5 && (app->menu_scroll + i) < MenuItemCount; i++) {
        uint8_t idx = app->menu_scroll + i;
        draw_list_item(canvas, BODY_Y + i*LINE_H, items[idx], idx == app->menu_sel);
    }
    draw_scrollbar(canvas, app->menu_scroll, MenuItemCount, 5);
}

static void draw_browse(Canvas* canvas, App* app) {
    draw_hdr(canvas, "All Verses");
    canvas_set_font(canvas, FontSecondary);
    for(uint8_t i = 0; i < VISIBLE_LINES && (app->browse_scroll+i) < app->verse_count; i++) {
        uint16_t vi = app->browse_scroll + i;
        draw_list_item(canvas, BODY_Y + i*LINE_H,
            app->index[vi].ref, vi == app->browse_sel);
    }
    draw_scrollbar(canvas, app->browse_scroll, app->verse_count, VISIBLE_LINES);
    char cnt[16];
    snprintf(cnt, sizeof(cnt), "%u/%u", app->browse_sel+1, app->verse_count);
    canvas_draw_str_aligned(canvas, SCREEN_W-4, SCREEN_H-1, AlignRight, AlignBottom, cnt);
}

static void draw_verse_read(Canvas* canvas, App* app) {
    draw_hdr(canvas, app->cur_ref);
    apply_verse_font(canvas, app->font_choice);
    uint8_t lh  = FONT_LINE_H[app->font_choice];
    uint8_t vis = font_visible_lines(app->font_choice);
    for(uint8_t i = 0; i < vis && (app->wrap.scroll+i) < app->wrap.count; i++)
        canvas_draw_str(canvas, 2, BODY_Y + i*lh + lh - 1,
            app->wrap.lines[app->wrap.scroll + i]);
    draw_scrollbar(canvas, app->wrap.scroll, app->wrap.count, vis);
    if(app->cur_verse >= 0 && is_bookmarked(app, (uint16_t)app->cur_verse)) {
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(canvas, SCREEN_W-2, SCREEN_H-1, AlignRight, AlignBottom, "*");
    }
}

static void draw_single_verse(Canvas* canvas, App* app, const char* title) {
    if(app->cur_verse < 0) return;
    draw_hdr(canvas, title);
    apply_verse_font(canvas, app->font_choice);
    uint8_t lh  = FONT_LINE_H[app->font_choice];
    uint8_t vis = font_visible_lines(app->font_choice);
    if(vis > 1) vis--;  // leave room for reference line at bottom
    for(uint8_t i = 0; i < vis && (app->wrap.scroll+i) < app->wrap.count; i++)
        canvas_draw_str(canvas, 2, BODY_Y + i*lh + lh - 1,
            app->wrap.lines[app->wrap.scroll + i]);
    canvas_set_font(canvas, FontSecondary);
    char ref[REF_LEN + 4];
    snprintf(ref, sizeof(ref), "- %s", app->cur_ref);
    canvas_draw_str_aligned(canvas, SCREEN_W-4, SCREEN_H-1, AlignRight, AlignBottom, ref);
}

// Update app->kb_suggestion based on the current search_buf.
// Only active during API input (verse lookup). Matches the typed prefix
// case-insensitively against all 66 book names. Clears suggestion if the
// buffer is empty, already contains a space (user is past the book name),
// or no book name starts with the typed text.
static void kb_update_suggestion(App* app) {
    app->kb_suggestion[0] = '\0';
    if(!app->api_input_active) return;
    if(!app->search_len) return;
    // Stop suggesting only when the user has typed a full book name followed
    // by a space (i.e. they accepted and moved on to the chapter number).
    // Do NOT stop on intermediate spaces — book names like "1 Kings",
    // "Song of Solomon", "1 Corinthians" all contain spaces.
    // Check: if last char is a space and everything before it exactly matches
    // a book name, the suggestion phase is over.
    if(app->search_buf[app->search_len - 1] == ' ') return;

    for(uint8_t b = 0; b < BIBLE_BOOKS_COUNT; b++) {
        const char* name = BIBLE_BOOKS[b].name;
        uint8_t     nlen = (uint8_t)strlen(name);
        if(app->search_len > nlen) continue;          // typed more than book name
        bool match = true;
        for(uint8_t i = 0; i < app->search_len; i++) {
            char typed = app->search_buf[i];
            char book  = name[i];
            // case-insensitive
            if(typed >= 'A' && typed <= 'Z') typed += 32;
            if(book  >= 'A' && book  <= 'Z') book  += 32;
            if(typed != book) { match = false; break; }
        }
        if(match) {
            strncpy(app->kb_suggestion, name, sizeof(app->kb_suggestion) - 1);
            app->kb_suggestion[sizeof(app->kb_suggestion) - 1] = '\0';
            return;  // use first match
        }
    }
}

static void draw_search_input(Canvas* canvas, App* app) {
    static const char* ptitles[] = { "Search", "Search: Sym", "Search: Spc" };
    static const char* api_ptitles[] = { "Lookup Verse", "Lookup: Sym", "Lookup: Spc" };
    const char* title = app->api_input_active ?
        api_ptitles[app->kb_page] : ptitles[app->kb_page];
    draw_hdr(canvas, title);

    canvas_set_font_custom(canvas, FONT_SIZE_SMALL);
    canvas_draw_frame(canvas, 2, BODY_Y, SCREEN_W-4, 12);
    char disp[MAX_SEARCH_LEN + 4];
    snprintf(disp, sizeof(disp), "%s_", app->search_buf);
    canvas_draw_str(canvas, 4, BODY_Y+9, disp);

    // Ghost suggestion: show the full book name right-aligned in the field.
    // Drawn before the typed text so the typed text naturally overdraws it
    // where they overlap.
    if(app->kb_suggestion[0]) {
        canvas_draw_str_aligned(canvas, SCREEN_W-6, BODY_Y+9,
            AlignRight, AlignBottom, app->kb_suggestion);
    }

    uint8_t ky = 30, kw = 9, kh = 8;
    for(uint8_t r = 0; r < KB_NROWS; r++) {
        for(uint8_t c = 0; c < KB_NCOLS; c++) {
            uint8_t x = 4 + c*kw, y = ky + r*kh;
            bool sel = (r == app->kb_row && c == app->kb_col);
            if(sel) {
                canvas_set_color(canvas, ColorBlack);
                canvas_draw_box(canvas, x, y, kw-1, kh-1);
                canvas_set_color(canvas, ColorWhite);
            }
            canvas_draw_str(canvas, x+1, y+7, kb_key_label(app, r, c));
            if(sel && app->kb_page == 0) {
                char base = kb_page0[r][c];
                if(base >= 'a' && base <= 'z') {
                    char opp = app->kb_caps ? base : (char)(base - 32);
                    char hint[2] = { opp, '\0' };
                    canvas_draw_str(canvas, x+5, y+4, hint);
                }
            }
            canvas_set_color(canvas, ColorBlack);
        }
    }
    uint8_t by = ky + KB_NROWS*kh + 1;
    // 5 special buttons: DEL | SPC | CAP | SYM/SPC/ABC | GO!
    const char* btns[5] = {
        "DEL",
        "SPC",
        (app->kb_page == 0) ? "CAP" : "---",
        (app->kb_page == 0) ? "SYM" : (app->kb_page == 1) ? "SPC" : "ABC",
        "GO!"
    };
    uint8_t bx[5] = {  2, 27, 52, 77, 102 };
    uint8_t bw[5] = { 23, 23, 23, 23,  23 };
    for(uint8_t i = 0; i < 5; i++) {
        bool sel      = (app->kb_row == KB_NROWS && app->kb_col == i);
        bool caps_lit = (i == 2 && app->kb_page == 0 && app->kb_caps);
        bool fill = sel || caps_lit;
        if(fill) {
            canvas_set_color(canvas, ColorBlack);
            canvas_draw_box(canvas, bx[i], by, bw[i], kh);
            canvas_set_color(canvas, ColorWhite);
        } else {
            canvas_draw_frame(canvas, bx[i], by, bw[i], kh);
        }
        canvas_set_font_custom(canvas, FONT_SIZE_SMALL);
        canvas_draw_str_aligned(canvas, bx[i]+bw[i]/2, by+kh-1,
            AlignCenter, AlignBottom, btns[i]);
        canvas_set_color(canvas, ColorBlack);
    }
}

static void draw_search_results(Canvas* canvas, App* app) {
    char hdr[20];
    snprintf(hdr, sizeof(hdr), "Found: %d", app->hits.count);
    draw_hdr(canvas, hdr);
    if(!app->hits.count) {
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(canvas, SCREEN_W/2, 32, AlignCenter, AlignCenter, "No matches found");
        canvas_draw_str_aligned(canvas, SCREEN_W/2, 44, AlignCenter, AlignCenter, "Try different words");
        return;
    }
    uint8_t vis = VISIBLE_LINES;
    uint8_t scroll = (app->hits.sel >= vis) ? app->hits.sel - vis + 1 : 0;
    canvas_set_font(canvas, FontSecondary);
    for(uint8_t i = 0; i < vis && (scroll+i) < app->hits.count; i++) {
        uint8_t si = scroll + i;
        draw_list_item(canvas, BODY_Y + i*LINE_H,
            app->index[app->hits.idx[si]].ref, si == app->hits.sel);
    }
    draw_scrollbar(canvas, scroll, app->hits.count, vis);
}

static void draw_bookmarks(Canvas* canvas, App* app) {
    draw_hdr(canvas, "Bookmarks");
    if(!app->bmarks.count) {
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(canvas, SCREEN_W/2, 28, AlignCenter, AlignCenter, "No bookmarks yet");
        canvas_draw_str_aligned(canvas, SCREEN_W/2, 42, AlignCenter, AlignCenter, "Long-press OK on");
        canvas_draw_str_aligned(canvas, SCREEN_W/2, 52, AlignCenter, AlignCenter, "a verse to save it");
        return;
    }
    uint8_t vis = VISIBLE_LINES;
    uint8_t scroll = (app->bmarks.sel >= vis) ? app->bmarks.sel - vis + 1 : 0;
    canvas_set_font(canvas, FontSecondary);
    for(uint8_t i = 0; i < vis && (scroll+i) < app->bmarks.count; i++) {
        uint8_t si = scroll + i;
        draw_list_item(canvas, BODY_Y + i*LINE_H,
            app->index[app->bmarks.idx[si]].ref, si == app->bmarks.sel);
    }
    draw_scrollbar(canvas, scroll, app->bmarks.count, vis);
}

static void draw_settings(Canvas* canvas, App* app) {
    draw_hdr(canvas, "Settings");
    canvas_set_font(canvas, FontSecondary);

    // Layout constants
    const uint8_t SEC_LABEL_Y = BODY_Y + 7;   // y for section title text
    const uint8_t ITEM_Y0     = SEC_LABEL_Y + 9; // y for first item row

    if(app->settings_sec == 0) {
        // ── Section: Bible Version ──────────────────────────────
        canvas_set_color(canvas, ColorBlack);
        canvas_draw_box(canvas, 0, BODY_Y, SCREEN_W, 9);
        canvas_set_color(canvas, ColorWhite);
        canvas_draw_str_aligned(canvas, SCREEN_W/2, SEC_LABEL_Y,
            AlignCenter, AlignBottom, "Bible Version  [Right=Font]");
        canvas_set_color(canvas, ColorBlack);

        // Show up to 4 version items; scroll so selected is visible
        uint8_t vis    = 4;
        uint8_t scroll = (app->settings_sel >= vis) ? app->settings_sel - vis + 1 : 0;
        for(uint8_t i = 0; i < vis && (scroll+i) < app->vfile_count; i++) {
            uint8_t si = scroll + i;
            uint8_t y  = ITEM_Y0 + i * LINE_H;
            bool sel    = (app->settings_sel == si);
            bool active = (app->vfile_sel == si);
            if(sel) {
                canvas_set_color(canvas, ColorBlack);
                canvas_draw_box(canvas, 2, y-1, SCREEN_W-4, LINE_H);
                canvas_set_color(canvas, ColorWhite);
            }
            canvas_draw_str(canvas, 5,  y+7, active ? ">" : " ");
            canvas_draw_str(canvas, 13, y+7, app->vfiles[si].label);
            canvas_set_color(canvas, ColorBlack);
        }
        draw_scrollbar(canvas, scroll, app->vfile_count, vis);
    } else {
        // ── Section: Font Size ──────────────────────────────────
        canvas_set_color(canvas, ColorBlack);
        canvas_draw_box(canvas, 0, BODY_Y, SCREEN_W, 9);
        canvas_set_color(canvas, ColorWhite);
        canvas_draw_str_aligned(canvas, SCREEN_W/2, SEC_LABEL_Y,
            AlignCenter, AlignBottom, "Font Size  [Left=Version]");
        canvas_set_color(canvas, ColorBlack);

        uint8_t vis    = 4;
        uint8_t scroll = (app->settings_sel >= vis) ? app->settings_sel - vis + 1 : 0;
        for(uint8_t i = 0; i < vis && (scroll+i) < FONT_COUNT; i++) {
            uint8_t si = scroll + i;
            uint8_t y  = ITEM_Y0 + i * LINE_H;
            bool sel    = (app->settings_sel == si);
            bool active = ((uint8_t)app->font_choice == si);
            if(sel) {
                canvas_set_color(canvas, ColorBlack);
                canvas_draw_box(canvas, 2, y-1, SCREEN_W-4, LINE_H);
                canvas_set_color(canvas, ColorWhite);
            }
            canvas_draw_str(canvas, 5,  y+7, active ? ">" : " ");
            canvas_draw_str(canvas, 13, y+7, FONT_LABELS[si]);
            canvas_set_color(canvas, ColorBlack);
        }
        draw_scrollbar(canvas, scroll, FONT_COUNT, vis);
    }
}

static void draw_about(Canvas* canvas, App* app) {
    draw_hdr(canvas, "About");

    static const char* const about_lines[] = {
        "Bible Viewer v" APP_VERSION,
        "─────────────────────",
        "OFFLINE FEATURES",
        "KJV / ESV / Luther 1912",
        "Browse & search verses",
        "Random verse",
        "Verse of the Day",
        "  random, persists daily",
        "Bookmarks (hold OK)",
        "5 font sizes",
        "─────────────────────",
        "ONLINE (bible-api.com)",
        "No login or key needed",
        "Keyboard lookup",
        "  Hold OK: accept",
        "  book name suggestion",
        "Quick picker:",
        "  Book/Chapter/Verse",
        "  clamped per chapter",
        "9 translations",
        "WiFi icon in header",
        "WiFi Status screen:",
        "  Board/State/SSID/IP",
        "─────────────────────",
        "CONTROLS",
        "Up/Down: scroll",
        "Left/Right: cycle picker",
        "OK: select / bookmark",
        "Hold OK: accept suggestion",
        "  or caps (keyboard)",
        "Back: return/backspace",
    };
    static const uint8_t ABOUT_LINES =
        sizeof(about_lines) / sizeof(about_lines[0]);

    canvas_set_font(canvas, FontSecondary);
    const uint8_t lh  = 10;  // FontSecondary line height
    const uint8_t vis = (SCREEN_H - HDR_H - 2) / lh;  // 5 lines visible

    for(uint8_t i = 0; i < vis; i++) {
        uint8_t li = app->about_scroll + i;
        if(li >= ABOUT_LINES) break;
        canvas_draw_str(canvas, 2, HDR_H + 2 + i*lh + lh - 1, about_lines[li]);
    }
    draw_scrollbar(canvas, app->about_scroll, ABOUT_LINES, vis);
}

// WiFi icon bitmap — 12x12 px, XBM format (LSB-first, 0=draw, 1=background).
// 2 bytes per row (16 bits), only 12 bits used; upper nibble of byte 2 is padding.
static const uint8_t wifi_icon_bits[24] = {
    0x00, 0xF0, 0xF8, 0xF1, 0x0E, 0xF7, 0x01, 0xF8, 0xF0, 0xF0, 0x9C, 0xF3,
    0x02, 0xF4, 0x60, 0xF0, 0x98, 0xF1, 0x00, 0xF0, 0x60, 0xF0, 0x60, 0xF0,
};

static void draw_api_menu(Canvas* canvas, App* app) {
    draw_hdr(canvas, "Bible API");

    // WiFi icon — top-right of header bar (12x12 px, fits exactly in HDR_H=12).
    {
        bool wifi_ok = app->wifi_connected;
        if(wifi_ok) {
            // Draw WiFi XBM icon in white (arcs = 0-bits, background = 1-bits skipped)
            canvas_set_color(canvas, ColorWhite);
            canvas_draw_xbm(canvas, 114, 0, 12, 12, wifi_icon_bits);
        } else {
            // Draw a white 11x10 X centred in the header icon area (x=114..124, y=1..10)
            canvas_set_color(canvas, ColorWhite);
            canvas_draw_line(canvas, 114, 1, 124, 10);
            canvas_draw_line(canvas, 115, 1, 124, 9);
            canvas_draw_line(canvas, 114, 10, 124, 1);
            canvas_draw_line(canvas, 115, 10, 124, 2);
        }
        canvas_set_color(canvas, ColorBlack);
    }

    canvas_set_font(canvas, FontSecondary);

    // 6 menu items, 5 visible at once
    uint8_t vis = 5;
    for(uint8_t i = 0; i < vis; i++) {
        uint8_t idx = app->api_menu_scroll + i;
        if(idx >= API_MENU_ITEMS) break;
        uint8_t y   = BODY_Y + i * LINE_H;
        bool    sel = (idx == app->api_menu_sel);

        char label[28];
        switch(idx) {
        case 0:
            strncpy(label, "Lookup Verse", sizeof(label)-1);
            label[sizeof(label)-1] = '\0';
            break;
        case 1:
            // Bk: <name>  — abbreviated prefix to keep it short
            snprintf(label, sizeof(label), "Book: %s",
                BIBLE_BOOKS[app->api_book_sel].name);
            break;
        case 2:
            snprintf(label, sizeof(label), "Chapter: %u",
                (unsigned)app->api_chapter_sel);
            break;
        case 3:
            snprintf(label, sizeof(label), "Verse: %u",
                (unsigned)app->api_verse_sel);
            break;
        case 4:
            snprintf(label, sizeof(label), "Trans: %s",
                API_TRANSLATIONS[app->api_trans_sel].label);
            break;
        case 5:
            strncpy(label, "WiFi Status", sizeof(label)-1);
            label[sizeof(label)-1] = '\0';
            break;
        case 6:
        default:
            strncpy(label, "Back", sizeof(label)-1);
            label[sizeof(label)-1] = '\0';
            break;
        }

        // For picker rows (1-3), show left/right arrows on the selected row
        if(sel && idx >= 1 && idx <= 3) {
            // Draw highlighted row background
            canvas_set_color(canvas, ColorBlack);
            canvas_draw_box(canvas, 0, y, SCREEN_W-4, LINE_H);
            canvas_set_color(canvas, ColorWhite);
            // Left arrow at left edge
            canvas_draw_str(canvas, 2, y+8, "<");
            // Value centered
            canvas_draw_str_aligned(canvas, SCREEN_W/2, y+8,
                AlignCenter, AlignBottom, label);
            // Right arrow at right edge
            canvas_draw_str(canvas, SCREEN_W-8, y+8, ">");
            canvas_set_color(canvas, ColorBlack);
        } else {
            draw_list_item(canvas, y, label, sel);
        }
    }
    draw_scrollbar(canvas, app->api_menu_scroll, API_MENU_ITEMS, vis);
}

static void draw_api_loading(Canvas* canvas, App* app) {
    draw_hdr(canvas, "Bible API");
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, SCREEN_W/2, 26,
        AlignCenter, AlignCenter, "Fetching verse...");
    if(app->api_query[0]) {
        char disp[68];
        snprintf(disp, sizeof(disp), "\"%s\"", app->api_query);
        canvas_draw_str_aligned(canvas, SCREEN_W/2, 38,
            AlignCenter, AlignCenter, disp);
    }
    canvas_draw_str_aligned(canvas, SCREEN_W/2, 54,
        AlignCenter, AlignBottom, "Please wait");
}

static void draw_api_result(Canvas* canvas, App* app) {
    draw_hdr(canvas, app->api_result_ref[0] ? app->api_result_ref : "Result");
    apply_verse_font(canvas, app->font_choice);
    uint8_t lh  = FONT_LINE_H[app->font_choice];
    uint8_t vis = font_visible_lines(app->font_choice);
    for(uint8_t i = 0; i < vis && (app->api_wrap.scroll+i) < app->api_wrap.count; i++)
        canvas_draw_str(canvas, 2,
            BODY_Y + i*lh + lh - 1,
            app->api_wrap.lines[app->api_wrap.scroll + i]);
    draw_scrollbar(canvas, app->api_wrap.scroll, app->api_wrap.count, vis);
    // Translation label bottom-right
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, SCREEN_W-2, SCREEN_H-1,
        AlignRight, AlignBottom,
        API_TRANSLATIONS[app->api_trans_sel].code);
}

static void draw_api_error(Canvas* canvas, App* app) {
    draw_hdr(canvas, "API Error");
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, SCREEN_W/2, 24,
        AlignCenter, AlignCenter, app->api_result_ref);
    canvas_draw_str_aligned(canvas, SCREEN_W/2, 38,
        AlignCenter, AlignCenter, "Check WiFi board");
    canvas_draw_str_aligned(canvas, SCREEN_W/2, 50,
        AlignCenter, AlignCenter, "& connection");
    canvas_draw_str_aligned(canvas, SCREEN_W/2, SCREEN_H-1,
        AlignCenter, AlignBottom, "Back to return");
}

// Send a simple WiFi query command, wait briefly, copy last_response.
// Returns false if fhttp is not ready.
static bool api_query_string(App* app, const char* cmd, char* out, size_t out_sz) {
    if(!app->wifi_connected) return false;
    if(!flipper_http_send_data(app->fhttp, cmd)) return false;
    // Wait up to 1 s for the response to arrive
    for(uint8_t i = 0; i < 20; i++) {
        furi_delay_ms(50);
        if(app->fhttp->last_response && app->fhttp->last_response[0]) break;
    }
    if(!app->fhttp->last_response || !app->fhttp->last_response[0]) return false;
    strncpy(out, app->fhttp->last_response, out_sz - 1);
    out[out_sz - 1] = '\0';
    // Clear last_response so the next query starts fresh
    app->fhttp->last_response[0] = '\0';
    return true;
}

// Populate api_status_ssid / api_status_ip and switch to ViewApiStatus.
static void api_open_status(App* app) {
    api_ensure_fhttp(app);
    app->api_status_ssid[0] = '\0';
    app->api_status_ip[0]   = '\0';

    if(!app->wifi_connected) {
        strncpy(app->api_status_ssid, "N/A", sizeof(app->api_status_ssid)-1);
        strncpy(app->api_status_ip,   "N/A", sizeof(app->api_status_ip)-1);
    } else {
        // Clear stale response before querying
        app->fhttp->last_response[0] = '\0';
        if(!api_query_string(app, "[WIFI/SSID]",
                             app->api_status_ssid, sizeof(app->api_status_ssid)))
            strncpy(app->api_status_ssid, "Unknown", sizeof(app->api_status_ssid)-1);
        if(!api_query_string(app, "[IP/ADDRESS]",
                             app->api_status_ip,   sizeof(app->api_status_ip)))
            strncpy(app->api_status_ip, "Unknown", sizeof(app->api_status_ip)-1);
    }
    app->view = ViewApiStatus;
}

static void draw_api_status(Canvas* canvas, App* app) {
    draw_hdr(canvas, "WiFi Status");
    canvas_set_font(canvas, FontSecondary);

    bool board_found = app->wifi_connected;
    bool connected   = app->wifi_connected;

    uint8_t y = BODY_Y;
    char line[40];

    // Row 1: Board
    snprintf(line, sizeof(line), "Board: %s",
        board_found ? "Found" : "Not found");
    canvas_draw_str(canvas, 2, y + 8, line);
    y += LINE_H;

    // Row 2: State
    const char* state_str;
    if(!app->wifi_connected) {
        state_str = "Disconnected";
    } else {
        switch(app->fhttp->state) {
        case IDLE:      state_str = "Connected";    break;
        case RECEIVING: state_str = "Active";       break;
        case ISSUE:     state_str = "Error";        break;
        default:        state_str = "Disconnected"; break;
        }
    }
    snprintf(line, sizeof(line), "State: %s", state_str);
    canvas_draw_str(canvas, 2, y + 8, line);
    y += LINE_H;

    // Row 3: SSID
    snprintf(line, sizeof(line), "SSID: %s",
        connected ? app->api_status_ssid : "---");
    canvas_draw_str(canvas, 2, y + 8, line);
    y += LINE_H;

    // Row 4: IP
    snprintf(line, sizeof(line), "IP: %s",
        connected ? app->api_status_ip : "---");
    canvas_draw_str(canvas, 2, y + 8, line);

    // Back hint at bottom
    canvas_draw_str_aligned(canvas, SCREEN_W/2, SCREEN_H - 1,
        AlignCenter, AlignBottom, "OK/Back: return");
}

static void draw_api_trans(Canvas* canvas, App* app) {
    draw_hdr(canvas, "Translation");
    canvas_set_font(canvas, FontSecondary);
    uint8_t vis    = 4;
    uint8_t scroll = (app->api_trans_scroll);
    for(uint8_t i = 0; i < vis && (scroll+i) < API_TRANS_COUNT; i++) {
        uint8_t si = scroll + i;
        uint8_t y  = BODY_Y + i * LINE_H;
        bool active = (app->api_trans_sel == si);
        char label[24];
        snprintf(label, sizeof(label), "%s%s",
            active ? ">" : " ",
            API_TRANSLATIONS[si].label);
        draw_list_item(canvas, y, label, active);
    }
    draw_scrollbar(canvas, scroll, API_TRANS_COUNT, vis);
}

// ============================================================
// Main draw callback
// ============================================================

static void draw_cb(Canvas* canvas, void* ctx) {
    App* app = (App*)ctx;
    canvas_clear(canvas);
    switch(app->view) {
    case ViewMainMenu:      draw_main_menu(canvas, app);          break;
    case ViewBrowseList:    draw_browse(canvas, app);             break;
    case ViewVerseRead:     draw_verse_read(canvas, app);         break;
    case ViewSearchInput:   draw_search_input(canvas, app);       break;
    case ViewSearchResults: draw_search_results(canvas, app);     break;
    case ViewRandomVerse:   draw_single_verse(canvas, app, "Random Verse");       break;
    case ViewDailyVerse:    draw_single_verse(canvas, app, "Verse of the Day");   break;
    case ViewBookmarks:     draw_bookmarks(canvas, app);          break;
    case ViewSettings:      draw_settings(canvas, app);           break;
    case ViewAbout:         draw_about(canvas, app);              break;
    case ViewLoading:       draw_loading(canvas, app);            break;
    case ViewError:         draw_error(canvas, app);              break;
    case ViewApiMenu:       draw_api_menu(canvas, app);           break;
    case ViewApiLoading:    draw_api_loading(canvas, app);        break;
    case ViewApiResult:     draw_api_result(canvas, app);         break;
    case ViewApiError:      draw_api_error(canvas, app);          break;
    case ViewApiTrans:      draw_api_trans(canvas, app);          break;
    case ViewApiStatus:     draw_api_status(canvas, app);         break;
    }
}

// ============================================================
// Input handling
// ============================================================

static void input_cb(InputEvent* ev, void* ctx) {
    furi_message_queue_put(((App*)ctx)->queue, ev, FuriWaitForever);
}

static void on_main_menu(App* app, InputEvent* ev) {
    if(ev->type != InputTypeShort && ev->type != InputTypeRepeat) return;
    switch(ev->key) {
    case InputKeyUp:
        if(app->menu_sel > 0) {
            app->menu_sel--;
            if(app->menu_sel < app->menu_scroll) app->menu_scroll = app->menu_sel;
        }
        break;
    case InputKeyDown:
        if(app->menu_sel < MenuItemCount - 1) {
            app->menu_sel++;
            if(app->menu_sel >= app->menu_scroll + 4) app->menu_scroll = app->menu_sel - 3;
        }
        break;
    case InputKeyOk:
        switch(app->menu_sel) {
        case MenuBrowse:
            app->browse_sel = 0; app->browse_scroll = 0;
            app->view = ViewBrowseList; break;
        case MenuSearch:
            memset(app->search_buf, 0, sizeof(app->search_buf));
            app->search_len = 0; app->kb_row = 0; app->kb_col = 0;
            app->kb_caps = false; app->kb_page = 0;
            app->kb_suggestion[0] = '\0';
            app->view = ViewSearchInput; break;
        case MenuRandom:
            app->rng ^= furi_get_tick();
            open_verse(app, (uint16_t)(rng_next(&app->rng) % app->verse_count), ViewRandomVerse);
            app->view = ViewRandomVerse; break;
        case MenuDaily: {
            // Day number derived the same way as before, used only for comparison
            uint32_t today = furi_get_tick() / (1000u * 60 * 60 * 24);
            if(today != app->daily_verse_day || app->daily_verse_idx >= app->verse_count) {
                // New day (or first run) — pick a fresh random verse and persist it
                app->rng ^= furi_get_tick();
                app->daily_verse_idx = (uint16_t)(rng_next(&app->rng) % app->verse_count);
                app->daily_verse_day = today;
                settings_save(app);
            }
            open_verse(app, app->daily_verse_idx, ViewDailyVerse);
            app->view = ViewDailyVerse; break;
        }
        case MenuBookmarks:
            app->bmarks.sel = 0; app->view = ViewBookmarks; break;
        case MenuSettings:
            app->settings_sec = 0;
            app->settings_sel = app->vfile_sel;
            app->view = ViewSettings;
            break;
        case MenuAbout:
            app->view = ViewAbout; break;
        case MenuApi:
            app->api_menu_sel    = 0;
            app->api_menu_scroll = 0;
            app->api_trans_scroll = (app->api_trans_sel >= 4) ?
                                    app->api_trans_sel - 3 : 0;
            // Initialise picker to 1-based values on first use
            if(app->api_chapter_sel == 0) app->api_chapter_sel = 1;
            if(app->api_verse_sel   == 0) app->api_verse_sel   = 1;
            api_ensure_fhttp(app);
            // Prefetch WiFi status so icon and WiFi Status screen are up to date
            app->api_status_ssid[0] = '\0';
            app->api_status_ip[0]   = '\0';
            if(app->wifi_connected) {
                app->fhttp->last_response[0] = '\0';
                api_query_string(app, "[WIFI/SSID]",
                    app->api_status_ssid, sizeof(app->api_status_ssid));
                api_query_string(app, "[IP/ADDRESS]",
                    app->api_status_ip,   sizeof(app->api_status_ip));
            }
            app->view = ViewApiMenu; break;
        default: break;
        }
        break;
    case InputKeyBack:
        app->running = false; break;
    default: break;
    }
}

static void on_browse(App* app, InputEvent* ev) {
    if(ev->type != InputTypeShort && ev->type != InputTypeRepeat) return;
    uint16_t total = app->verse_count;
    switch(ev->key) {
    case InputKeyUp:
        if(app->browse_sel > 0) {
            app->browse_sel--;
            if(app->browse_sel < app->browse_scroll) app->browse_scroll = app->browse_sel;
        } break;
    case InputKeyDown:
        if(app->browse_sel < total-1) {
            app->browse_sel++;
            if(app->browse_sel >= app->browse_scroll + VISIBLE_LINES)
                app->browse_scroll = app->browse_sel - VISIBLE_LINES + 1;
        } break;
    case InputKeyLeft:
        app->browse_sel = (app->browse_sel > VISIBLE_LINES) ? app->browse_sel - VISIBLE_LINES : 0;
        if(app->browse_sel < app->browse_scroll) app->browse_scroll = app->browse_sel;
        break;
    case InputKeyRight:
        app->browse_sel = (app->browse_sel + VISIBLE_LINES < total) ? app->browse_sel + VISIBLE_LINES : total-1;
        if(app->browse_sel >= app->browse_scroll + VISIBLE_LINES)
            app->browse_scroll = app->browse_sel - VISIBLE_LINES + 1;
        break;
    case InputKeyOk:  open_verse(app, app->browse_sel, ViewBrowseList); break;
    case InputKeyBack: app->view = ViewMainMenu; break;
    default: break;
    }
}

static void on_verse_read(App* app, InputEvent* ev) {
    if(ev->type == InputTypeShort || ev->type == InputTypeRepeat) {
        switch(ev->key) {
        case InputKeyUp:
            if(app->wrap.scroll > 0) app->wrap.scroll--;
            break;
        case InputKeyDown:
            if(app->wrap.scroll + font_visible_lines(app->font_choice) < app->wrap.count)
                app->wrap.scroll++;
            break;
        case InputKeyLeft:
            if(app->cur_verse > 0) open_verse(app, (uint16_t)(app->cur_verse-1), app->return_view);
            break;
        case InputKeyRight:
            if((uint16_t)(app->cur_verse+1) < app->verse_count)
                open_verse(app, (uint16_t)(app->cur_verse+1), app->return_view);
            break;
        case InputKeyBack: app->view = app->return_view; break;
        default: break;
        }
    }
    if(ev->type == InputTypeLong && ev->key == InputKeyOk && app->cur_verse >= 0)
        toggle_bmark(app, (uint16_t)app->cur_verse);
}

static void search_buf_backspace(App* app) {
    if(!app->search_len) return;
    while(app->search_len > 0 && (app->search_buf[app->search_len-1] & 0xC0) == 0x80)
        app->search_buf[--app->search_len] = '\0';
    if(app->search_len > 0) app->search_buf[--app->search_len] = '\0';
}

static void on_search_input(App* app, InputEvent* ev) {
    // Maps each of the 13 keyboard columns to the nearest special button (0-4)
    static const uint8_t col_to_btn[KB_NCOLS] = { 0,0,0, 1,1, 2,2,2, 3,3,3, 4,4 };
    // Maps each special button back to a representative keyboard column
    static const uint8_t btn_to_col[5]         = { 1, 3, 6, 9, 11 };

    if(ev->type == InputTypeLong && ev->key == InputKeyOk) {
        if(app->kb_row < KB_NROWS) {
            if(app->kb_suggestion[0]) {
                // Accept the suggestion: replace what's typed with the full
                // book name followed by a space, ready for the chapter number.
                uint8_t slen = (uint8_t)strlen(app->kb_suggestion);
                if(slen + 1 < MAX_SEARCH_LEN) {
                    memcpy(app->search_buf, app->kb_suggestion, slen);
                    app->search_buf[slen]   = ' ';
                    app->search_buf[slen+1] = '\0';
                    app->search_len = slen + 1;
                    kb_update_suggestion(app); // clears suggestion (space present)
                }
            } else if(app->kb_page == 0) {
                // Original behaviour: type the caps version of the current key
                char ch = kb_page0[app->kb_row][app->kb_col];
                if(ch >= 'a' && ch <= 'z') {
                    if(!app->kb_caps) ch -= 32;
                    if(app->search_len < MAX_SEARCH_LEN-1) {
                        app->search_buf[app->search_len++] = ch;
                        app->search_buf[app->search_len]   = '\0';
                        kb_update_suggestion(app);
                    }
                }
            }
        }
        app->kb_long_consumed = true; return;
    }
    if(ev->type == InputTypeRelease && ev->key == InputKeyOk) { app->kb_long_consumed = false; return; }
    if(ev->type == InputTypeRepeat  && ev->key == InputKeyOk && app->kb_long_consumed) return;
    if(ev->type != InputTypeShort   && ev->type != InputTypeRepeat) return;

    switch(ev->key) {
    case InputKeyUp:
        if(app->kb_row == KB_NROWS)      { app->kb_row = KB_NROWS-1; app->kb_col = btn_to_col[app->kb_col]; }
        else if(app->kb_row > 0)           app->kb_row--;
        else                             { app->kb_row = KB_NROWS;   app->kb_col = col_to_btn[app->kb_col]; }
        break;
    case InputKeyDown:
        if(app->kb_row < KB_NROWS-1)      app->kb_row++;
        else if(app->kb_row == KB_NROWS-1){ app->kb_row = KB_NROWS;   app->kb_col = col_to_btn[app->kb_col]; }
        else                             { app->kb_row = 0;           app->kb_col = btn_to_col[app->kb_col]; }
        break;
    case InputKeyLeft:
        if(app->kb_row == KB_NROWS) app->kb_col = (app->kb_col == 0) ? 4         : app->kb_col-1;
        else                        app->kb_col = (app->kb_col == 0) ? KB_NCOLS-1: app->kb_col-1;
        break;
    case InputKeyRight:
        if(app->kb_row == KB_NROWS) app->kb_col = (app->kb_col == 4)         ? 0 : app->kb_col+1;
        else                        app->kb_col = (app->kb_col == KB_NCOLS-1) ? 0 : app->kb_col+1;
        break;
    case InputKeyOk:
        if(app->kb_row == KB_NROWS) {
            if(app->kb_col == 0) {                                        // DEL
                search_buf_backspace(app);
                kb_update_suggestion(app);
            } else if(app->kb_col == 1) {                                 // SPC
                if(app->search_len < MAX_SEARCH_LEN-1) {
                    app->search_buf[app->search_len++] = ' ';
                    app->search_buf[app->search_len]   = '\0';
                    kb_update_suggestion(app);
                }
            } else if(app->kb_col == 2) {                                 // CAP
                if(app->kb_page == 0) app->kb_caps = !app->kb_caps;
            } else if(app->kb_col == 3) {                                 // SYM/SPC/ABC
                app->kb_page = (app->kb_page+1) % KB_NPAGES;
            } else {                                                       // GO!
                if(app->api_input_active) {
                    strncpy(app->api_query, app->search_buf,
                            sizeof(app->api_query) - 1);
                    app->api_query[sizeof(app->api_query)-1] = '\0';
                    app->api_query_len = app->search_len;
                    app->api_input_active = false;
                    api_fetch(app);
                } else {
                    do_search(app); app->view = ViewSearchResults;
                }
            }
        } else {
            const char* seq = kb_key_bytes(app, app->kb_row, app->kb_col);
            size_t slen = strlen(seq);
            if(seq[0] && seq[0] != ' ' && app->search_len + slen < MAX_SEARCH_LEN-1) {
                memcpy(app->search_buf + app->search_len, seq, slen);
                app->search_len += (uint8_t)slen;
                app->search_buf[app->search_len] = '\0';
                kb_update_suggestion(app);
            }
        }
        break;
    case InputKeyBack:
        if(app->search_len) {
            search_buf_backspace(app);
            kb_update_suggestion(app);
        } else {
            bool was_api = app->api_input_active;
            app->api_input_active = false;
            app->view = was_api ? ViewApiMenu : ViewMainMenu;
        }
        break;
    default: break;
    }
}

static void on_search_results(App* app, InputEvent* ev) {
    if(ev->type != InputTypeShort && ev->type != InputTypeRepeat) return;
    switch(ev->key) {
    case InputKeyUp:
        if(app->hits.sel > 0) app->hits.sel--;
        break;
    case InputKeyDown:
        if(app->hits.sel < app->hits.count-1) app->hits.sel++;
        break;
    case InputKeyOk:
        if(app->hits.count) open_verse(app, app->hits.idx[app->hits.sel], ViewSearchResults);
        break;
    case InputKeyBack: app->view = ViewSearchInput; break;
    default: break;
    }
}

static void on_random_daily(App* app, InputEvent* ev, bool is_random) {
    if(ev->type == InputTypeShort || ev->type == InputTypeRepeat) {
        switch(ev->key) {
        case InputKeyUp:
            if(app->wrap.scroll > 0) app->wrap.scroll--;
            break;
        case InputKeyDown:
            if(app->wrap.scroll + font_visible_lines(app->font_choice) - 1 < app->wrap.count)
                app->wrap.scroll++;
            break;
        case InputKeyOk:
            if(is_random) {
                open_verse(app, (uint16_t)(rng_next(&app->rng) % app->verse_count), ViewRandomVerse);
                app->view = ViewRandomVerse;
            }
            break;
        case InputKeyBack: app->view = ViewMainMenu; break;
        default: break;
        }
    }
    if(ev->type == InputTypeLong && ev->key == InputKeyOk && app->cur_verse >= 0)
        toggle_bmark(app, (uint16_t)app->cur_verse);
}

static void on_bookmarks(App* app, InputEvent* ev) {
    if(ev->type != InputTypeShort && ev->type != InputTypeRepeat) return;
    switch(ev->key) {
    case InputKeyUp:
        if(app->bmarks.sel > 0) app->bmarks.sel--;
        break;
    case InputKeyDown:
        if(app->bmarks.count && app->bmarks.sel < app->bmarks.count-1) app->bmarks.sel++;
        break;
    case InputKeyOk:
        if(app->bmarks.count) open_verse(app, app->bmarks.idx[app->bmarks.sel], ViewBookmarks);
        break;
    case InputKeyBack: app->view = ViewMainMenu; break;
    default: break;
    }
}

static void on_settings(App* app, InputEvent* ev) {
    if(ev->type != InputTypeShort && ev->type != InputTypeRepeat) return;
    switch(ev->key) {

    case InputKeyLeft:
        // Switch to Bible Version section
        if(app->settings_sec != 0) {
            app->settings_sec = 0;
            app->settings_sel = app->vfile_sel; // restore cursor to active item
        }
        break;

    case InputKeyRight:
        // Switch to Font Size section
        if(app->settings_sec != 1) {
            app->settings_sec = 1;
            app->settings_sel = (uint8_t)app->font_choice; // restore cursor to active item
        }
        break;

    case InputKeyUp:
        if(app->settings_sel > 0) app->settings_sel--;
        break;

    case InputKeyDown:
        if(app->settings_sec == 0) {
            if(app->settings_sel < app->vfile_count - 1) app->settings_sel++;
        } else {
            if(app->settings_sel < FONT_COUNT - 1) app->settings_sel++;
        }
        break;

    case InputKeyOk:
        if(app->settings_sec == 0) {
            // ── Apply Bible Version change ──────────────────────
            if(app->settings_sel != app->vfile_sel) {
                snprintf(app->loading_msg, sizeof(app->loading_msg),
                    "%s", app->vfiles[app->settings_sel].label);
                app->view = ViewLoading;
                view_port_update(app->view_port);
                furi_delay_ms(50);

                if(!switch_verse_file(app, app->settings_sel)) {
                    strncpy(app->error_msg, "Failed to load file",
                        sizeof(app->error_msg));
                    app->view = ViewError;
                } else {
                    app->cur_verse     = -1;
                    app->browse_sel    = 0;
                    app->browse_scroll = 0;
                    app->bmarks.count  = 0;
                    settings_save(app);
                    app->loading_msg[0] = '\0';
                    app->view = ViewSettings;
                }
            }
        } else {
            // ── Apply Font Size change ──────────────────────────
            FontChoice chosen = (FontChoice)app->settings_sel;
            if(chosen != app->font_choice) {
                app->font_choice = chosen;
                // Re-wrap current verse with new column width
                if(app->cur_verse >= 0) {
                    char text[LINE_BUF_LEN];
                    if(read_verse_text(app, (uint16_t)app->cur_verse,
                            text, sizeof(text)))
                        word_wrap(&app->wrap, text,
                            FONT_CHARS[app->font_choice]);
                }
                settings_save(app);
            }
        }
        break;

    case InputKeyBack:
        app->view = ViewMainMenu;
        break;

    default: break;
    }
}


static void on_api_menu(App* app, InputEvent* ev) {
    if(ev->type != InputTypeShort && ev->type != InputTypeRepeat) return;

    switch(ev->key) {
    case InputKeyUp:
        if(app->api_menu_sel > 0) {
            app->api_menu_sel--;
            if(app->api_menu_sel < app->api_menu_scroll)
                app->api_menu_scroll = app->api_menu_sel;
        }
        break;
    case InputKeyDown:
        if(app->api_menu_sel < API_MENU_ITEMS - 1) {
            app->api_menu_sel++;
            if(app->api_menu_sel >= app->api_menu_scroll + 5)
                app->api_menu_scroll = app->api_menu_sel - 4;
        }
        break;

    case InputKeyLeft:
        switch(app->api_menu_sel) {
        case 1: // Book: prev
            if(app->api_book_sel > 0) app->api_book_sel--;
            else                      app->api_book_sel = BIBLE_BOOKS_COUNT - 1;
            // Clamp chapter then verse to new book's real limits
            if(app->api_chapter_sel > BIBLE_BOOKS[app->api_book_sel].chapters)
                app->api_chapter_sel = BIBLE_BOOKS[app->api_book_sel].chapters;
            if(app->api_verse_sel > book_chapter_verses(app->api_book_sel, app->api_chapter_sel))
                app->api_verse_sel = book_chapter_verses(app->api_book_sel, app->api_chapter_sel);
            break;
        case 2: // Chapter: prev
            if(app->api_chapter_sel > 1) app->api_chapter_sel--;
            else app->api_chapter_sel = BIBLE_BOOKS[app->api_book_sel].chapters;
            // Clamp verse to new chapter's real limit
            if(app->api_verse_sel > book_chapter_verses(app->api_book_sel, app->api_chapter_sel))
                app->api_verse_sel = book_chapter_verses(app->api_book_sel, app->api_chapter_sel);
            break;
        case 3: // Verse: prev — cycle within current chapter
            if(app->api_verse_sel > 1) app->api_verse_sel--;
            else app->api_verse_sel = book_chapter_verses(app->api_book_sel, app->api_chapter_sel);
            break;
        default: break;
        }
        break;

    case InputKeyRight:
        switch(app->api_menu_sel) {
        case 1: // Book: next
            if(app->api_book_sel < BIBLE_BOOKS_COUNT - 1) app->api_book_sel++;
            else                                           app->api_book_sel = 0;
            // Clamp chapter then verse to new book's real limits
            if(app->api_chapter_sel > BIBLE_BOOKS[app->api_book_sel].chapters)
                app->api_chapter_sel = BIBLE_BOOKS[app->api_book_sel].chapters;
            if(app->api_verse_sel > book_chapter_verses(app->api_book_sel, app->api_chapter_sel))
                app->api_verse_sel = book_chapter_verses(app->api_book_sel, app->api_chapter_sel);
            break;
        case 2: // Chapter: next
            if(app->api_chapter_sel < BIBLE_BOOKS[app->api_book_sel].chapters)
                app->api_chapter_sel++;
            else
                app->api_chapter_sel = 1;
            // Clamp verse to new chapter's real limit
            if(app->api_verse_sel > book_chapter_verses(app->api_book_sel, app->api_chapter_sel))
                app->api_verse_sel = book_chapter_verses(app->api_book_sel, app->api_chapter_sel);
            break;
        case 3: // Verse: next — cycle within current chapter
            if(app->api_verse_sel < book_chapter_verses(app->api_book_sel, app->api_chapter_sel))
                app->api_verse_sel++;
            else
                app->api_verse_sel = 1;
            break;
        default: break;
        }
        break;

    case InputKeyOk:
        switch(app->api_menu_sel) {
        case 0: // Lookup Verse — open keyboard
            memset(app->search_buf, 0, sizeof(app->search_buf));
            app->search_len = 0;
            app->kb_row = 0; app->kb_col = 0;
            app->kb_caps = false; app->kb_page = 0;
            app->kb_long_consumed = false;
            app->kb_suggestion[0] = '\0';
            app->api_input_active = true;
            app->view = ViewSearchInput;
            break;
        case 1: // Book — OK fetches quick lookup
        case 2: // Chapter — OK fetches quick lookup
        case 3: // Verse — OK fetches quick lookup
            settings_save(app);
            api_fetch_quick(app);
            break;
        case 4: // Translation picker
            app->api_trans_scroll = (app->api_trans_sel >= 4) ?
                                    app->api_trans_sel - 3 : 0;
            app->view = ViewApiTrans;
            break;
        case 5: // WiFi Status
            api_open_status(app);
            break;
        case 6: // Back
            api_release_fhttp(app);
            app->view = ViewMainMenu;
            break;
        default: break;
        }
        break;

    case InputKeyBack:
        settings_save(app);
        api_release_fhttp(app);
        app->view = ViewMainMenu;
        break;
    default: break;
    }
}

static void on_api_result(App* app, InputEvent* ev) {
    if(ev->type != InputTypeShort && ev->type != InputTypeRepeat) return;
    uint8_t vis = font_visible_lines(app->font_choice);
    switch(ev->key) {
    case InputKeyUp:
        if(app->api_wrap.scroll > 0) app->api_wrap.scroll--;
        break;
    case InputKeyDown:
        if(app->api_wrap.scroll + vis < app->api_wrap.count)
            app->api_wrap.scroll++;
        break;
    case InputKeyBack:
        app->view = ViewApiMenu;
        break;
    default: break;
    }
}

static void on_api_trans(App* app, InputEvent* ev) {
    if(ev->type != InputTypeShort && ev->type != InputTypeRepeat) return;
    switch(ev->key) {
    case InputKeyUp:
        if(app->api_trans_sel > 0) {
            app->api_trans_sel--;
            if(app->api_trans_sel < app->api_trans_scroll)
                app->api_trans_scroll = app->api_trans_sel;
        }
        break;
    case InputKeyDown:
        if(app->api_trans_sel < API_TRANS_COUNT - 1) {
            app->api_trans_sel++;
            if(app->api_trans_sel >= app->api_trans_scroll + 4)
                app->api_trans_scroll = app->api_trans_sel - 3;
        }
        break;
    case InputKeyOk:
    case InputKeyBack:
        settings_save(app);
        app->view = ViewApiMenu;
        break;
    default: break;
    }
}

static void on_api_status(App* app, InputEvent* ev) {
    if(ev->type != InputTypeShort) return;
    if(ev->key == InputKeyBack || ev->key == InputKeyOk)
        app->view = ViewApiMenu;
}

// ============================================================
// Entry point
// ============================================================

int32_t bible_viewer_app(void* p) {
    UNUSED(p);

    App* app = malloc(sizeof(App));
    if(!app) return -1;
    memset(app, 0, sizeof(App));

    // Heap-allocate the verse index: 600 * 28 bytes = ~16 KB
    app->index = malloc(MAX_VERSES * sizeof(VerseIndex));
    if(!app->index) { free(app); return -1; }

    app->running   = true;
    app->view      = ViewLoading;
    app->cur_verse = -1;
    app->rng       = furi_get_tick();
    g_app_ptr      = app;

    // Open storage
    app->storage = furi_record_open(RECORD_STORAGE);
    storage_simply_mkdir(app->storage, DATA_DIR);

    // Set up GUI — show "Loading" screen immediately
    app->queue     = furi_message_queue_alloc(8, sizeof(InputEvent));
    app->view_port = view_port_alloc();
    view_port_draw_callback_set(app->view_port, draw_cb, app);
    view_port_input_callback_set(app->view_port, input_cb, app);
    app->gui = furi_record_open(RECORD_GUI);
    gui_add_view_port(app->gui, app->view_port, GuiLayerFullscreen);
    view_port_update(app->view_port);

    // Discover verse files on SD and build index
    discover_verse_files(app);

    if(app->vfile_count == 0) {
        strncpy(app->error_msg, "No verse files found!", sizeof(app->error_msg));
        app->view = ViewError;
    } else {
        // Load saved settings (verse_file + font_size) before opening
        settings_load(app);

        // Show which file is loading
        snprintf(app->loading_msg, sizeof(app->loading_msg),
            "%s", app->vfiles[app->vfile_sel].label);
        view_port_update(app->view_port);

        if(open_verse_file(app) && build_index(app)) {
            bmarks_load(app);
            app->loading_msg[0] = '\0';
            app->view = ViewMainMenu;
        } else {
            strncpy(app->error_msg, "Failed to read file", sizeof(app->error_msg));
            app->view = ViewError;
        }
    }
    view_port_update(app->view_port);

    // Main event loop
    InputEvent ev;
    while(app->running) {
        if(furi_message_queue_get(app->queue, &ev, 100) != FuriStatusOk) continue;

        switch(app->view) {
        case ViewMainMenu:      on_main_menu(app, &ev);               break;
        case ViewBrowseList:    on_browse(app, &ev);                  break;
        case ViewVerseRead:     on_verse_read(app, &ev);              break;
        case ViewSearchInput:   on_search_input(app, &ev);            break;
        case ViewSearchResults: on_search_results(app, &ev);          break;
        case ViewRandomVerse:   on_random_daily(app, &ev, true);      break;
        case ViewDailyVerse:    on_random_daily(app, &ev, false);     break;
        case ViewBookmarks:     on_bookmarks(app, &ev);               break;
        case ViewSettings:      on_settings(app, &ev);                break;
        case ViewAbout:
            if(ev.type == InputTypeShort || ev.type == InputTypeRepeat) {
                // 25 lines, 5 visible at lh=10 (FontSecondary) in 50px body
                static const uint8_t ABOUT_TOTAL = 31;
                static const uint8_t ABOUT_VIS   = 5;
                if(ev.key == InputKeyUp && app->about_scroll > 0)
                    app->about_scroll--;
                else if(ev.key == InputKeyDown &&
                        app->about_scroll + ABOUT_VIS < ABOUT_TOTAL)
                    app->about_scroll++;
                else if(ev.key == InputKeyBack) {
                    app->about_scroll = 0;
                    app->view = ViewMainMenu;
                }
            }
            break;
        case ViewError:
        case ViewLoading:
            if(ev.type == InputTypeShort && ev.key == InputKeyBack) app->running = false;
            break;
        // ── Bible API views ──────────────────────────────────────
        case ViewApiMenu:       on_api_menu(app, &ev);                break;
        case ViewApiResult:     on_api_result(app, &ev);              break;
        case ViewApiTrans:      on_api_trans(app, &ev);               break;
        case ViewApiStatus:     on_api_status(app, &ev);              break;
        case ViewApiLoading:    break; // blocked — no input handled
        case ViewApiError:
            if(ev.type == InputTypeShort && ev.key == InputKeyBack)
                app->view = ViewApiMenu;
            break;
        }
        view_port_update(app->view_port);
    }

    // Cleanup
    if(app->vfile) {
        storage_file_close(app->vfile);
        storage_file_free(app->vfile);
    }
    api_release_fhttp(app);
    g_app_ptr = NULL;
    gui_remove_view_port(app->gui, app->view_port);
    furi_record_close(RECORD_GUI);
    view_port_free(app->view_port);
    furi_message_queue_free(app->queue);
    furi_record_close(RECORD_STORAGE);
    free(app->index);
    free(app);
    return 0;
}
