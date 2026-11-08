#pragma once

class QCoreApplication;
class QQuickWindow;

namespace breakwater::desktop {
class ProjectController;
// Offline verification of the loaded, compiled Main component using own-window
// Qt events. The application exits with compact JSON and status 0 or 8.
void startWorkbenchUiCheck(QQuickWindow*, ProjectController*, QCoreApplication*);
}
