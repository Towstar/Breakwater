#pragma once
#include <QString>
class QQuickWindow;
class QCoreApplication;
namespace breakwater::desktop {
class ProjectController;
void startSceneBenchmark(QQuickWindow*, ProjectController*, QCoreApplication*, const QString& output);
}
