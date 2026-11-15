// How raye-run sets up its application object, shared with the tests.
#pragma once

class QGuiApplication;

namespace RunApp
{
// The box hides itself while Browse is open or a launch asks something,
// and Qt would quit when the last window goes, or when the file dialog
// lets go of its event loop lock. Only Cancel, Esc or a launch end it.
void setUp(QGuiApplication &app);
}
