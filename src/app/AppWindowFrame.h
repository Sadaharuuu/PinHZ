#ifndef APPWINDOWFRAME_H
#define APPWINDOWFRAME_H

class QWidget;

// Keep the native Windows frame and synchronize its colors with the Qt palette.
class AppWindowFrame {
public:
    static void refresh();
    static bool apply(QWidget *window);
};

#endif
