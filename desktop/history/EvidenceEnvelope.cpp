#include "EvidenceEnvelope.hpp"
#include "ProjectDocument.hpp"
#include "telemetry/ObservedMapping.hpp"
#include "telemetry/TelemetryContracts.hpp"
#include "quality/PipeTransportEvidence.hpp"
#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QMap>
#include <QSet>
#include <QUuid>
#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace breakwater::desktop::history {
namespace {
void require(bool v,const char* message) { if (!v) throw std::invalid_argument(message); }
void keys(const QJsonObject& o,const QSet<QString>& expected) {
    require(o.size()==expected.size(),"Unsupported evidence envelope fields.");
    for (auto i=o.begin();i!=o.end();++i) require(expected.contains(i.key()),"Unknown evidence envelope field.");
}
double number(const QJsonValue& v) { require(v.isDouble() && std::isfinite(v.toDouble()),"Nonfinite or missing evidence value."); return v.toDouble(); }
double nonnegative(const QJsonValue& v) { const auto n=number(v); require(n>=0,"Negative uncertainty or clock."); return n; }
bool near(double a,double b) { return std::abs(a-b)<=1e-10*std::max({1.0,std::abs(a),std::abs(b)}); }
struct Bounded { double value,bound; };
Bounded bounded(const QJsonValue& v) {
    require(v.isObject(),"Missing bounded witness value."); const auto o=v.toObject(); keys(o,{"value","absolute_bound"});
    return {number(o.value("value")),nonnegative(o.value("absolute_bound"))};
}
void observed(const QJsonObject& payload,const QJsonObject& snapshot,bool bindEffective) {
    const auto context=payload.value("context").toObject(), document=payload.value("snapshot").toObject();
    (void)telemetry::parse_observed_snapshot(document,context);
    const auto mapping=payload.value("mapping").toObject();
    const auto result=telemetry::map_volume_capture(document,context,mapping.value("source_available").toBool());
    require(result.document==document && result.effective_network==payload.value("effective_network").toObject(),"Observed mapping differs from the retained measurements.");
    require(result.diagnostics.value("mapper_version")==mapping.value("mapper_version"),"Unknown observation mapper version.");
    if (bindEffective) {
        require(document.value("availability")=="available" && !result.effective_network.isEmpty(),"Observed investigation lacks usable frozen state.");
        require(snapshot.value("network")==result.effective_network,"Observed investigation network differs from its effective capture.");
    } else require(snapshot.value("network")==context.value("network"),"Detection archive design differs from its registry.");
}
void detection(const QJsonObject& payload,const QJsonObject& snapshot,bool acknowledgement) {
    keys(payload,{"evidence_kind","evidence_version","events","snapshot","context","mapping","effective_network","detector","balance_witness","actionable"});
    require(payload.value("evidence_version")==1 && payload.value("actionable")==false,"Unsupported detection envelope version or authority.");
    observed(payload,snapshot,false);
    const auto context=payload.value("context").toObject(), captured=payload.value("snapshot").toObject();
    const auto events=payload.value("events").toArray(), witness=payload.value("balance_witness").toArray();
    require(payload.value("events").isArray() && !events.isEmpty() && events.size()<=256 &&
        payload.value("balance_witness").isArray() && witness.size()<=8192,"Detection evidence exceeds its bounded dimensions.");
    QMap<QString,QList<QJsonObject>> points;
    for (const auto& value:witness) {
        const auto point=value.toObject();
        keys(point,{"time_s","snapshot_id","tank_id","inventory_m3","net_delivery_m3h","timing_volume_bound_m3","delivery_channels_m3h"});
        const auto time=nonnegative(point.value("time_s"));
        require(time<=number(captured.value("captured_event_time_s")) && !point.value("snapshot_id").toString().isEmpty(),"Invalid witness capture reference.");
        (void)bounded(point.value("inventory_m3")); (void)bounded(point.value("net_delivery_m3h"));
        (void)nonnegative(point.value("timing_volume_bound_m3"));
        const auto channels=point.value("delivery_channels_m3h").toObject();
        require(point.value("delivery_channels_m3h").isObject() && channels.size()<=256,"Invalid witness delivery channels.");
        for (auto i=channels.begin();i!=channels.end();++i) (void)bounded(i.value());
        auto& tank=points[point.value("tank_id").toString()];
        require(!point.value("tank_id").toString().isEmpty() && (tank.isEmpty() || time>number(tank.last().value("time_s"))),"Witness times must increase for each tank.");
        tank.append(point);
    }
    for (const auto& value:events) {
        const auto event=value.toObject(); const auto e=telemetry::parse_detection_event(event,context);
        require(event.value("physical_hash")==captured.value("physical_hash") && event.value("configuration_hash")==captured.value("configuration_hash") &&
            event.value("stream_id")==captured.value("stream_id") && event.value("epoch_id")==captured.value("epoch_id"),"Event source identity differs from its captured evidence.");
        if (acknowledgement) { require(e.acknowledgement.has_value(),"Acknowledgement record is missing its acknowledgement."); continue; }
        require(event.value("snapshot_id")==captured.value("snapshot_id"),"Detection event refers to a different capture.");
        if (e.detector_id!="volume-balance") continue;
        require(e.asset_ids.size()==1 && e.evidence.size()==1 && payload.value("detector").toObject().value("temporal_model")=="declared_piecewise_constant_delivery", "Balance evidence lacks its declared hold model.");
        const auto samples=points.value(QString::fromStdString(e.asset_ids[0]));
        require(samples.size()>=2,"Balance event lacks a pre-assimilation window.");
        double integrated=0,uncertainty=0,variation=0,largest=0;
        for (qsizetype i=1;i<samples.size();++i) {
            const auto before=samples[i-1],after=samples[i];
            const auto dt=number(after.value("time_s"))-number(before.value("time_s"));
            require(dt<=5,"Balance witness contains an unsupported sample gap.");
            const auto left=bounded(before.value("net_delivery_m3h")),right=bounded(after.value("net_delivery_m3h"));
            integrated+=left.value*dt/3600; uncertainty+=std::max(left.bound,right.bound)*dt/3600;
            largest=std::max(largest,std::abs(bounded(before.value("inventory_m3")).value));
            const auto a=before.value("delivery_channels_m3h").toObject(),b=after.value("delivery_channels_m3h").toObject();
            require(a.keys()==b.keys(),"Balance witness changes delivery topology.");
            for (auto channel=a.begin();channel!=a.end();++channel) variation+=std::abs(bounded(channel.value()).value-bounded(b.value(channel.key())).value)*dt/3600;
        }
        const auto first=samples.first(),last=samples.last();
        require(number(last.value("time_s"))-number(first.value("time_s"))>=30 && last.value("snapshot_id")==captured.value("snapshot_id"),"Balance witness does not cover the required current window.");
        const auto a=bounded(first.value("inventory_m3")),b=bounded(last.value("inventory_m3"));
        const auto arithmetic=64*std::numeric_limits<double>::epsilon()*samples.size()*std::max({1.0,largest,std::abs(b.value),std::abs(integrated)});
        const auto threshold=a.bound+b.bound+uncertainty+variation+number(first.value("timing_volume_bound_m3"))+number(last.value("timing_volume_bound_m3"))+arithmetic;
        require(near(e.evidence[0].value,b.value-a.value-integrated) && near(e.evidence[0].threshold,threshold),"Retained balance evidence does not reconstruct from its witness.");
    }
}
}
QJsonObject envelope(const QString& kind,const QJsonObject& payload) { return {{"envelope_version",1},{"kind",kind},{"payload",payload}}; }
QJsonObject evidence_archive(const QString& kind,const QJsonObject& payload,const QJsonObject& snapshot,const QString& objective,const QString& id,const QString& created) {
    const auto identity=id.isEmpty() ? QUuid::createUuid().toString(QUuid::WithoutBraces):id;
    const auto date=created.isEmpty() ? QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs):created;
    return {{"archive_version",2},{"session_kind",kind},{"session_id",identity},{"checkpoint_sequence",1},{"created_at",date},{"updated_at",date},
        {"status","completed"},{"objective",objective},{"snapshot",snapshot},{"model",QJsonObject{}},{"runs",QJsonArray{}},{"events",QJsonArray{}},
        {"recommendation",QJsonObject{}},{"approvals",QJsonArray{}},{"actionable",false},{"evidence_envelopes",QJsonArray{envelope(kind,payload)}}};
}
bool validate_evidence_envelope(const QJsonObject& archive,QString* error) {
    try {
        const auto values=archive.value("evidence_envelopes").toArray();
        require(archive.value("evidence_envelopes").isArray() && values.size()==1,"A v2 record requires one typed evidence envelope.");
        const auto item=values.first().toObject(); keys(item,{"envelope_version","kind","payload"});
        require(item.value("envelope_version")==1 && item.value("payload").isObject(),"Unsupported evidence envelope version.");
        const auto kind=item.value("kind").toString();
        const auto payload=item.value("payload").toObject(),snapshot=archive.value("snapshot").toObject();
        require(QJsonDocument(item).toJson(QJsonDocument::Compact).size()<=8*1024*1024,"Evidence envelope exceeds 8 MiB.");
        require(archive.value("actionable")==false && archive.value("recommendation").toObject().value("actionable")!=true,"V2 observed/retained evidence cannot authorize controls.");
        require(archive.value("session_kind")==kind || (kind=="manual_science_origin" && archive.value("session_kind")=="scientific"),"Envelope kind and session kind disagree.");
        for (const auto& value:archive.value("approvals").toArray())
            require(value.toObject().value("status")=="operator_acknowledged_advisory","Retained evidence cannot contain an operating application action.");
        if (kind=="observed_origin") {
            keys(payload,{"snapshot","context","mapping","effective_network","events"}); observed(payload,snapshot,true);
            require(payload.value("events").isArray() && payload.value("events").toArray().size()<=256,"Invalid observed event count.");
            for (const auto& event:payload.value("events").toArray()) (void)telemetry::parse_detection_event(event.toObject(),payload.value("context").toObject());
        } else if (kind=="observation_detection" || kind=="observation_acknowledgement") detection(payload,snapshot,kind=="observation_acknowledgement");
        else if (kind=="pipe_transport") {
            keys(payload,{"result","source"}); const auto result=payload.value("result").toObject(),source=payload.value("source").toObject();
            QString problem; require(quality::validate_pipe_transport_evidence(result,&problem),"Invalid retained native transport result.");
            require(result.value("run_id")==source.value("run_id") && result.value("snapshot_id")==snapshot.value("snapshot_id") &&
                result.value("physical_hash")==snapshot.value("physical_hash") && source.value("network")==snapshot.value("network") &&
                source.value("snapshot_id")==snapshot.value("snapshot_id") && source.value("physical_hash")==snapshot.value("physical_hash") &&
                source.value("horizon_h")==snapshot.value("horizon_h") && result.value("horizon_h")==snapshot.value("horizon_h") &&
                result.value("result_id")==archive.value("session_id"),"Pipe result and source identities disagree.");
            require(source.value("controls").isArray() && source.value("controls").toArray().size()<=384,"Pipe source controls exceed native bounds.");
            const auto project=parse_project({{"version",1},{"network",source.value("network")},{"layout",QJsonObject{}}});
            std::vector<ControlValue> controls;
            for (const auto& v:source.value("controls").toArray()) { const auto c=v.toObject(); controls.push_back({c.value("control_id").toString().toStdString(),number(c.value("value"))}); }
            const auto compiled=compile(project.network,controls); bool found=false;
            for (const auto& route:compiled.network.routes) if (QString::fromStdString(route.id)==result.value("route_id")) {
                double flow=route.available ? route.setpoint_m3h:0;
                for (const auto& a:compiled.network.assets) if (a.kind==AssetKind::source && a.id==route.source_id) flow=a.inflow_m3h-a.curtailment_m3h;
                require(near(flow,number(result.value("flow_m3h"))),"Pipe result flow differs from the immutable source run."); found=true;
            }
            require(found,"Pipe route is missing from the retained source network.");
        } else if (kind=="manual_science_origin") {
            keys(payload,{"source_session_id","source_snapshot_id","source_run_id","source_physical_hash","effective_physical_hash","horizon_h","sample_step_h","numerical_revision","source_mode"});
            for (const auto* field:{"source_session_id","source_snapshot_id","source_run_id","source_physical_hash","source_mode"})
                require(payload.value(QLatin1String(field)).isString() && !payload.value(QLatin1String(field)).toString().isEmpty() && payload.value(QLatin1String(field)).toString().size()<=256,"Invalid immutable science source identity.");
            require(payload.value("horizon_h")==snapshot.value("horizon_h") && number(payload.value("sample_step_h"))>0 && nonnegative(payload.value("numerical_revision"))<=9007199254740991.0 &&
                payload.value("source_run_id")==snapshot.value("snapshot_id") && payload.value("effective_physical_hash")==snapshot.value("physical_hash"),"Invalid science source grid or effective identity.");
        } else require(false,"Unsupported evidence envelope kind.");
        if (error) error->clear(); return true;
    } catch (const std::exception& problem) { if (error) *error=QString::fromUtf8(problem.what()); return false; }
}
}
