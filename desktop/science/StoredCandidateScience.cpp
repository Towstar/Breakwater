#include "StoredCandidateScience.hpp"
#include "history/SessionRepository.hpp"
#include <QJsonArray>
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace breakwater::desktop::science {
namespace {
void require(bool value,const char* message) { if (!value) throw std::invalid_argument(message); }
bool agrees(double a,double b) { return std::isfinite(a) && std::isfinite(b) && std::abs(a-b)<=1e-9+1e-10*std::max(std::abs(a),std::abs(b)); }
}
StoredScienceSource capture_stored_science(const QJsonObject& archive,const QString& runId) {
    QString error; require(history::SessionRepository::validateArchive(archive,&error),"The selected session is not complete validated history evidence.");
    const auto snapshot=archive.value("snapshot").toObject(); QJsonObject details;
    for (const auto& row:archive.value("runs").toArray()) if (row.toObject().value("run").toObject().value("run_id")==runId) details=row.toObject();
    require(!details.isEmpty(),"Select an actual stored numerical candidate."); const auto run=details.value("run").toObject();
    require(run.value("solver_version")==QString::fromLatin1(solver_version),"This stored numerical method is unsupported for a fresh scientific comparison.");
    auto project=parse_project({{"version",1},{"network",snapshot.value("network")},{"layout",QJsonObject{}}});
    std::vector<ControlValue> controls;
    for (const auto& value:run.value("controls").toArray()) { const auto row=value.toObject(); controls.push_back({row.value("control_id").toString().toStdString(),row.value("value").toDouble()}); }
    const auto compiled=compile(project.network,controls);
    const SimulationConfig config{run.value("horizon_h").toDouble(),run.value("sample_step_h").toDouble()};
    const auto solverName=run.value("solver").toString();
    require(solverName=="sparse" || solverName=="affine" || solverName=="dense_reference","Unsupported stored solver.");
    auto result=simulate(compiled,config,solverName=="affine" ? Solver::affine:solverName=="dense_reference" ? Solver::dense_reference:Solver::sparse);
    const auto times=details.value("time_h").toArray(),ids=details.value("tank_ids").toArray(),volumes=details.value("volumes_m3").toArray();
    require(times.size()==static_cast<qsizetype>(result.time_h.size()) && ids.size()==static_cast<qsizetype>(result.tank_ids.size()) && agrees(run.value("cost").toDouble(),result.cost),"Stored candidate grid or cost differs from native reproduction.");
    for (qsizetype i=0;i<times.size();++i) {
        require(times[i].toDouble()==result.time_h[static_cast<std::size_t>(i)],"Stored candidate sample times differ from native reproduction.");
        for (qsizetype tank=0;tank<ids.size();++tank) {
            require(ids[tank].toString().toStdString()==result.tank_ids[static_cast<std::size_t>(tank)],"Stored tank identity differs from native reproduction.");
            const auto stored=volumes[tank].toArray()[i].toDouble();
            require(agrees(stored,result.volumes_m3[static_cast<std::size_t>(tank)][static_cast<std::size_t>(i)]),"Stored candidate trajectory disagrees with native reproduction.");
            result.volumes_m3[static_cast<std::size_t>(tank)][static_cast<std::size_t>(i)]=stored;
        }
    }
    result.cost=run.value("cost").toDouble();
    auto capture=std::make_shared<CapturedForecast>(); capture->network=compiled.network; capture->config=config;
    capture->result=std::move(result); capture->validation=validate(compiled,capture->result,config); capture->id=runId; capture->revision=0;
    project.network=capture->network; capture->physical_hash=physics_hash(project);
    return {capture,{{"source_session_id",archive.value("session_id")},{"source_snapshot_id",snapshot.value("snapshot_id")},
        {"source_run_id",runId},{"source_physical_hash",snapshot.value("physical_hash")},{"effective_physical_hash",capture->physical_hash},
        {"horizon_h",config.horizon_h},{"sample_step_h",config.step_h},{"numerical_revision",0},
        {"source_mode",archive.value("historical").toBool() ? "historical_candidate":"stored_candidate"}}};
}
}
