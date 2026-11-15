// Setting up the application the way every run needs it (style, font,
// colour scheme, Qt Quick style), and the capture of one page.

#pragma once

#include "options.h"
#include "page.h"

#include <functional>

class GalleryWindow;
class SchemeManager;

struct GallerySetup {
    SchemeManager *schemes = nullptr;
    PageContext context;
    QString style;
    QString error;
};

// Call before QApplication: the scale and the Qt Quick style are read
// when the application starts.
void prepareEnvironment(const GalleryOptions &options);

// Call once QApplication exists. error is set when a requested style or
// the colour scheme cannot be used.
GallerySetup setupApplication(const GalleryOptions &options);

// Exit codes of a capture run.
enum ShotResult {
    ShotSaved = 0,
    ShotNotSaved = 2,
    ShotPageProblems = 3,
};

// Puts the page in its capture state (focus, popups, pointer), waits for
// tooltips and animations, grabs the screen into file, then calls done.
void scheduleShot(GalleryWindow *window, const QString &file, const std::function<void(int)> &done);
