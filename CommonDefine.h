#ifndef COMMONDEFINE_H
#define COMMONDEFINE_H

#define FONT_COLOR_BLACK        "<font color=\"#000000\">"
#define FONT_COLOR_WHITE        "<font color=\"#FFFFFF\">"
#define FONT_COLOR_RED          "<font color=\"#FF0000\">"
#define FONT_COLOR_GREEN        "<font color=\"#00FF00\">"
#define FONT_COLOR_BLUE         "<font color=\"#0000FF\">"
#define FONT_COLOR_CYAN         "<font color=\"#00FFFF\">"
#define FONT_COLOR_PINK         "<font color=\"#FF00FF\">"
#define FONT_COLOR_YELLOW       "<font color=\"#FFFF00\">"

#define FONT_COLOR_DARK_ORANGE  "<font color=\"#FF8C00\">"
#define FONT_COLOR_BLUEGREEN    "<font color=\"#00B3B3\">"
#define FONT_COLOR_BLUEVIOLET   "<font color=\"#8A2BE2\">"

enum e_logLevel
{
    LogLevel_DBG = 0,
    LogLevel_INF,
    LogLevel_WAR,
    LogLevel_ERR,
    LogLevel_COlORFUL,
};

#endif // COMMONDEFINE_H
