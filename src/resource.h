// {{NO_DEPENDENCIES}}
// For #define-ing static resources for resource script file(s).
// Used by resource.rc

// clang-format off

/* Icons */
#define IDI_MAIN                    101 /* 32x32 & 48x48 icon */
#define IDI_SMALL                   102 /* Small 16x16 icon */
#define IDI_ABOUT                   103 /* About Dialog icon */

/* Main application resource, also used to attach menu */
#define IDC_MAIN                    106

/* Dialogs */
#define IDD_ABOUTDLG                108

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
#define IDM_MONOCHROME              208 /* Only uses gray tones */
// Background color choices
#define IDM_WHITE_BKG               209
#define IDM_BLACK_BKG               210
#define IDM_GREY_BKG                211
#define IDM_RED_BKG                 212
#define IDM_GREEN_BKG               213
#define IDM_BLUE_BKG                214

// Drawing speed
#define IDM_SLOW                    215
#define IDM_MEDIUM                  216
#define IDM_FAST                    217
#define IDM_HYPER                   218

// Dev menu items
#define IDM_TESTTRAP                219

// Buttons
#define IDC_BUTTON                  300

// Timer IDs
#define TIMER_ART                   400

// For resources to be loaded without an ID from the system.
#ifndef IDC_STATIC
 #define IDC_STATIC                 -1
#endif // IDC_STATIC
// clang-format on
