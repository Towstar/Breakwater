#include "ProjectController.hpp"
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QJsonDocument>
#include <QSaveFile>
#include <QTextStream>
#include <cmath>

using namespace breakwater::desktop;
int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);
    if (app.arguments().size() != 2 || !QDir::isAbsolutePath(app.arguments()[1])) return 2;
    const QDir output(app.arguments()[1]);
    if (!QDir().mkpath(output.absolutePath())) return 2;
    for (const auto* name : {"healthy", "pump-failure", "bad-reroute", "mixed-response", "native-results"})
        if (QFileInfo::exists(output.filePath(QString(name) + ".json"))) return 2;
    QJsonObject results;
    for (const auto* name : {"healthy", "pump-failure", "bad-reroute", "mixed-response"}) {
        ProjectController controller;
        if (QString(name) == "pump-failure") controller.loadExample("baseline");
        if (QString(name) == "bad-reroute") controller.loadExample("bad");
        if (QString(name) == "mixed-response") controller.loadExample("mixed");
        if (!controller.save(QUrl::fromLocalFile(output.filePath(QString(name) + ".json")))) return 3;
        const auto document = parse_project(controller.document());
        const auto compiled = breakwater::compile(document.network);
        const auto run = breakwater::simulate(compiled, {});
        const auto validation = breakwater::validate(compiled, run, {});
        QJsonObject volumes;
        for (std::size_t i = 0; i < run.tank_ids.size(); ++i) volumes[QString::fromStdString(run.tank_ids[i])] = run.volumes_m3[i].back();
        // Two independent closed-form conservation checks for the stock fixture.
        if (QString(name) == "healthy" && (std::abs(volumes["tank_a"].toDouble() - 104) > 1e-9 || std::abs(volumes["tank_b"].toDouble() - 114) > 1e-9)) return 4;
        if (QString(name) == "pump-failure" && (std::abs(volumes["tank_a"].toDouble() - 236) > 1e-9 || std::abs(volumes["tank_b"].toDouble() - 114) > 1e-9)) return 4;
        results[name] = QJsonObject{{"horizon_h", 3}, {"final_m3", volumes}, {"cost_synthetic", run.cost}, {"feasible", validation.feasible}};
    }
    QSaveFile file(output.filePath("native-results.json")); const auto bytes = QJsonDocument(results).toJson(QJsonDocument::Indented);
    if (!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size() || !file.commit()) return 3;
    QTextStream(stdout) << "Exported four native projects; healthy/failure closed-form checks passed.\n";
    return 0;
}
