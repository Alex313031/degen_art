// {{NO_DEPENDENCIES}}
// For #define-ing static resources for resource script file(s).
// Used by resource.rc

// clang-format off

/* Icons */
#define IDI_MAIN                    101 /* 32x32 & 48x48 icon */
#define IDI_SMALL                   102 /* Small 16x16 icon */
#define IDI_ABOUT                   103 /* About Dialog icon */

/* Bitmaps */
#define IDB_PEN_BMP                 104 /* "Pen" icon for toolbar strip, extracted from Win2K mspaint.exe */
#define IDB_NODRAW_BMP              105 /* "Pen X" icon for toolbar strip, overwrites Pen icon when needed, modified with red X */
#define IDB_SHAPES_BMP              106 /* "Shapes" icon for toolbar strip, custom icon */
#define IDB_PAUSE_BMP               107 /* "Pause" icon for toolbar strip, custom icon */
#define IDB_PLAY_BMP                108 /* "Play" icon for toolbar strip, overwrites Pause icon when needed, custom icon */
#define IDB_EXIT_BMP                109 /* "Exit" icon for toolbar strip, custom icon */
#define IDB_SOUND_BMP               110 /* "Speaker" icon for toolbar strip, same as Win2000 speaker icon*/
#define IDB_MUTE_BMP                111 /* "Mute" icon for toolbar strip, overwrites Speaker icon when needed */
#define IDB_SAVE_BMP                112 /* "Save" icon for toolbar strip, floppy disk icon */

/* Main application resource, also used to attach menu */
#define IDC_MAIN                    133

/* Dialogs */
#define IDD_ABOUTDLG                150

/* Menu items */
#define IDM_ABOUT                   200
#define IDM_EXIT                    201
#define IDM_HELP                    202
#define IDM_RESERVED                203

// Save snapshot
#define IDM_SAVE_AS                 204

// Shape menu choices
#define IDM_RECTANGLES              205
#define IDM_ELLIPSES                206
#define IDM_BOTH                    207

// Color choices
#define IDM_PAUSED                  208 /* Pause painting */
#define IDM_MONOCHROME              209 /* Only uses gray tones */
// Background color choices
#define IDM_WHITE_BKG               210
#define IDM_BLACK_BKG               211
#define IDM_GREY_BKG                212
#define IDM_RED_BKG                 213
#define IDM_GREEN_BKG               214
#define IDM_BLUE_BKG                215

// Drawing speed
#define IDM_SLOW                    216
#define IDM_MEDIUM                  217
#define IDM_FAST                    218
#define IDM_HYPER                   219

// Sound settings
#define IDM_SOUND                   220

// Threads (concurrently drawn shapes) control. MUST remain consecutive —
// both the RC check-state probe in InitMenuDefaults and the WM_COMMAND
// handler derive the thread count arithmetically as (id - IDM_CONC_1 + 1).
// Upper bound tracks kMaxArtThreads (art.h). If you bump kMaxArtThreads,
// add more IDs here AND re-shift every IDM_* below this block by the same
// amount so nothing aliases.
#define IDM_CONC_1                  221
#define IDM_CONC_2                  222
#define IDM_CONC_3                  223
#define IDM_CONC_4                  224
#define IDM_CONC_5                  225
#define IDM_CONC_6                  226
#define IDM_CONC_7                  227
#define IDM_CONC_8                  228

// Whether bezier curves are enabled
#define IDM_BEZIERS                 229

// Whether lines are enabled
#define IDM_LINES                   230

// Forces painting a new canvas, with whatever settings it currently has
#define IDM_REPAINT                 231

// "Single step" through painting, allows you to run it manually one iteration at a time
// instead of using timer
#define IDM_SINGLE                  232

// Pauses shape painting, and allows one to "draw" a line with left click drag, like mspaint.exe
#define IDM_DRAW                    233

// Global color picker, for various functions
#define IDM_PICKCOLOR               234

// Draw-color quick-pick items shown in the IDM_DRAW toolbar dropdown
#define IDM_DRAW_WHITE              235
#define IDM_DRAW_BLACK              236
#define IDM_DRAW_RED                237
#define IDM_DRAW_GREEN              238
#define IDM_DRAW_BLUE               239

// Toolbar button menu itentifier for "Shapes" button with submenu.
#define IDM_SHAPES                  240

// Dev menu items
#define IDM_TESTTRAP                250

// Embedded background-music WAV. Loaded as a user-defined "WAVE" resource
// when kUseEmbeddedBgm is true (see utils.h). The RC file binds this ID
// to res/watersky.wav; FindResourceW(L"WAVE") picks it up at runtime.
#define IDR_BGM_WAVE                500

// Buttons
#define IDC_BUTTON                  300

// Timer IDs
#define TIMER_ART                   400

// Custom posted-message IDs (WM_APP range, guaranteed to not clash with any
// system / common-control message). Used to defer work that mustn't run
// inside WM_CREATE — see WM_APP_AUTOPLAY usage in main.cc.
#define WM_APP_AUTOPLAY             (WM_APP + 0)

// For resources to be loaded without an ID from the system.
#ifndef IDC_STATIC
 #define IDC_STATIC                 -1
#endif // IDC_STATIC
// clang-format on
