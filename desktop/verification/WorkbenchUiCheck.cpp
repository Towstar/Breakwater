#include "WorkbenchUiCheck.hpp"
#include "../ProjectController.hpp"
#include "../history/HistoryController.hpp"
#include "../agent/SimulationGateway.hpp"
#include <QCoreApplication>
#include <QDir>
#include <QDateTime>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPointer>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQmlExpression>
#include <QQuickItem>
#include <QQuickWindow>
#include <QTextStream>
#include <QTemporaryDir>
#include <QTimer>
#include <QUuid>
#include <QWheelEvent>
#include <cmath>
#include <algorithm>
#include <filesystem>
#include <memory>
#include <stdexcept>

namespace breakwater::desktop {
namespace {
class Check final : public QObject {
    QPointer<QQuickWindow> window_; QPointer<ProjectController> controller_;
    QCoreApplication* app_; QTimer timer_; QElapsedTimer elapsed_;
    int phase_ = 0, changes_ = 0; bool done_ = false; QJsonArray checks_; QJsonObject observed_;
    QElapsedTimer outside_shortcut_setup_;
    bool outside_focus_settled_ = false, outside_escape_sent_ = false;
    QString hash_, run_, feedback_; quint64 revision_ = 0;
    QString pipe_evidence_id_;
    QRectF retained_button_rect_;
    QElapsedTimer retained_viewer_wait_;
    QElapsedTimer stored_source_wait_;
    quint64 navigation_editor_revision_ = 0;
    QString navigation_selection_;
    bool navigation_all_labels_ = false;
    bool navigation_dock_open_ = false;
    QElapsedTimer route_label_wait_;
    QJsonObject cue_original_project_, cue_static_, workspace_observations_;
    int rendered_frames_ = 0, cue_frame_mark_ = 0, cue_frame_deadline_ms_ = 2000;
    QElapsedTimer cue_settle_wait_;
    bool cue_fixture_captured_ = false;
    QJsonObject scene_focus_preferences_, plan_before_dock_;
    double plan_before_wheel_ = 0, plan_pan_x_ = 0, plan_pan_z_ = 0;
    bool observed_only_=false;
    bool compare_settings_only_=false;
    std::function<int()> model_requests_;
    QJsonObject comparison_project_, comparison_archive_;
    QPointer<QQuickItem> comparison_key_input_, comparison_save_key_;
    std::unique_ptr<QTemporaryDir> comparison_storage_;
    qint64 comparison_checkpoint_=0;
    QJsonObject observed_project_;
    QJsonObject forecast_resize_project_;
    int forecast_resize_width_=0, forecast_resize_height_=0, forecast_resize_dock_=0;
    QJsonObject position_; QPointF tank_; double capacity_ = 0;
    std::unique_ptr<QTemporaryDir> artifacts_;
    QVariant eval(const QString& source, QObject* scope=nullptr) {
        if (!scope) scope=window_;
        QQmlExpression expression(QQmlEngine::contextForObject(scope), scope, source);
        const auto value = expression.evaluate();
        if (expression.hasError()) throw std::runtime_error("qml_expression_failed");
        return value;
    }
    void require(bool value, const char* code) { if (!value) throw std::runtime_error(code); }
    void passed(const char* name) { checks_.append(QString::fromLatin1(name)); }
    void snapshot(const QString& name) {
        if (!artifacts_) return;
        const auto image = window_->grabWindow(); require(!image.isNull(), "evidence_capture_failed");
        QFile file(QDir(artifacts_->path()).filePath(name));
        require(file.open(QIODevice::WriteOnly | QIODevice::NewOnly), "evidence_file_creation_failed");
        require(image.save(&file, "PNG") && file.flush(), "evidence_png_write_failed");
    }
    QQuickItem* find(QQuickItem* root, const QString& value, bool byText = false) {
        if (!root) return nullptr;
        if ((byText ? root->property("text").toString() : root->objectName()) == value) return root;
        for (auto* child : root->childItems()) if (auto* result = find(child, value, byText)) return result;
        return nullptr;
    }
    QQuickItem* item(const QString& name, bool byText = false) {
        auto* result = find(window_->contentItem(), name, byText);
        require(result, "ui_item_missing"); return result;
    }
    void mouse(QEvent::Type type, QPointF point, Qt::MouseButton button, Qt::MouseButtons held, Qt::KeyboardModifiers modifiers = Qt::NoModifier) {
        QMouseEvent event(type, point, point, point, button, held, modifiers);
        QCoreApplication::sendEvent(window_, &event);
    }
    void click(QPointF point) {
        mouse(QEvent::MouseButtonPress, point, Qt::LeftButton, Qt::LeftButton);
        mouse(QEvent::MouseButtonRelease, point, Qt::LeftButton, Qt::NoButton);
    }
    void click(QQuickItem* target) { click(target->mapToScene(QPointF(target->width()/2, target->height()/2))); }
    void controlClick(QQuickItem* target) {
        const auto point = target->mapToScene(QPointF(target->width()/2, target->height()/2));
        mouse(QEvent::MouseButtonPress, point, Qt::LeftButton, Qt::LeftButton, Qt::ControlModifier);
        mouse(QEvent::MouseButtonRelease, point, Qt::LeftButton, Qt::NoButton, Qt::ControlModifier);
    }
    void shiftClick(QQuickItem* target) {
        const auto point = target->mapToScene(QPointF(target->width()/2, target->height()/2));
        mouse(QEvent::MouseButtonPress, point, Qt::LeftButton, Qt::LeftButton, Qt::ShiftModifier);
        mouse(QEvent::MouseButtonRelease, point, Qt::LeftButton, Qt::NoButton, Qt::ShiftModifier);
    }
    void key(int code, Qt::KeyboardModifiers modifiers = Qt::NoModifier, const QString& text = {}) {
        QKeyEvent press(QEvent::KeyPress, code, modifiers, text), release(QEvent::KeyRelease, code, modifiers, text);
        QCoreApplication::sendEvent(window_, &press); QCoreApplication::sendEvent(window_, &release);
    }
    void enter(const QString& value) {
        click(item("numericProperty_capacity"));
        key(Qt::Key_A, Qt::ControlModifier); key(Qt::Key_unknown, Qt::NoModifier, value); key(Qt::Key_Return);
    }
    QJsonObject tankPosition() {
        return controller_->document()["layout"].toObject()["positions"].toObject()["tank_a"].toObject();
    }
    void preserved() {
        require(controller_->networkHash() == hash_ && controller_->runId() == run_
            && controller_->simulationRevision() == revision_ && controller_->canApprove(), "numerical_evidence_changed");
    }
    void navigationPreserved() {
        preserved();
        require(controller_->editorDocument() == observed_project_
            && controller_->editorRevision() == navigation_editor_revision_, "connection_navigation_edited_project");
    }
    void filterNetwork(const QString& value) {
        click(item("networkFilter")); key(Qt::Key_A, Qt::ControlModifier);
        key(Qt::Key_unknown, Qt::NoModifier, value);
    }
    void routeFocused() {
        require(eval("app.fittedView && (function(){const r=app.controller.routes.find(row=>row.id===app.controller.selectedId);"
            "return !!r && Math.abs(cameraRig.x-(r.sx+r.tx)/2)<1e-6 && Math.abs(cameraRig.z-(r.sz+r.tz)/2)<1e-6})()").toBool(),
            "selected_route_endpoints_not_framed");
        navigationPreserved();
    }
    bool cueFramesReady() {
        if (rendered_frames_-cue_frame_mark_>=2) return true;
        require(cue_settle_wait_.elapsed()<cue_frame_deadline_ms_,"route_cue_frames_did_not_settle"); window_->update(); return false;
    }
    void markCueFrames(int deadlineMs=2000) { cue_frame_deadline_ms_=deadlineMs; cue_frame_mark_=rendered_frames_; cue_settle_wait_.start(); window_->update(); }
    void forecastResizeFits(bool empty) {
        auto* panel=item("forecastPanel"); auto* scroll=item("forecastBodyScroll");
        const auto bounds=scroll->mapRectToItem(panel,QRectF(0,0,scroll->width(),scroll->height()));
        require(scroll->clip() && bounds.top()>=0 && bounds.bottom()<=panel->height()+1,"forecast_body_outside_panel");
        if (empty) {
            auto* card=item("forecastEmptyState"); auto* content=item("forecastEmptyContent");
            const auto text=content->mapRectToItem(card,QRectF(0,0,content->width(),content->height()));
            require(text.left()>=0 && text.top()>=0 && text.right()<=card->width()+1 && text.bottom()<=card->height()+1,
                "forecast_empty_content_outside_card");
            require(controller_->runId().isEmpty() && !controller_->canApprove(),"forecast_resize_created_a_run_or_approval");
        } else {
            auto* timeline=item("forecastTimeline"); auto* canvas=item("forecastContent");
            const auto footer=timeline->mapRectToItem(canvas,QRectF(0,0,timeline->width(),timeline->height()));
            require(footer.top()>=0 && footer.bottom()<=canvas->height()+1 && eval("plotted.available",panel).toBool(),
                "forecast_resize_clipped_timeline_or_lost_plot");
            preserved();
        }
        require(controller_->editorDocument()==forecast_resize_project_,"forecast_resize_changed_project");
    }
    void planLayoutFixture(bool wide) {
        const auto physical=controller_->networkHash(), run=controller_->runId();
        const auto revision=controller_->simulationRevision();
        auto document=controller_->editorDocument(); const bool wrapped=document.value("version")==2;
        auto project=wrapped ? document.value("project").toObject():document;
        auto layout=project.value("layout").toObject(), positions=layout.value("positions").toObject();
        const auto assets=controller_->assets(); require(assets.size()==6,"plan_fit_fixture_asset_count_wrong");
        for (qsizetype i=0;i<assets.size();++i) {
            const auto id=assets[i].toMap().value("id").toString();
            const double along=(double(i)-2.5)*600, across=i%2 ? 100:-100;
            positions[id]=QJsonObject{{"x",wide ? along:across},{"z",wide ? across:along}};
        }
        layout["positions"]=positions; project["layout"]=layout;
        if (wrapped) document["project"]=project; else document=project;
        require(controller_->restore(document),"plan_fit_layout_fixture_restore_failed");
        require(controller_->networkHash()==physical && controller_->runId()==run
            && controller_->simulationRevision()==revision && controller_->canApprove(),"plan_fit_layout_changed_numerical_evidence");
        observed_project_=controller_->editorDocument(); hash_=physical; run_=run; revision_=revision;
        navigation_editor_revision_=controller_->editorRevision();
    }
    QJsonObject planFootprints(const QString& name,const QStringList& ids={}) {
        navigationPreserved(); QJsonArray chosen; for (const auto& id:ids) chosen.append(id);
        const auto source=QStringLiteral(
            "JSON.stringify((function(){const selected=%1,f=app.planFit,b=f.usable,rows=app.sceneAssets.filter(r=>!selected.length||selected.indexOf(r.id)>=0);"
            // Qt Quick 3D camera properties are float, while the helper uses JS
            // doubles. Allow float storage precision; rendered corner containment
            // below remains an independent one-pixel bound.
            "const nearFloat=(actual,expected)=>Math.abs(actual-expected)<=Math.max(1e-6,Math.abs(expected)*1.2e-7);"
            "let valid=app.topView&&app.planZoom===1&&rows.length===(selected.length||app.sceneAssets.length)"
            "&&[b.x,b.y,b.width,b.height,f.magnification,f.cameraX,f.cameraZ,planCamera.x,planCamera.z,planCamera.horizontalMagnification,planCamera.verticalMagnification].every(Number.isFinite)"
            "&&b.width>0&&b.height>0&&f.magnification>0&&nearFloat(planCamera.x,f.cameraX)&&nearFloat(planCamera.z,f.cameraZ)"
            "&&nearFloat(planCamera.horizontalMagnification,f.magnification)&&nearFloat(planCamera.verticalMagnification,f.magnification);"
            "const raw=app.framedBounds,xs=rows.map(r=>r.x),zs=rows.map(r=>r.z);"
            "valid=valid&&raw&&Math.abs(raw.minX-Math.min(...xs))<1e-6&&Math.abs(raw.maxX-Math.max(...xs))<1e-6"
            "&&Math.abs(raw.minZ-Math.min(...zs))<1e-6&&Math.abs(raw.maxZ-Math.max(...zs))<1e-6"
            "&&app.frameScope===(selected.length?'selection':'network');"
            "const corners=[];for(const row of rows)for(const dx of [-70,70])for(const dz of [-90,90]){"
            "const p=view.mapFrom3DScene(Qt.vector3d(row.x+dx,40,row.z+dz));"
            "const inside=[p.x,p.y,p.z].every(Number.isFinite)&&p.z>0&&p.x>=b.x-1&&p.x<=b.x+b.width+1&&p.y>=b.y-1&&p.y<=b.y+b.height+1;"
            "valid=valid&&inside;corners.push({id:row.id,x:p.x,y:p.y,inside:inside})}"
            "return {valid:valid,fit:f,view_width:view.width,view_height:view.height,"
            "camera:{x:planCamera.x,z:planCamera.z,horizontal:planCamera.horizontalMagnification,vertical:planCamera.verticalMagnification},corners:corners}})())")
            .arg(QString::fromUtf8(QJsonDocument(chosen).toJson(QJsonDocument::Compact)));
        const auto result=QJsonDocument::fromJson(eval(source).toString().toUtf8()).object();
        workspace_observations_["plan_fit_"+name]=result;
        require(result.value("valid").toBool(),"plan_fit_actual_camera_or_body_footprints_outside_usable_view");
        return result;
    }
    QJsonObject panelPreferences() {
        return QJsonDocument::fromJson(eval("JSON.stringify(workspaceSettings.values)").toString().toUtf8()).object();
    }
    void planWheel() {
        const auto point=eval("view.mapToItem(null,Qt.point(app.planFit.usable.x+app.planFit.usable.width/2,app.planFit.usable.y+app.planFit.usable.height/2))").toPointF();
        QWheelEvent event(point,point,QPoint{},QPoint(0,120),Qt::NoButton,Qt::NoModifier,Qt::NoScrollPhase,false);
        QCoreApplication::sendEvent(window_,&event);
    }
    void planPan() {
        const auto point=eval("view.mapToItem(null,Qt.point(app.planFit.usable.x+app.planFit.usable.width/2,app.planFit.usable.y+app.planFit.usable.height/2))").toPointF();
        mouse(QEvent::MouseButtonPress,point,Qt::MiddleButton,Qt::MiddleButton);
        mouse(QEvent::MouseMove,point+QPointF(-12,-8),Qt::NoButton,Qt::MiddleButton);
        mouse(QEvent::MouseButtonRelease,point+QPointF(-12,-8),Qt::MiddleButton,Qt::NoButton);
    }
    QJsonObject selectedCue() {
        return QJsonDocument::fromJson(eval(QStringLiteral(
            "JSON.stringify((function(){const id=app.controller.selectedId,seen=new Set(),active=[],models=[];let cue=null,pipeRed=false;"
            "function visit(n){if(!n||seen.has(n))return;seen.add(n);const name=String(n.objectName);"
            "if(name.indexOf('selectedRouteCue:')===0&&n.active&&n.visible)active.push(name);"
            "if(name==='selectedRouteCue:'+id)cue=n;"
            "if(n.pickName==='route:'+id&&typeof n.span==='number'){const c=n.tint;pipeRed=c.r>0.85&&c.g<0.6&&c.b<0.6}"
            "if(name==='selectedRouteCueShaft:'+id||name==='selectedRouteCueHead:'+id){const c=n.materials[0].baseColor;"
            "models.push({name:name,source:String(n.source),pickable:n.pickable,neutral:c.r>0.8&&c.g>0.8&&c.b>0.8,"
            "position:[n.position.x,n.position.y,n.position.z],scale:[n.scale.x,n.scale.y,n.scale.z]})}"
            "for(const child of n.children||[])visit(child);if(n.item)visit(n.item)}visit(view.scene);"
            "const r=app.controller.routes.find(row=>row.id===id),p=r?view.mapFrom3DScene(Qt.vector3d((r.sx+r.tx)/2,8,(r.sz+r.tz)/2)):null;"
            "const hit=p&&p.z>0?view.pick(p.x,p.y):null;"
            "return {active:active,found:!!cue,visible:!!cue&&cue.visible,pipe_red:pipeRed,body_hit:hit&&hit.objectHit?hit.objectHit.objectName:'',"
            "geometry:cue?{position:[cue.position.x,cue.position.y,cue.position.z],rotation:[cue.rotation.scalar,cue.rotation.x,cue.rotation.y,cue.rotation.z],"
            "models:models.sort((a,b)=>a.name<b.name?-1:a.name>b.name?1:0)}:{}}})())")).toString().toUtf8()).object();
    }
    void routeCueSummary(const QString& id,const QString& status,double setting) {
        navigationPreserved();
        require(controller_->selectedId()==id && item("sceneRouteSummary")->isVisible(),"route_selection_summary_missing");
        require(item("sceneRouteIdentity")->property("text").toString()==id
            && item("sceneRouteState")->property("text").toString()==status,"route_summary_identity_or_state_wrong");
        require(eval("app.selectedNetworkRoute.id===app.controller.selectedId && app.selectedNetworkRoute.label===routeSummary.row.label").toBool(),
            "route_summary_source_identity_wrong");
        require(eval("app.selectedNetworkRoute.flow").toDouble()==setting
            && item("sceneRouteSetting")->property("text").toString().contains("setpoint"),"route_summary_configured_setting_wrong");
        require(eval("routeSummary.Accessible.name").toString().contains("not measured delivery"),
            "route_summary_omits_direction_disclaimer");
        require(eval("(function(){const c=routeSummary;return c.x>=0&&c.y>=0&&c.width>0&&c.height>0&&c.x+c.width<=scenePanel.width"
            "&&c.y+c.height<=scenePanel.height&&sceneLabelLayer.positioned.every(r=>!(r.x<c.x+c.width&&r.x+r.width>c.x&&r.y<c.y+c.height&&r.y+r.height>c.y))})()").toBool(),
            "route_summary_clipped_or_overlaps_scene_labels");
        const auto cue=selectedCue(), geometry=cue.value("geometry").toObject();
        const auto models=geometry.value("models").toArray();
        workspace_observations_["route_cue_"+id]=cue;
        require(cue.value("found").toBool() && cue.value("visible").toBool() && cue.value("active").toArray().size()==1
            && models.size()==2,"selected_route_cue_missing_or_unbounded");
        for (const auto& value:models) {
            const auto model=value.toObject();
            require(model.contains("pickable") && !model.value("pickable").toBool() && model.value("neutral").toBool(),
                "route_selection_cue_intercepts_picking_or_availability_colour");
        }
        require(cue.value("body_hit").toString()=="route:"+id,"route_cue_changed_primary_picking");
    }
    void labelsMatchProjection(const QString& viewName) {
        const auto result=QJsonDocument::fromJson(eval(QStringLiteral(
            "JSON.stringify((function(){const rows=app.controller.assets;const checks=sceneLabelLayer.positioned.map(label=>{"
            "const asset=rows.find(row=>row.id===label.id);if(!asset)return {id:label.id,passed:false};"
            "const p=view.mapFrom3DScene(Qt.vector3d(asset.x,108,asset.z));"
            "const finite=[label.anchorX,label.anchorY,p.x,p.y].every(Number.isFinite);"
            "const error=Math.max(Math.abs(label.anchorX-p.x),Math.abs(label.anchorY-p.y));"
            "return {id:label.id,anchor_x:label.anchorX,anchor_y:label.anchorY,actual_x:p.x,actual_y:p.y,error_px:error,passed:finite&&error<=1}});"
            "return {label_count:checks.length,passed:checks.length>0&&checks.every(row=>row.passed),labels:checks}})())")).toString().toUtf8()).object();
        workspace_observations_["label_projection_"+viewName]=result;
        navigationPreserved();
        require(result.value("passed").toBool(),"scene_label_projection_stale_after_view_change");
    }
    QJsonObject focusObservation() {
        auto* field = item("numericProperty_capacity");
        auto* focused = window_->activeFocusItem();
        return {{"window_active", window_->isActive()}, {"notice_visible", item("actionFeedbackCard")->isVisible()},
            {"feedback_id", controller_->actionFeedback().value("id").toString()},
            {"feedback_code", controller_->actionFeedback().value("code").toString()},
            {"field_text", field->property("text").toString()},
            {"field_pending_commit", field->property("pendingUserCommit").toBool()},
            {"field_active_focus", field->hasActiveFocus()},
            {"active_focus_item", focused ? focused->objectName() : QString()},
            {"connect_mode", eval("app.connectMode").toBool()}};
    }
    void finish(bool ok, const QString& code) {
        if (done_) return;
        done_ = true; timer_.stop();
        QJsonObject result{{"check", compare_settings_only_ ? "ui_compare_settings" : observed_only_ ? "ui_observed_advisory":"ui_overhaul"}, {"passed", ok}, {"code", code},
            {"checks", checks_}, {"phase", phase_}, {"observed", observed_},
            {"device_pixel_ratio", window_ ? window_->devicePixelRatio() : 0.0},
            {"window_width", window_ ? window_->width() : 0}, {"window_height", window_ ? window_->height() : 0},
            {"elapsed_ms", elapsed_.elapsed()}, {"hosted_model_requests", "not_requested"}};
        if (artifacts_ && artifacts_->isValid()) result["artifact_directory"] = artifacts_->path();
        if (model_requests_) result["forbidden_model_request_attempts"] = model_requests_();
        if (!workspace_observations_.isEmpty()) result["workspace_observations"] = workspace_observations_;
        QTextStream stream(stdout); stream << QJsonDocument(result).toJson(QJsonDocument::Compact) << '\n'; stream.flush();
        app_->exit(ok ? 0 : 8);
    }
    history::HistoryController* comparisonHistory() {
        auto* result=window_->property("history").value<history::HistoryController*>();
        require(result,"comparison_history_unavailable"); return result;
    }
    void comparisonPreserved() {
        require(controller_->editorDocument()==comparison_project_ && controller_->networkHash()==hash_
            && controller_->simulationRevision()==revision_ && controller_->runId()==run_
            && !controller_->canApprove(),"comparison_or_settings_changed_project_or_approval");
        require(model_requests_ && model_requests_()==0,"comparison_or_settings_requested_model");
    }
    void rightScrollBar(const QString& scrollName) {
        require(eval("(function(){const b=ScrollBar.vertical;return b.visible && b.height>=height-1"
            "&&b.x>=width-b.width-1 && b.x+b.width<=width+1})()",item(scrollName)).toBool(),
            "scrollbar_not_full_height_on_right_edge");
    }
    void revealInScroll(const QString& scrollName,const QString& targetName) {
        auto* flick=qobject_cast<QQuickItem*>(item(scrollName)->property("contentItem").value<QObject*>());
        require(flick,"comparison_scroll_content_missing"); auto* target=item(targetName);
        const auto rectangle=target->mapRectToItem(flick,QRectF(0,0,target->width(),target->height()));
        double y=flick->property("contentY").toDouble();
        if(rectangle.top()<8) y+=rectangle.top()-8;
        else if(rectangle.bottom()>flick->height()-8) y+=rectangle.bottom()-(flick->height()-8);
        flick->setProperty("contentY",std::clamp(y,0.0,std::max(0.0,flick->property("contentHeight").toDouble()-flick->height())));
        markCueFrames();
    }
    QJsonObject comparisonFixture() {
        auto project=example_project(); project.network=main_pump_failure(project.network);
        agent::SimulationGateway gateway(project.network,physics_hash(project));
        gateway.execute("get_network_state",{}); gateway.execute("get_control_catalog",{});
        const auto base=gateway.execute("forecast_baseline",{{"snapshot_id",gateway.snapshotId()}}).value("run").toObject();
        QJsonArray controls;
        for(const auto& control:incident_controls("mixed")) controls.append(QJsonObject{{"control_id",QString::fromStdString(control.id)},{"value",control.value}});
        const auto result=gateway.execute("simulate_candidates",{{"snapshot_id",gateway.snapshotId()},
            {"candidates",QJsonArray{QJsonObject{{"candidate_id","Mixed response with retained numerical evidence and independent validation"},{"controls",controls}}}}}).value("runs").toArray();
        require(result.size()==1,"comparison_native_fixture_failed");
        const auto candidate=result.first().toObject().value("run_id").toString();
        gateway.execute("validate_run",{{"snapshot_id",gateway.snapshotId()},{"run_id",candidate}});
        const auto stamp=QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);
        return {{"archive_version",1},{"session_id",QUuid::createUuid().toString(QUuid::WithoutBraces)},
            {"checkpoint_sequence",1},{"created_at",stamp},{"updated_at",stamp},{"status","completed"},
            {"objective","Offline UI check using actual native forecasts; no model execution."},
            {"snapshot",gateway.snapshot()},{"model",QJsonObject{}},{"events",QJsonArray{}},{"approvals",QJsonArray{}},
            {"runs",QJsonArray{gateway.runDetails(base.value("run_id").toString()),gateway.runDetails(candidate)}},
            {"recommendation",gateway.recommendation(candidate)}};
    }
    void showComparison(QJsonObject archive) {
        archive["checkpoint_sequence"]=++comparison_checkpoint_;
        comparisonHistory()->setLiveSession(archive);
    }
    void compareSettingsTick() {
        comparisonPreserved();
        if (!cueFramesReady()) return;
        auto* history=comparisonHistory();
        switch(phase_) {
        case 240:
            require(item("comparisonEmptyState")->isVisible() && !item("trajectoryChart")->isVisible(),"empty_comparison_has_dead_controls");
            rightScrollBar("comparisonScroll");
            snapshot("02-comparison-empty.png"); revealInScroll("comparisonScroll","comparisonBrowseHistory"); ++phase_; return;
        case 241:
            click(item("comparisonBrowseHistory")); markCueFrames(); break;
        case 242:
            require(window_->property("resultTab").toInt()==3,"comparison_history_navigation_not_synchronized");
            click(item("Compare",true)); markCueFrames(); break;
        case 243:
            revealInScroll("comparisonScroll","comparisonGoInvestigate"); ++phase_; return;
        case 244:
            click(item("comparisonGoInvestigate")); markCueFrames(); break;
        case 245: {
            require(window_->property("resultTab").toInt()==2 && !history->historical(),"comparison_investigation_navigation_not_synchronized");
            passed("empty_comparison_and_both_navigation_actions");
            comparison_archive_=comparisonFixture(); auto single=comparison_archive_;
            single["runs"]=QJsonArray{single.value("runs").toArray().first()}; single["recommendation"]=QJsonObject{};
            showComparison(single); eval("app.openResults(1)"); markCueFrames(); break;
        }
        case 246:
            require(!item("comparisonEmptyState")->isVisible() && history->chart().value("series").toList().size()==1
                && !item("comparisonCostDelta")->isVisible() && !item("comparisonControlChanges")->isEnabled(),"single_run_presented_as_pair");
            require(history->chart().value("y_max").toDouble()>history->chart().value("capacity_m3").toDouble(),"comparison_clipped_overflow");
            showComparison(comparison_archive_); markCueFrames(); break;
        case 247:
            require(history->comparison().value("compatible").toBool() && history->chart().value("series").toList().size()==2,"compatible_pair_not_overlaid");
            snapshot("03-comparison-compatible.png"); revealInScroll("comparisonScroll","comparisonControlChanges"); ++phase_; return;
        case 248:
            require(item("comparisonCostDelta")->isVisible() && item("comparisonControlChanges")->isEnabled(),"compatible_pair_actions_missing");
            snapshot("04-comparison-metrics.png"); click(item("comparisonControlChanges")); markCueFrames(); break;
        case 249:
            require(item("runComparisonPanel")->property("dialogOpen").toBool() && !eval("app.sceneKeysEnabled()").toBool(),"comparison_dialog_or_shortcut_guard_missing");
            key(Qt::Key_Escape); markCueFrames(); break;
        case 250: {
            require(!item("runComparisonPanel")->property("dialogOpen").toBool() && item("comparisonControlChanges")->hasActiveFocus(),"comparison_dialog_close_or_focus_return_failed");
            // Deliberately incompatible display fixture, never an imported or qualified run.
            auto incompatible=comparison_archive_; auto runs=incompatible.value("runs").toArray();
            auto detail=runs.last().toObject(), run=detail.value("run").toObject(); run["snapshot_id"]="different-ui-fixture-snapshot";
            detail["run"]=run; runs[1]=detail; incompatible["runs"]=runs;
            showComparison(incompatible); markCueFrames(); break;
        }
        case 251: {
            require(!history->comparison().value("compatible").toBool() && history->chart().value("series").toList().size()==1
                && !item("comparisonCostDelta")->isVisible() && !item("comparisonControlChanges")->isEnabled(),"incompatible_pair_has_delta_or_overlay");
            const auto id=history->baselineRunId(); const auto before=history->selectedArchive(); history->selectRuns(id,id);
            require(history->selectedArchive()==before,"run_selection_mutated_archive");
            auto retired=comparison_archive_; auto retiredRuns=retired.value("runs").toArray();
            for(qsizetype i=0;i<retiredRuns.size();++i) { auto detail=retiredRuns[i].toObject(), run=detail.value("run").toObject(); run["current"]=false; detail["run"]=run; retiredRuns[i]=detail; }
            retired["runs"]=retiredRuns; showComparison(retired); markCueFrames(); break;
        }
        case 252: {
            require(!item("comparisonCostDelta")->isVisible() && !item("comparisonControlChanges")->isEnabled()
                && item("comparisonSourceWarning")->isVisible(),"same_or_retired_run_has_misleading_comparison");
            passed("single_matching_mismatched_and_same_run_preserve_controls_and_unclipped_evidence");
            showComparison(comparison_archive_);
            const auto rows=comparison_archive_.value("runs").toArray();
            history->selectRuns(rows.first().toObject().value("run").toObject().value("run_id").toString(),rows.last().toObject().value("run").toObject().value("run_id").toString());
            comparison_storage_=std::make_unique<QTemporaryDir>(); require(comparison_storage_->isValid(),"comparison_import_directory_failed");
            QFile file(comparison_storage_->filePath("native-session.json")); require(file.open(QIODevice::WriteOnly|QIODevice::NewOnly),"comparison_archive_write_failed");
            const auto bytes=QJsonDocument(comparison_archive_).toJson(); require(file.write(bytes)==bytes.size() && file.flush(),"comparison_archive_write_failed"); file.close();
            history->importSession(QUrl::fromLocalFile(file.fileName())); markCueFrames(); break;
        }
        case 253:
            if (history->busy()) { markCueFrames(); return; }
            require(history->historical(),"comparison_history_import_failed");
            require(item("comparisonReturnCurrent")->isVisible() && !controller_->canApprove(),"historical_comparison_restored_authority");
            snapshot("05-comparison-historical.png"); click(item("comparisonReturnCurrent")); markCueFrames(); break;
        case 254:
            require(!history->historical(),"comparison_return_current_failed");
            passed("historical_comparison_is_read_only"); click(item("openSettings")); markCueFrames(); break;
        case 255:
            require(eval("settingsDialog.visible").toBool() && !eval("app.sceneKeysEnabled()").toBool(),"settings_did_not_block_scene_keys");
            rightScrollBar("settingsWorkspaceScroll");
            // Quality changes rebuild 3D resources on ASan/WARP. Keep both rendered
            // frames and the global watchdog; give preset transitions a bounded 6 s.
            click(item("settingsRenderingQuality")); key(Qt::Key_Home); key(Qt::Key_Return); markCueFrames(6000); break;
        case 256:
            require(eval("workspaceSettings.values.quality").toInt()==0,"quality_control_not_bound_to_existing_preferences");
            revealInScroll("settingsWorkspaceScroll","settingsGridVisible"); ++phase_; return;
        case 257:
            click(item("settingsGridVisible")); markCueFrames(); break;
        case 258:
            require(!window_->property("siteGridVisible").toBool(),"settings_grid_not_shared_with_scene");
            revealInScroll("settingsWorkspaceScroll","restoreWorkspacePreferences"); ++phase_; return;
        case 259:
            click(item("restoreWorkspacePreferences")); markCueFrames(6000); break;
        case 260:
            require(window_->property("siteGridVisible").toBool() && eval("workspaceSettings.values.quality").toInt()==1,"preference_reset_did_not_restore_display_defaults");
            snapshot("06-settings-workspace.png"); click(item("settingsConnectionsTab")); markCueFrames(); break;
        case 261:
            require(!eval("app.credentials.configured || app.credentials.saved").toBool(),"settings_ui_discovered_user_credentials");
            click(item("settingsManageKey")); markCueFrames(); break;
        case 262:
            comparison_key_input_=item("nemotronApiKeyInput"); comparison_save_key_=item("saveNemotronKey");
            require(eval("keyDialog.visible").toBool() && item("nemotronApiKeyInput")->property("echoMode").toInt()==2,"shared_key_dialog_not_masked");
            key(Qt::Key_unknown,Qt::NoModifier,"ui-placeholder-not-a-real-key"); key(Qt::Key_Escape); markCueFrames(); break;
        case 263:
            require(!eval("keyDialog.visible").toBool() && comparison_key_input_ && comparison_key_input_->property("text").toString().isEmpty()
                && item("settingsManageKey")->hasActiveFocus() && !eval("app.credentials.saved").toBool(),"key_cancel_retained_input_or_lost_focus");
            eval("settingsDialog.investigationRunning=true; keyDialog.investigationRunning=true");
            require(!item("settingsManageKey")->isEnabled() && comparison_save_key_ && !comparison_save_key_->isEnabled(),"busy_settings_allow_credential_change");
            eval("settingsDialog.investigationRunning=Qt.binding(function(){return app.credentialChangeBlocked}); keyDialog.investigationRunning=Qt.binding(function(){return app.credentialChangeBlocked})");
            click(item("settingsCheckWorker")); markCueFrames(); break;
        case 264:
            if(eval("app.scientificWorker.busy").toBool()) { markCueFrames(); return; }
            require(eval("app.scientificWorker.readiness.health_status").toString()=="failed"
                && !item("settingsWorkerStatus")->property("text").toString().isEmpty(),"missing_worker_not_visible_in_settings");
            snapshot("07-settings-connections.png");
            passed("settings_preferences_shared_key_masking_busy_gates_and_missing_worker");
            key(Qt::Key_Escape); markCueFrames(); break;
        case 265:
            require(!eval("settingsDialog.visible").toBool() && item("openSettings")->hasActiveFocus(),"settings_close_did_not_restore_focus");
            click(item("switchToOperations")); markCueFrames(); break;
        case 266:
            require(window_->property("workspace").toString()=="eog","settings_operations_fixture_failed");
            key(Qt::Key_Comma,Qt::ControlModifier,","); markCueFrames(); break;
        case 267:
            require(eval("settingsDialog.visible").toBool(),"settings_shortcut_unavailable_in_operations");
            click(item("settingsInformationTab")); markCueFrames(); break;
        case 268:
            snapshot("08-settings-information.png"); click(item("settingsAbout")); markCueFrames(); break;
        case 269:
            require(eval("aboutDialog.visible").toBool() && !eval("settingsDialog.visible").toBool(),"settings_about_navigation_failed");
            eval("aboutDialog.close()"); click(item("switchToWorkbench")); markCueFrames(); break;
        case 270:
            passed("settings_both_workspaces_keyboard_escape_focus_and_about");
            comparisonPreserved(); finish(true,{}); return;
        default: throw std::runtime_error("invalid_compare_settings_phase");
        }
        ++phase_;
    }
    void tick() {
        try {
            require(window_ && controller_, "ui_destroyed");
            // Includes actual native pipe execution, SQL retention/reload and
            // the compact History-to-viewer action in addition to editor checks.
            require(elapsed_.elapsed() < (observed_only_ ? 40000 : 60000), "ui_check_timeout");
            if(compare_settings_only_ && phase_>=240) { compareSettingsTick(); return; }
            switch (phase_) {
            case 0: {
                if (!window_->isVisible() || !window_->isExposed() || !window_->property("sceneReady").toBool()) return;
                require(!QImage(":/breakwater/assets/breakwater-logo.svg").isNull()
                    && !QImage(":/breakwater/assets/icons/play.svg").isNull(), "bundled_svg_unavailable");
                auto* open = item("Open", true);
                require(open->property("accessibleName").toString() == "Open"
                    && open->property("tooltip").toString() == "Open project", "icon_accessibility_missing");
                require(!item("Undo", true)->isEnabled() && item("Run forecast", true)->isEnabled()
                    && !item("Approve simulated preview", true)->isEnabled(), "initial_action_gates_wrong");
                passed("native_assets_accessibility_action_gates");
                const auto parent = qEnvironmentVariable("BREAKWATER_UI_EVIDENCE_DIR");
                if (!parent.isEmpty()) {
                    const QFileInfo directory(parent);
#ifdef Q_OS_WIN
                    const auto absolute = std::filesystem::path(parent.toStdWString()).is_absolute();
#else
                    const auto absolute = QDir::isAbsolutePath(parent);
#endif
                    require(absolute && directory.isDir() && !directory.canonicalFilePath().isEmpty(), "evidence_parent_invalid");
                    artifacts_ = std::make_unique<QTemporaryDir>(QDir(directory.canonicalFilePath()).filePath("breakwater-ui-XXXXXX"));
                    require(artifacts_->isValid(), "evidence_directory_creation_failed"); artifacts_->setAutoRemove(false);
                }
                snapshot("01-workbench-ready.png");
                if(compare_settings_only_) {
                    comparison_project_=controller_->editorDocument(); hash_=controller_->networkHash();
                    run_=controller_->runId(); revision_=controller_->simulationRevision();
                    eval("app.openResults(1)"); phase_=240; markCueFrames(); return;
                }
                if (observed_only_) {
                    controller_->loadExample("monitoring"); observed_project_=controller_->editorDocument();
                    eval("app.openTelemetry()"); phase_=52; break;
                }
                require(item("forecastEmptyRun")->isVisible() && item("forecastEmptyRun")->isEnabled(),"empty_forecast_action_unavailable");
                forecast_resize_project_=controller_->editorDocument();
                forecast_resize_width_=window_->width(); forecast_resize_height_=window_->height();
                forecast_resize_dock_=eval("workspaceSettings.values.dockHeight").toInt();
                eval("workspaceSettings.setValue('dockHeight',160)"); window_->resize(1008,672);
                markCueFrames(); phase_=300; return;
            }
            case 300:
                if (!cueFramesReady()) return;
                forecastResizeFits(true); snapshot("25-forecast-short-dock.png");
                window_->resize(960,540); markCueFrames(); phase_=301; return;
            case 301:
                if (!cueFramesReady()) return;
                forecastResizeFits(true); snapshot("26-forecast-minimum-empty.png");
                revealInScroll("forecastBodyScroll","forecastEmptyRun"); phase_=302; return;
            case 302:
                if (!cueFramesReady()) return;
                click(item("forecastEmptyRun"));
                window_->resize(forecast_resize_width_,forecast_resize_height_);
                eval(QStringLiteral("workspaceSettings.setValue('dockHeight',%1)").arg(forecast_resize_dock_));
                phase_=64; return;
            case 303:
                if (!cueFramesReady()) return;
                forecastResizeFits(false); snapshot("27-forecast-minimum-executed.png");
                revealInScroll("forecastBodyScroll","forecastTimeline"); phase_=305; return;
            case 305: {
                if (!cueFramesReady()) return;
                auto* flick=qobject_cast<QQuickItem*>(item("forecastBodyScroll")->property("contentItem").value<QObject*>());
                require(flick,"forecast_scroll_viewport_missing"); auto* timeline=item("forecastTimeline");
                const auto footer=timeline->mapRectToItem(flick,QRectF(0,0,timeline->width(),timeline->height()));
                require(footer.top()>=-1 && footer.bottom()<=flick->height()+1,"forecast_timeline_not_scrollable_into_view");
                snapshot("28-forecast-scrolled-timeline.png");
                window_->resize(forecast_resize_width_,forecast_resize_height_);
                eval(QStringLiteral("workspaceSettings.setValue('dockHeight',%1)").arg(forecast_resize_dock_));
                markCueFrames(); phase_=304; return;
            }
            case 304:
                if (!cueFramesReady()) return;
                preserved(); passed("empty_and_executed_forecast_resize_scroll_preserve_project_and_evidence");
                key(Qt::Key_F1); phase_=65; return;
            case 1:
                if (controller_->busy()) return;
                require(!controller_->runId().isEmpty() && controller_->forecastHorizonH() == 1.25
                    && controller_->canApprove(), "actual_forecast_unavailable");
                hash_ = controller_->networkHash(); run_ = controller_->runId(); revision_ = controller_->simulationRevision();
                position_ = tankPosition(); passed("actual_1_25h_forecast"); break;
            case 2: {
                const auto json = eval(QStringLiteral("JSON.stringify((function(){ let a; for (const row of app.controller.assets) if(row.id==='tank_a') a=row; const p=view.mapFrom3DScene(Qt.vector3d(a.x,40,a.z)); const hit=view.pick(p.x,p.y); let node=hit.objectHit,depth=0; while(node && node.objectName!=='equipment:tank_a'){++depth;node=node.parent} const q=view.mapToItem(null,p.x,p.y);return {name:hit.objectHit?hit.objectHit.objectName:'',depth:depth,ancestor:!!node,x:q.x,y:q.y};})())")).toString();
                const auto data = QJsonDocument::fromJson(json.toUtf8()).object();
                require(data["name"] == "asset:tank_a" && data["ancestor"].toBool() && data["depth"].toInt() >= 2, "nested_tank_pick_failed");
                tank_ = {data["x"].toDouble(), data["y"].toDouble()}; click(tank_); passed("nested_geometry_pick_and_click"); break;
            }
            case 3: {
                require(controller_->selectedId() == "tank_a", "actual_click_did_not_select");
                if (window_->property("compactLayout").toBool() && !window_->property("inspectorVisible").toBool()) {
                    click(item("Inspector", true)); return;
                }
                // Edit two successive fields in the expanded inspector. A
                // short hosted desktop can clip the second field even above
                // the old 620-pixel cutoff; use the real dock action for all sizes.
                if (eval("workspaceSettings.values.dockOpen").toBool()) {
                    click(item("Toggle results", true)); return;
                }
                // Opening a compact pane changes the viewport. Pick the actual
                // current projection before testing the next physical gesture.
                const auto point = eval("(function(){let a;for(const row of app.controller.assets)if(row.id==='tank_a')a=row;const p=view.mapFrom3DScene(Qt.vector3d(a.x,40,a.z));return view.mapToItem(null,p.x,p.y)})()").toPointF();
                tank_ = point;
                capacity_ = controller_->selected().value("capacity").toDouble();
                mouse(QEvent::MouseButtonPress, tank_, Qt::LeftButton, Qt::LeftButton); break;
            }
            case 4:
                mouse(QEvent::MouseMove, tank_ + QPointF(38, 16), Qt::NoButton, Qt::LeftButton); break;
            case 5:
                mouse(QEvent::MouseButtonRelease, tank_ + QPointF(38, 16), Qt::LeftButton, Qt::NoButton); break;
            case 6:
                require(tankPosition() != position_, "actual_drag_did_not_move"); preserved();
                snapshot("04-inspector-before-entry.png");
                passed("layout_drag_preserves_hash_run_approval"); enter("NaN"); break;
            case 7:
                observed_["entry"] = focusObservation();
                require(controller_->actionFeedback().value("code") == "property.invalid_number", "keyboard_invalid_commit_not_rejected");
                require(item("numericRejection_capacity")->isVisible()
                    && item("numericRejection_capacity")->property("text").toString().contains("NaN")
                    && item("numericRejection_capacity")->property("text").toString().contains("Accepted value:"), "inline_rejection_missing");
                require(controller_->selected().value("capacity").toDouble() == capacity_, "rejected_capacity_changed");
                preserved(); feedback_ = controller_->actionFeedback().value("id").toString(); enter("NaN"); break;
            case 8:
                require(controller_->actionFeedback().value("id").toString().toULongLong() == feedback_.toULongLong() + 1, "repeat_rejection_not_exactly_once");
                require(item("actionFeedbackCard")->isVisible(), "repeat_notice_not_visible");
                preserved(); snapshot("02-repeated-invalid-before-dismiss.png");
                feedback_ = controller_->actionFeedback().value("id").toString();
                click(item("dismissActionFeedback")); break;
            case 9:
                observed_ = {{"notice_visible", item("actionFeedbackCard")->isVisible()},
                    {"inline_visible", item("numericRejection_capacity")->isVisible()},
                    {"capacity", controller_->selected().value("capacity").toDouble()},
                    {"selected_id", controller_->selectedId()}, {"feedback_id", controller_->actionFeedback().value("id").toString()},
                    {"before_dismiss_feedback_id", feedback_}, {"field_text", item("numericProperty_capacity")->property("text").toString()}};
                require(!item("actionFeedbackCard")->isVisible(), "dismiss_notice_still_visible");
                require(item("numericRejection_capacity")->isVisible(), "dismiss_inline_evidence_removed");
                require(controller_->actionFeedback().value("id").toString() == feedback_, "dismiss_recommitted_field");
                preserved(); passed("keyboard_rejection_repeat_dismiss_preserves_evidence");
                eval("app.connectMode=true"); enter("NaN"); break;
            case 10:
                observed_["third_retry_before_escape"] = focusObservation();
                require(controller_->actionFeedback().value("id").toString().toULongLong() == feedback_.toULongLong() + 1,
                    "third_keyboard_retry_not_fresh");
                require(item("actionFeedbackCard")->isVisible(), "third_retry_notice_not_visible");
                require(eval("app.connectMode").toBool(), "connection_mode_lost_before_escape");
                item("dismissActionFeedback")->forceActiveFocus();
                observed_["focused_before_escape"] = focusObservation();
                require(item("dismissActionFeedback")->hasActiveFocus(), "notice_keyboard_focus_not_obtained");
                key(Qt::Key_Escape); break;
            case 11:
                observed_["after_focused_escape"] = focusObservation();
                require(!item("actionFeedbackCard")->isVisible(), "focused_escape_notice_not_dismissed");
                require(eval("app.connectMode").toBool(), "focused_escape_cancelled_connection");
                passed("focused_notice_escape_isolated");
                outside_shortcut_setup_.start();
                if (!window_->isActive()) window_->requestActivate();
                item("numericProperty_capacity")->forceActiveFocus();
                observed_["outside_escape_setup_started"] = focusObservation(); break;
            case 12: {
                if (!outside_escape_sent_) {
                    auto* field = item("numericProperty_capacity");
                    observed_["outside_escape_before_key"] = focusObservation();
                    require(outside_shortcut_setup_.elapsed() < 2000, "outside_shortcut_focus_unavailable");
                    if (!window_->isActive() || !field->hasActiveFocus() || window_->activeFocusItem() != field) {
                        outside_focus_settled_ = false;
                        if (!window_->isActive()) window_->requestActivate();
                        field->forceActiveFocus(); return;
                    }
                    // Window shortcuts require an active window. Let the
                    // requested activation/focus settle for an event-loop turn
                    // before exercising the real key/Shortcut delivery path.
                    if (!outside_focus_settled_) { outside_focus_settled_ = true; return; }
                    outside_escape_sent_ = true; key(Qt::Key_Escape); return;
                }
                observed_["after_outside_escape"] = focusObservation();
                require(!eval("app.connectMode").toBool(), "outside_notice_escape_did_not_cancel_connection");
                preserved(); passed("outside_notice_escape_cancels_connection"); enter(QString::number(capacity_ + 1, 'g', 17)); break;
            }
            case 13:
                require(controller_->selected().value("capacity").toDouble() == capacity_ + 1, "keyboard_valid_retry_not_applied");
                require(!item("numericRejection_capacity")->isVisible() && controller_->runId().isEmpty()
                    && !controller_->canApprove() && controller_->networkHash() != hash_, "valid_retry_not_retired");
                require(!item("Approve simulated preview", true)->isEnabled(), "stale_approval_button_enabled");
                snapshot("03-numerical-edit-retired-run.png");
                passed("valid_retry_clears_inline_retires_run_approval");
                changes_ = 0; revision_ = controller_->simulationRevision();
                click(item("numericProperty_capacity")); key(Qt::Key_A, Qt::ControlModifier);
                key(Qt::Key_unknown, Qt::NoModifier, QString::number(capacity_ + 2, 'g', 17));
                click(item("numericProperty_initial")); break;
            case 14:
                require(controller_->selected().value("capacity").toDouble() == capacity_ + 2
                    && controller_->simulationRevision() == revision_ + 1 && changes_ == 1, "focus_loss_commit_not_exactly_once");
                require(!item("numericProperty_capacity")->property("pendingUserCommit").toBool(), "focus_loss_commit_not_consumed");
                click(item("numericProperty_capacity")); break;
            case 15:
                click(item("numericProperty_initial")); break;
            case 16:
                require(changes_ == 1 && controller_->simulationRevision() == revision_ + 1
                    && controller_->selected().value("capacity").toDouble() == capacity_ + 2, "unedited_focus_loss_recommitted");
                passed("edited_focus_loss_once_unedited_focus_loss_no_commit");
                controller_->reset();
                if (window_->property("compactLayout").toBool()) click(item("Navigator", true));
                break;
            case 17:
                click(item("networkAsset:tank_a")); break;
            case 18:
                controlClick(item("networkAsset:tank_b")); break;
            case 19:
                require(controller_->selectedIds().size() == 2, "native_control_click_not_additive");
                hash_ = controller_->networkHash();
                key(Qt::Key_D, Qt::ControlModifier); break;
            case 20:
                require(controller_->assets().size() == 8 && controller_->selectedIds().size() == 2
                    && controller_->networkHash() != hash_, "native_duplicate_shortcut_failed");
                key(Qt::Key_Z, Qt::ControlModifier); break;
            case 21:
                require(controller_->assets().size() == 6 && controller_->networkHash() == hash_, "duplicate_undo_not_atomic");
                passed("native_multiselect_duplicate_atomic_undo");
                click(item("openSensorEditor")); break;
            case 22:
                require(window_->property("sensorEditorVisible").toBool() && item("openSensorEditor")->property("checked").toBool(), "sensor_toggle_highlight_missing");
                key(Qt::Key_D, Qt::ControlModifier);
                require(controller_->assets().size() == 6, "dialog_stole_scene_shortcut");
                snapshot("05-sensor-editor.png"); click(item("sensorPanelClose")); break;
            case 23:
                require(!window_->property("sensorEditorVisible").toBool() && !item("openSensorEditor")->property("checked").toBool(), "sensor_close_did_not_clear_highlight");
                click(item("openTelemetry")); break;
            case 24:
                require(window_->property("telemetryVisible").toBool() && item("openTelemetry")->property("checked").toBool(), "monitoring_toggle_highlight_missing");
                snapshot("06-monitoring.png"); click(item("telemetryPanelClose")); break;
            case 25:
                require(!window_->property("telemetryVisible").toBool(), "monitoring_close_failed");
                click(item("openPipeEvidence")); break;
            case 26:
                require(window_->property("pipeTransportVisible").toBool() && item("openPipeEvidence")->property("checked").toBool(), "pipe_toggle_highlight_missing");
                snapshot("07-pipe-evidence.png"); click(item("pipeTransportClose")); break;
            case 27:
                require(!window_->property("pipeTransportVisible").toBool(), "pipe_close_failed");
                click(item("toggleInvestigator")); break;
            case 28:
                require(window_->property("showAgent").toBool() && item("toggleInvestigator")->property("checked").toBool(), "investigator_toggle_highlight_missing");
                snapshot("08-investigation-dock.png"); click(item("toggleInvestigator")); break;
            case 29:
                require(!window_->property("showAgent").toBool() && !item("toggleInvestigator")->property("checked").toBool(), "repeated_investigator_click_did_not_close");
                passed("native_tool_highlights_close_buttons_shortcut_guard_repeat_toggle");
                eval("app.openRecordedInvestigation()"); break;
            case 30:
                observed_["recording"] = QJsonDocument::fromJson(eval(QStringLiteral(
                    "JSON.stringify({loaded:app.eog.loaded,loading:app.eog.busy,"
                    "folder_matches:app.eog.sourceFolder===String(app.portfolioExamples.operationsPath),"
                    "pending:String(app.pendingRecordedArchive).length>0,"
                    "historical:app.eogAnalysis.historical,status:app.eogAnalysis.status,"
                    "storage:app.eogAnalysis.storageStatus,trace:app.eogAnalysis.trace.length})")).toString().toUtf8()).object();
                if (!eval("app.eogAnalysis.historical").toBool()) return;
                require(eval("app.eog.loaded && app.eogAnalysis.trace.length > 0 && app.eogAnalysis.plans.length > 0 && app.eogAnalysis.summary.length > 0 && !app.eogAnalysis.canReview").toBool(),
                    "recorded_investigation_not_historical_grounded_evidence");
                passed("bundled_hosted_recording_reopens_historical_without_review_authority");
                // History restoration changes the Flow's visible actions. Wait
                // for layout/rendering even when optional PNG capture is off;
                // grabWindow() must not be what makes the mouse target settle.
                markCueFrames(); phase_=198; return;
            case 198:
                if (!cueFramesReady()) return;
                snapshot("09-recorded-investigation.png");
                require(item("openEogVerifiedExplanation")->isVisible() && item("openEogVerifiedExplanation")->isEnabled(),
                    "retained_evidence_reader_unavailable");
                require(window_->contentItem()->boundingRect().contains(item("openEogVerifiedExplanation")->mapRectToScene(
                    item("openEogVerifiedExplanation")->boundingRect())), "retained_evidence_reader_button_outside_client");
                click(item("openEogVerifiedExplanation"));
                markCueFrames(); phase_=200; return;
            case 200: {
                if (!cueFramesReady()) return;
                auto* explanation = find(window_->contentItem(), "eogVerifiedExplanationText");
                if (!explanation || !explanation->isVisible()) {
                    require(cue_settle_wait_.elapsed()<2000, "retained_evidence_reader_did_not_open");
                    window_->update(); return;
                }
                require(explanation->isVisible() && explanation->property("readOnly").toBool()
                    && explanation->property("text").toString() == eval("app.eogAnalysis.summary").toString()
                    && !item("showExactEogTrace")->property("checked").toBool()
                    && !eval("app.eogAnalysis.canReview").toBool(), "retained_evidence_reader_text_or_authority_wrong");
                auto* viewport = item("eogVerifiedExplanationScroll");
                const auto rectangle = viewport->mapRectToScene(viewport->boundingRect());
                require(rectangle.width() > 200 && rectangle.height() > 100
                    && window_->contentItem()->boundingRect().contains(rectangle), "retained_evidence_reader_outside_client");
                snapshot("09b-retained-numerical-explanation.png");
                key(Qt::Key_Escape); phase_=201; return;
            }
            case 201: {
                // Closed Qt popups detach their content from the visual tree.
                const auto* explanation = find(window_->contentItem(), "eogVerifiedExplanationText");
                require((!explanation || !explanation->isVisible())
                    && item("openEogVerifiedExplanation")->hasActiveFocus()
                    && !eval("app.eogAnalysis.canReview").toBool(), "retained_evidence_reader_close_or_focus_wrong");
                passed("full_plain_numerical_explanation_without_raw_model_disclosure_or_authority");
                eval("app.workspace='workbench'; app.compactPane='navigator'; workspaceSettings.setValue('navigatorOpen',true)");
                controller_->select("source_b");
                { SimulationConfig config; config.horizon_h = 1.25; controller_->forecastWithConfig(config); }
                phase_=31; return;
            }
            case 31:
                if (controller_->busy()) return;
                require(controller_->canApprove() && !controller_->runId().isEmpty(), "filter_forecast_unavailable");
                hash_ = controller_->networkHash(); run_ = controller_->runId(); revision_ = controller_->simulationRevision();
                click(item("networkFilter")); key(Qt::Key_A, Qt::ControlModifier); key(Qt::Key_unknown, Qt::NoModifier, "_A"); break;
            case 32:
                require(eval("JSON.stringify(app.filteredNetworkAssets.map(row=>row.id))").toString() == "[\"tank_a\",\"source_a\"]",
                    "filter_name_id_type_or_case_wrong");
                require(controller_->selectedId() == "source_b", "filter_changed_hidden_selection"); preserved();
                key(Qt::Key_D, Qt::ControlModifier);
                require(controller_->assets().size() == 6, "filter_input_stole_scene_shortcut");
                click(item("networkAsset:tank_a")); break;
            case 33:
                require(controller_->selectedId() == "tank_a", "filtered_asset_click_failed");
                eval("networkAssetList.positionViewAtIndex(1,ListView.Contain)"); break;
            case 34:
                shiftClick(item("networkAsset:source_a")); break;
            case 35:
                require(controller_->selectedIds().size() == 2 && controller_->selectedIds().contains("tank_a")
                    && controller_->selectedIds().contains("source_a"), "filtered_range_selected_hidden_asset");
                preserved(); click(item("clearNetworkFilter")); break;
            case 36:
                require(item("networkFilter")->property("text").toString().isEmpty()
                    && eval("app.filteredNetworkAssets.length").toInt() == 6 && controller_->selectedIds().size() == 2,
                    "clear_filter_changed_selection_or_did_not_restore_list");
                preserved(); passed("native_filter_preserves_evidence_selection_shortcuts_and_visible_ranges");
                eval("app.compactPane='inspector'; workspaceSettings.setValue('inspectorOpen',true)");
                for (const auto& row : controller_->routes()) {
                    const auto route = row.toMap();
                    if (route.value("pump").toBool()) { controller_->select(route.value("id").toString()); break; }
                }
                break;
            case 37: {
                auto* bar = item("propertyInspectorScrollBar"); auto* panel = item("assetInspector");
                const auto origin = bar->mapToItem(panel, QPointF(0, 0));
                const bool mirrored = bar->parentItem() && bar->parentItem()->property("mirrored").toBool();
                observed_["inspector_scrollbar"] = QJsonObject{{"x", origin.x()}, {"height", bar->height()},
                    {"panel_width", panel->width()}, {"panel_height", panel->height()}, {"size", bar->property("size").toDouble()}};
                require((mirrored ? origin.x() < panel->width() * 0.2 : origin.x() > panel->width() * 0.8)
                    && bar->height() > panel->height() * 0.8,
                    "inspector_scrollbar_not_at_viewport_edge");
                require(bar->isVisible() == (bar->property("size").toDouble() < 1.0), "inspector_scrollbar_visibility_wrong");
                require(controller_->selected().value("pump").toBool() && item("inspectorPumpState")->isVisible()
                    && item("inspectorPumpState")->property("text").toString() == (controller_->selected().value("available").toBool() ? "Pump available" : "Pump unavailable"),
                    "inspector_pump_state_missing");
                preserved(); passed("inspector_route_state_preserves_numerical_evidence");
                snapshot("10-route-inspector.png"); controller_->select("tank_a"); break;
            }
            case 38:
                require(item("equipmentLabel")->property("text").toString() == "Tank A"
                    && item("numericProperty_capacity")->isVisible(), "inspector_equipment_fields_missing");
                preserved(); snapshot("11-tank-inspector.png");
                require(eval("siteGround.gridStep >= 20 && siteGround.gridStep % 20 === 0 && siteGround.gridColumns <= 65 && siteGround.gridRows <= 65 && siteGround.gridCenterX % siteGround.gridStep === 0 && siteGround.gridCenterZ % siteGround.gridStep === 0").toBool(),
                    "layout_grid_not_bounded_or_world_aligned");
                click(item("sceneGrid")); break;
            case 39:
                require(!window_->property("siteGridVisible").toBool() && !item("sceneGrid")->property("checked").toBool(), "grid_did_not_hide");
                preserved(); click(item("sceneGrid")); break;
            case 40:
                require(window_->property("siteGridVisible").toBool() && item("sceneGrid")->property("checked").toBool(), "grid_did_not_restore");
                preserved(); passed("world_aligned_bounded_grid_toggle_preserves_numerical_evidence");
                snapshot("12-layout-grid.png"); controller_->loadExample("monitoring");
                eval("app.openTelemetry()"); break;
            case 41: {
                require(window_->property("telemetryVisible").toBool(),"monitoring_example_did_not_open");
                const auto sensors=controller_->sensorConfiguration().value("sensors").toArray();
                require(sensors.size()==10 && controller_->draftIssues().isEmpty(),"monitoring_example_coverage_invalid");
                auto* start=item("startLocalInput"); auto* capture=item("captureObservedState");
                require(start->width()>80 && capture->width()>80 && !capture->isEnabled(),"monitoring_controls_wrong");
                require(!item("saveTimedRecording")->isEnabled(),"manual_mode_enabled_timed_export");
                require(item("observedCaptureSummary")->property("text").toString().contains("Capture an immutable state"),"observed_capture_empty_state_missing");
                passed("instrumented_example_and_monitoring_controls_have_explicit_empty_state");
                snapshot("13-local-monitoring.png"); click(item("telemetryPanelClose")); break;
            }
            case 42:
                require(!window_->property("telemetryVisible").toBool(),"new_monitoring_close_failed");
                controller_->loadExample("healthy"); controller_->forecast(); break;
            case 43:
                if (controller_->busy()) return;
                require(controller_->canApprove(),"retained_pipe_source_forecast_missing");
                hash_=controller_->networkHash(); run_=controller_->runId(); revision_=controller_->simulationRevision();
                controller_->select("main_pump");
                eval("app.pipeTransport.setParameter('cells','4'); app.pipeTransport.setParameter('output_step_h','3'); app.pipeTransport.run()"); break;
            case 44:
                if (eval("app.pipeTransport.busy").toBool() || eval("app.pipeTransport.historyStatus.indexOf('Saving')>=0").toBool()) return;
                require(eval("app.pipeTransport.historyStatus.indexOf('saved')>=0").toBool(),"native_pipe_was_not_retained");
                pipe_evidence_id_=eval("app.pipeTransport.result.result_id").toString(); require(!pipe_evidence_id_.isEmpty(),"retained_pipe_id_missing");
                eval("app.history.loadSession(app.pipeTransport.result.result_id); app.openResults(3)"); break;
            case 45: {
                if (eval("app.history.busy").toBool()) return;
                require(eval("app.history.historical && app.history.retainedEvidence.length===1 && app.history.retainedEvidence[0].kind==='pipe_transport'").toBool(),"retained_pipe_history_did_not_reopen");
                auto* scroll=item("selectedEvidenceScroll"); auto* flick=scroll->property("contentItem").value<QQuickItem*>();
                require(flick,"retained_history_scroll_unavailable");
                auto* button=item("viewRetainedPipeHistory");
                const auto offset=flick->property("contentY").toDouble()+button->mapToItem(flick,QPointF{}).y()-scroll->height()*0.4;
                const auto maximum=std::max(0.0,flick->property("contentHeight").toDouble()-flick->height());
                flick->setProperty("contentY",std::clamp(offset,0.0,maximum)); break;
            }
            case 46: {
                auto* button=item("viewRetainedPipeHistory");
                auto* flick=item("selectedEvidenceScroll")->property("contentItem").value<QQuickItem*>();
                require(flick,"retained_history_scroll_unavailable");
                const auto local=button->mapToItem(flick,QPointF{});
                if (local.y()<0 || local.y()+button->height()>flick->height()) {
                    const auto maximum=std::max(0.0,flick->property("contentHeight").toDouble()-flick->height());
                    flick->setProperty("contentY",std::clamp(flick->property("contentY").toDouble()+local.y()-flick->height()*0.4,0.0,maximum));
                    retained_button_rect_={}; return;
                }
                const QRectF rect(button->mapToScene(QPointF{}),QSizeF(button->width(),button->height()));
                if (rect!=retained_button_rect_) { retained_button_rect_=rect; return; }
                observed_["retained_pipe_button"]=QJsonObject{{"x",rect.x()},{"y",rect.y()},{"width",rect.width()},{"height",rect.height()},
                    {"scroll_y",flick->property("contentY").toDouble()},{"viewport_height",flick->height()}};
                snapshot("14-retained-pipe-history.png"); retained_viewer_wait_.start(); click(button); break;
            }
            case 47:
                if (!window_->property("pipeTransportVisible").toBool() && retained_viewer_wait_.elapsed()<1000) return;
                observed_["retained_pipe_open"]=QJsonObject{{"visible",window_->property("pipeTransportVisible").toBool()},
                    {"imported",eval("app.pipeTransport.viewingImportedEvidence").toBool()},{"status",eval("app.pipeTransport.status").toString()}};
                require(window_->property("pipeTransportVisible").toBool() && eval("app.pipeTransport.viewingImportedEvidence").toBool(),"retained_pipe_action_did_not_open_viewer");
                require(eval("app.pipeTransport.result.result_id").toString()==pipe_evidence_id_ && !eval("app.investigator.canApprove").toBool(),"retained_pipe_identity_or_authority_changed");
                preserved(); snapshot("15-retained-pipe-viewer.png");
                passed("actual_pipe_execution_retention_reload_and_history_viewer_preserve_volume_controls");
                click(item("pipeTransportClose")); break;
            case 48:
                require(!window_->property("pipeTransportVisible").toBool(),"retained_pipe_viewer_did_not_close");
                eval("app.openResults(4)"); break;
            case 49:
                require(item("scienceSourceSelector")->isVisible(),"scientific_source_selector_missing");
                click(item("scienceSourceSelector")); key(Qt::Key_Down); key(Qt::Key_Return); break;
            case 50:
                require(eval("app.storageScience.sourceMode==='candidate' && !app.storageScience.canRun").toBool(),"empty_stored_source_borrowed_current_forecast");
                require(eval("!visible && storedRuns.length===0 && runChoices.length===0",item("runComparisonPanel")).toBool(),"hidden_comparison_materialized_run_controls");
                eval("app.investigator.searchLocally()"); break;
            case 51: {
                if (eval("app.investigator.running").toBool()) return;
                if (!stored_source_wait_.isValid()) stored_source_wait_.start();
                const auto state = QJsonObject::fromVariantMap(eval("({"
                    "recommendation:app.investigator.recommendation,investigator_status:app.investigator.status,"
                    "history_session:app.history.selectedSessionId,history_candidate:app.history.candidateRunId,"
                    "history_busy:app.history.busy,historical:app.history.historical,history_status:app.history.status,"
                    "source_mode:app.storageScience.sourceMode,source_busy:app.storageScience.busy,"
                    "can_run:app.storageScience.canRun,source_identity:app.storageScience.sourceIdentity,"
                    "source_status:app.storageScience.status,worker_busy:app.scientificWorker.busy})").toMap());
                auto diagnostics = state;
                diagnostics["wait_ms"] = stored_source_wait_.elapsed();
                observed_["stored_science_source"] = diagnostics;
                const auto recommendation = state.value("recommendation").toObject();
                const auto selected = recommendation.value("selected_run_id").toString();
                const auto session = recommendation.value("session_id").toString();
                require(!selected.isEmpty() && !session.isEmpty(), "stored_science_recommendation_missing");
                const bool historyAligned = state.value("history_session").toString()==session
                    && state.value("history_candidate").toString()==selected && !state.value("history_busy").toBool();
                const auto identity = state.value("source_identity").toObject();
                const bool ready = historyAligned && state.value("source_mode").toString()=="candidate"
                    && !state.value("source_busy").toBool() && state.value("can_run").toBool()
                    && identity.value("source_run_id").toString()==selected
                    && identity.value("source_session_id").toString()==session
                    && identity.value("source_snapshot_id")==recommendation.value("snapshot_id");
                // Investigation completion precedes checkpoint publication and asynchronous reconstruction.
                // A completed rejection must fail promptly; only pending convergence receives a bounded wait.
                const bool rejected = historyAligned && !state.value("source_busy").toBool()
                    && !state.value("worker_busy").toBool() && !state.value("can_run").toBool();
                if (!ready && !rejected && stored_source_wait_.elapsed()<15000) return;
                require(ready,"selected_stored_science_source_did_not_reconstruct");
                preserved(); snapshot("16-selected-candidate-science.png");
                click(item("scienceSourceSelector")); key(Qt::Key_Home); key(Qt::Key_Return); break;
            }
            case 52:
                require(eval("app.storageScience.sourceMode==='forecast'").toBool(),"scientific_source_selector_did_not_restore_forecast");
                preserved(); passed("native_source_selector_uses_immutable_stored_candidate_without_applying_controls");
                eval("app.openResults(1)"); markCueFrames(); phase_=280; return;
            case 280:
                if (!cueFramesReady()) return;
                require(eval("visible && storedRuns.length===history.runs.length && storedRuns.length>2",item("runComparisonPanel")).toBool(),"local_search_comparison_did_not_materialize_retained_runs");
                preserved(); snapshot("18-local-search-comparison.png");
                eval("app.openResults(4)"); markCueFrames(); phase_=281; return;
            case 281:
                if (!cueFramesReady()) return;
                require(eval("!visible && storedRuns.length===0 && runChoices.length===0",item("runComparisonPanel")).toBool(),"hidden_comparison_retained_run_controls");
                preserved(); passed("local_search_comparison_is_lazy_and_preserves_controls");
                finish(true,"ok"); return;
            case 53:
                if (eval("app.telemetry.status!=='ready'").toBool()) return;
                require(eval("app.telemetry.bundledPublisher").toBool(),"observed_ui_python_publisher_unavailable");
                eval("app.telemetry.startSynthetic('healthy',73)"); break;
            case 54:
                require(eval("app.telemetry.error.length===0").toBool(),"observed_ui_publisher_failed");
                if (eval("(app.telemetry.counters.readings_received || 0)<10").toBool()) return;
                eval("app.telemetry.pause()"); break;
            case 55:
                if (!eval("app.telemetry.paused").toBool()) return;
                eval("app.telemetry.requestObservedCapture()"); break;
            case 56: {
                if (eval("app.telemetry.observedCaptureBusy || !app.telemetry.observedCapture.snapshot_id").toBool()) return;
                require(eval("app.telemetry.observedCaptureDisposition==='available'").toBool(),"observed_ui_capture_unavailable");
                auto* scroll=item("telemetryScroll"); auto* flick=scroll->property("contentItem").value<QQuickItem*>();
                require(flick,"observed_ui_scroll_unavailable"); auto* button=item("openObservedInvestigation");
                const auto maximum=std::max(0.0,flick->property("contentHeight").toDouble()-flick->height());
                flick->setProperty("contentY",std::clamp(flick->property("contentY").toDouble()+button->mapToItem(flick,QPointF{}).y()-flick->height()*0.4,0.0,maximum));
                break;
            }
            case 57:
                snapshot("17-observed-capture.png"); click(item("openObservedInvestigation")); break;
            case 58:
                if (!window_->property("observedInvestigationVisible").toBool()) return;
                require(item("observedInvestigationOrigin")->isVisible() && !eval("app.sceneKeysEnabled()").toBool(),"observed_dialog_origin_or_shortcut_gate_missing");
                click(item("observedLocalSearch")); break;
            case 59:
                if (eval("app.observedInvestigator.running || !app.observedInvestigator.canAcknowledge").toBool()) return;
                require(controller_->editorDocument()==observed_project_ && controller_->runId().isEmpty() && !controller_->canApprove(),"observed_search_modified_project");
                snapshot("18-observed-native-response.png"); click(item("reviewObservedAdvisory")); break;
            case 60:
                if (!window_->property("observedAdvisoryReviewVisible").toBool()) return;
                require(item("acknowledgeObservedAdvisory")->isEnabled(),"observed_review_acknowledgement_unavailable");
                require(eval("app.observedInvestigator.review.measurements.length>=10 && app.observedInvestigator.review.validation_id.length>0").toBool(),"observed_review_evidence_missing");
                snapshot("19-observed-advisory-review.png"); click(item("acknowledgeObservedAdvisory")); break;
            case 61: {
                auto* observedHistory=qobject_cast<history::HistoryController*>(window_->property("observedHistory").value<QObject*>());
                require(observedHistory,"observed_history_unavailable");
                if (window_->property("observedAdvisoryReviewVisible").toBool() || observedHistory->selectedArchive().value("approvals").toArray().size()!=1) return;
                require(!eval("app.observedInvestigator.canAcknowledge || app.observedInvestigator.canApprove").toBool() &&
                    controller_->editorDocument()==observed_project_ && controller_->runId().isEmpty() && !controller_->canApprove(),"observed_review_restored_operating_authority");
                eval("app.telemetry.requestObservedCapture()"); break;
            }
            case 62:
                if (eval("app.telemetry.observedCaptureBusy || !app.observedInvestigator.nextSourceDiffers").toBool()) return;
                require(eval("app.observedInvestigator.sourceSummary.status==='retired' && app.observedInvestigator.nextSourceSummary.status==='available'").toBool(),
                    "observed_next_capture_mixed_with_displayed_evidence");
                require(item("observedInvestigationNextSource")->isVisible() &&
                    item("observedInvestigationNextSource")->property("text").toString().contains(eval("app.observedInvestigator.nextSourceSummary.snapshot_id").toString()),
                    "observed_next_capture_preview_missing");
                require(!eval("app.observedInvestigator.canAcknowledge || app.observedInvestigator.canApprove").toBool(),"new_capture_restored_old_advisory_authority");
                snapshot("19b-next-capture-preview.png"); click(item("observedInvestigationClose"));
                eval("app.telemetry.stop()"); break;
            case 63:
                if (eval("app.telemetry.active").toBool()) return;
                require(!window_->property("observedInvestigationVisible").toBool(),"observed_dialog_close_failed");
                require(eval("app.telemetry.error.length===0").toBool(),"observed_ui_source_shutdown_failed");
                passed("actual_python_capture_native_search_advisory_review_and_retention_without_control_application");
                finish(true,"ok"); return;
            case 64:
                if (controller_->busy()) return;
                require(!controller_->runId().isEmpty() && controller_->forecastHorizonH()==3,"onboarding_did_not_run_native_forecast");
                observed_project_=controller_->editorDocument(); hash_=controller_->networkHash(); run_=controller_->runId(); revision_=controller_->simulationRevision();
                eval("workspaceSettings.setValue('dockHeight',160)"); window_->resize(960,540);
                markCueFrames(); phase_=303; return;
            case 65:
                if (!eval("sceneHelpDialog.visible").toBool()) return;
                require(!eval("app.sceneKeysEnabled()").toBool(),"help_did_not_block_edit_shortcuts");
                snapshot("20-workspace-keyboard-help.png"); key(Qt::Key_D,Qt::ControlModifier); break;
            case 66:
                require(controller_->editorDocument()==observed_project_,"help_allowed_a_project_edit"); preserved();
                key(Qt::Key_Escape); break;
            case 67:
                require(!eval("sceneHelpDialog.visible").toBool(),"help_escape_close_failed");
                key(Qt::Key_0,Qt::ControlModifier); break;
            case 68:
                require(eval("app.fittedView").toBool(),"fit_keyboard_shortcut_failed"); preserved();
                key(Qt::Key_Home,Qt::ControlModifier); break;
            case 69: {
                require(!eval("app.fittedView").toBool() && controller_->editorDocument()==observed_project_,"home_shortcut_changed_project"); preserved();
                passed("empty_native_forecast_keyboard_help_and_view_shortcuts_preserve_project");
                navigation_editor_revision_=controller_->editorRevision(); navigation_selection_=controller_->selectedId();
                navigation_all_labels_=eval("workspaceSettings.values.allLabels").toBool();
                navigation_dock_open_=eval("workspaceSettings.values.dockOpen").toBool();
                eval("workspaceSettings.setValue('allLabels',false)");
                if (navigation_dock_open_) click(item("Toggle results",true));
                phase_=70; return;
            }
            case 70:
                if (window_->property("compactLayout").toBool() && !window_->property("navigatorVisible").toBool()) {
                    click(item("Navigator",true)); return;
                }
                navigationPreserved();
                require(item("networkEquipmentMode")->isVisible() && item("networkConnectionsMode")->isVisible(),
                    "network_mode_controls_missing");
                {
                    const auto bounds=[&](QQuickItem* control) {
                        return QRectF(control->mapToScene(QPointF(0,0)),QSizeF(control->width(),control->height()));
                    };
                    const auto equipment=bounds(item("networkEquipmentMode")), connections=bounds(item("networkConnectionsMode"));
                    const QRectF viewport(0,0,window_->width(),window_->height());
                    require(viewport.contains(equipment) && viewport.contains(connections) && !equipment.intersects(connections)
                        && equipment.width()>=20 && connections.width()>=20, "inline_network_modes_clipped_or_overlapping");
                }
                // Keep a real asset selected while filtering connections, so a
                // leaked Ctrl+D would produce a detectable model edit.
                click(item("networkAsset:tank_a"));
                click(item("networkConnectionsMode")); break;
            case 71:
                require(eval("app.networkMode==='connections' && app.filteredNetworkRoutes.length===6").toBool()
                    && item("networkConnectionsMode")->property("checked").toBool()
                    && !item("networkEquipmentMode")->property("checked").toBool(), "connections_mode_or_count_wrong");
                require(controller_->selectedId()=="tank_a", "connection_mode_changed_selection");
                navigationPreserved(); snapshot("21-connections-navigator.png"); filterNetwork("BACKUP_PUMP"); break;
            case 72:
                require(eval("JSON.stringify(app.filteredNetworkRoutes.map(row=>row.id))").toString()=="[\"backup_pump\"]",
                    "route_id_filter_case_or_match_wrong");
                require(eval("(function(){const r=app.filteredNetworkRoutes[0];return r.sourceLabel==='Tank B' && r.targetLabel==='Treatment'"
                    " && r.flow===16 && !r.sourceDriven && r.settingText.indexOf('setpoint')>=0 && r.settingText.toLowerCase().indexOf('measured')<0"
                    " && r.statusText==='Pump available'})()").toBool(), "configured_route_metadata_wrong");
                key(Qt::Key_D,Qt::ControlModifier); navigationPreserved();
                require(controller_->selectedId()=="tank_a", "route_filter_changed_selection");
                click(item("networkRoute:backup_pump")); break;
            case 73:
                require(controller_->selectedId()=="backup_pump" && controller_->selectedIds().isEmpty()
                    && controller_->selected().value("kind").toString()=="route", "route_list_did_not_select_exact_route");
                require(eval("JSON.stringify(app.routeEndpointIds)").toString()=="[\"tank_b\",\"treatment\"]",
                    "route_context_endpoint_ids_wrong");
                if (window_->property("compactLayout").toBool() && !window_->property("navigatorVisible").toBool()) {
                    click(item("Navigator",true)); return;
                }
                navigationPreserved(); click(item("clearNetworkFilter")); break;
            case 74:
                require(item("networkFilter")->property("text").toString().isEmpty()
                    && eval("app.filteredNetworkRoutes.length").toInt()==6 && controller_->selectedId()=="backup_pump",
                    "route_filter_clear_changed_selection_or_count");
                navigationPreserved(); route_label_wait_.start(); click(item("sceneFocus")); break;
            case 75:
                routeFocused();
                if (!eval("sceneLabelLayer.positioned.some(row=>row.id==='tank_b') && sceneLabelLayer.positioned.some(row=>row.id==='treatment')").toBool()) {
                    require(route_label_wait_.elapsed()<2000,"selected_route_context_labels_missing"); return;
                }
                require(!eval("workspaceSettings.values.allLabels").toBool(),"route_context_enabled_all_labels");
                passed("route_list_selection_focus_and_related_labels_preserve_project");
                click(item("sceneHome")); break;
            case 76:
                require(!eval("app.fittedView").toBool(),"route_test_home_did_not_restore_view");
                navigationPreserved(); key(Qt::Key_F,Qt::NoModifier,"f"); break;
            case 77:
                routeFocused();
                if (window_->property("compactLayout").toBool() && !window_->property("inspectorVisible").toBool()) {
                    click(item("Inspector",true)); return;
                }
                require(item("inspectorFocus")->isVisible() && item("inspectorFocus")->isEnabled(),"route_inspector_focus_unavailable");
                click(item("sceneHome")); break;
            case 78:
                require(!eval("app.fittedView").toBool(),"inspector_route_test_home_did_not_restore_view");
                navigationPreserved(); click(item("inspectorFocus")); break;
            case 79:
                routeFocused();
                require(item("inspectorPumpState")->property("text").toString()=="Pump available", "selected_route_pump_state_missing");
                passed("route_keyboard_and_inspector_focus_preserve_numerical_evidence"); snapshot("22-route-focused-inspector.png"); break;
            case 80:
                if (window_->property("compactLayout").toBool() && !window_->property("navigatorVisible").toBool()) {
                    click(item("Navigator",true)); return;
                }
                navigationPreserved(); filterNetwork("pRoDuCtIoN a"); break;
            case 81:
                require(eval("JSON.stringify(app.filteredNetworkRoutes.map(row=>row.id))").toString()=="[\"feed_a\"]",
                    "route_source_label_filter_wrong");
                require(eval("(function(){const r=app.filteredNetworkRoutes[0];return r.sourceDriven && r.flow===52"
                    " && r.sourceLabel==='Production A' && r.targetLabel==='Tank A' && r.settingText.indexOf('source setting')>=0"
                    " && r.settingText.toLowerCase().indexOf('measured')<0 && r.statusText==='Source-driven'})()").toBool(),
                    "source_route_setting_presented_as_measured_flow");
                require(item("networkRouteSetting:feed_a")->property("text").toString().contains("source setting")
                    && item("networkRouteState:feed_a")->property("text").toString()=="Source-driven", "source_route_row_setting_or_status_missing");
                navigationPreserved(); click(item("networkRoute:feed_a")); break;
            case 82:
                require(controller_->selectedId()=="feed_a" && controller_->selectedIds().isEmpty()
                    && eval("JSON.stringify(app.routeEndpointIds)").toString()=="[\"source_a\",\"tank_a\"]",
                    "source_route_selection_or_context_wrong");
                navigationPreserved(); filterNetwork("TREATMENT"); break;
            case 83:
                require(eval("JSON.stringify(app.filteredNetworkRoutes.map(row=>row.id))").toString()=="[\"main_pump\",\"backup_pump\"]",
                    "route_target_label_filter_wrong");
                require(controller_->selectedId()=="feed_a", "endpoint_filter_changed_hidden_route_selection");
                navigationPreserved(); filterNetwork("no-such-route"); break;
            case 84:
                require(eval("app.filteredNetworkRoutes.length").toInt()==0 && controller_->selectedId()=="feed_a",
                    "empty_route_filter_changed_selection_or_has_rows");
                require(item("clearNetworkFilter")->isVisible(),"empty_route_filter_has_no_clear_action");
                navigationPreserved(); click(item("clearNetworkFilter")); break;
            case 85:
                require(eval("app.filteredNetworkRoutes.length").toInt()==6 && controller_->selectedId()=="feed_a",
                    "cleared_route_filter_did_not_restore_rows");
                navigationPreserved(); click(item("networkEquipmentMode")); break;
            case 86:
                require(eval("app.networkMode==='equipment' && app.filteredNetworkAssets.length===6").toBool()
                    && item("networkEquipmentMode")->property("checked").toBool()
                    && !item("networkConnectionsMode")->property("checked").toBool()
                    && controller_->selectedId()=="feed_a", "equipment_mode_reentry_changed_route_selection");
                navigationPreserved();
                require(item("networkAsset:tank_a")->isVisible(),"equipment_rows_not_restored");
                passed("connection_filters_id_endpoints_source_settings_and_modes_preserve_selection");
                controller_->select(navigation_selection_);
                eval(navigation_all_labels_ ? "workspaceSettings.setValue('allLabels',true)":"workspaceSettings.setValue('allLabels',false)");
                click(item("sceneHome")); break;
            case 87: {
                require(controller_->selectedId()==navigation_selection_ && item("networkFilter")->property("text").toString().isEmpty()
                    && eval("app.networkMode==='equipment' && !app.fittedView && app.routeEndpointIds.length===0").toBool(),
                    "route_navigation_test_did_not_restore_editor_view");
                navigationPreserved();
                if (eval("workspaceSettings.values.dockOpen").toBool()!=navigation_dock_open_) click(item("Toggle results",true));
                break;
            }
            case 88: {
                require(eval("workspaceSettings.values.dockOpen").toBool()==navigation_dock_open_,"route_navigation_did_not_restore_results_dock");
                navigationPreserved();
                cue_original_project_=controller_->editorDocument();
                if (navigation_dock_open_) click(item("Toggle results",true));
                controller_->loadExample("baseline");
                SimulationConfig config; config.horizon_h=0.5; controller_->forecastWithConfig(config); break;
            }
            case 89:
                if (controller_->busy()) return;
                require(controller_->canApprove() && controller_->forecastHorizonH()==0.5,"route_cue_real_fixture_forecast_unavailable");
                if (!cue_fixture_captured_) {
                    observed_project_=controller_->editorDocument(); hash_=controller_->networkHash(); run_=controller_->runId();
                    revision_=controller_->simulationRevision(); navigation_editor_revision_=controller_->editorRevision(); cue_fixture_captured_=true;
                }
                navigationPreserved();
                require(eval("app.sceneRoutes.some(r=>r.id==='main_pump'&&r.flow===0&&!r.available) && app.sceneRoutes.some(r=>r.id==='bypass'&&r.flow===0&&r.available)").toBool(),
                    "route_cue_real_fixture_does_not_cover_zero_and_unavailable");
                if (window_->property("compactLayout").toBool() && !window_->property("navigatorVisible").toBool()) {
                    click(item("Navigator",true)); return;
                }
                click(item("networkConnectionsMode")); break;
            case 90:
                navigationPreserved(); filterNetwork("BYPASS"); break;
            case 91:
                require(eval("app.filteredNetworkRoutes.length===1 && app.filteredNetworkRoutes[0].id==='bypass'").toBool(),"zero_setting_route_filter_wrong");
                navigationPreserved(); click(item("networkRoute:bypass")); break;
            case 92:
                navigationPreserved(); click(item("sceneFocus")); markCueFrames(); break;
            case 93:
                if (!cueFramesReady()) return;
                routeFocused(); routeCueSummary("bypass","Configured route",0);
                cue_static_=selectedCue().value("geometry").toObject(); snapshot("23-zero-setting-route-cue.png");
                click(item("sceneHome")); markCueFrames(); break;
            case 94:
                if (!cueFramesReady()) return;
                require(!eval("app.fittedView").toBool() && selectedCue().value("geometry").toObject()==cue_static_,"route_direction_cue_moves_with_view_or_animates");
                navigationPreserved(); key(Qt::Key_F,Qt::NoModifier,"f"); markCueFrames(); break;
            case 95:
                if (!cueFramesReady()) return;
                routeFocused(); routeCueSummary("bypass","Configured route",0);
                require(selectedCue().value("geometry").toObject()==cue_static_,"route_direction_cue_changed_after_focus");
                navigationPreserved(); filterNetwork("MAIN_PUMP"); break;
            case 96:
                require(controller_->selectedId()=="bypass" && eval("app.filteredNetworkRoutes.length===1 && app.filteredNetworkRoutes[0].id==='main_pump'").toBool(),
                    "route_cue_filter_changed_selection");
                navigationPreserved(); click(item("networkRoute:main_pump")); break;
            case 97:
                navigationPreserved(); click(item("sceneFocus")); markCueFrames(); break;
            case 98:
                if (!cueFramesReady()) return;
                routeFocused(); routeCueSummary("main_pump","Pump unavailable",0);
                require(selectedCue().value("pipe_red").toBool(),"unavailable_route_selection_lost_availability_colour");
                snapshot("24-unavailable-route-cue.png");
                passed("zero_setting_and_unavailable_route_static_cues_status_bounds_and_picking_preserve_controls");
                click(item("sceneTop")); markCueFrames(); phase_=100; return;
            case 100:
                if (!cueFramesReady()) return;
                require(eval("app.topView").toBool(),"projection_test_top_view_did_not_activate");
                labelsMatchProjection("top");
                click(item("sceneFocus")); markCueFrames(); break;
            case 101:
                if (!cueFramesReady()) return;
                require(eval("app.topView").toBool(),"route_focus_left_top_view");
                routeFocused(); labelsMatchProjection("top_focus");
                click(item("sceneTop")); markCueFrames(); break;
            case 102:
                if (!cueFramesReady()) return;
                require(!eval("app.topView").toBool() && !eval("app.fittedView").toBool(),"projection_test_perspective_did_not_restore");
                labelsMatchProjection("perspective");
                click(item("sceneHome")); markCueFrames(); break;
            case 103:
                if (!cueFramesReady()) return;
                require(!eval("app.topView").toBool() && !eval("app.fittedView").toBool(),"projection_test_home_view_did_not_restore");
                labelsMatchProjection("home");
                passed("top_focus_perspective_and_home_label_anchors_follow_rendered_projection_without_numerical_edits");
                planLayoutFixture(false);
                eval("networkFilter.clear(); app.networkMode='equipment'");
                click(item("sceneTop")); click(item("sceneFit")); markCueFrames(); break;
            case 104:
                if (!cueFramesReady()) return;
                plan_before_dock_=planFootprints("tall");
                require(!eval("app.frameAdjusted").toBool(),"plan_fit_starts_adjusted");
                click(item("Toggle results",true)); markCueFrames(); break;
            case 105:
                if (!cueFramesReady()) return;
                {
                    const auto resized=planFootprints("tall_after_dock_resize");
                    require(resized.value("view_height")!=plan_before_dock_.value("view_height"),"plan_fit_dock_did_not_resize_viewport");
                    require(!eval("app.frameAdjusted").toBool(),"plan_fit_resize_requires_manual_refit");
                }
                planLayoutFixture(true); click(item("sceneTop")); click(item("sceneFit")); markCueFrames(); break;
            case 106:
                if (!cueFramesReady()) return;
                planFootprints("wide");
                // The cramped viewport above proves body framing. Give the separate
                // label-pan check real label space instead of testing allowed culling.
                if (eval("workspaceSettings.values.dockOpen").toBool()) click(item("Toggle results",true));
                markCueFrames(); phase_=113; return;
            case 113:
                if (!cueFramesReady()) return;
                navigationPreserved(); controller_->select("bypass"); click(item("sceneFocus"));
                markCueFrames(); phase_=107; return;
            case 107:
                if (!cueFramesReady()) return;
                planFootprints("route_endpoints",{"tank_a","tank_b"});
                require(eval("app.planZoom===1 && app.planPanX===0 && app.planPanZ===0 && !app.frameAdjusted").toBool(),"plan_focus_did_not_reset_manual_adjustment");
                plan_before_wheel_=eval("planCamera.horizontalMagnification").toDouble();
                planWheel(); markCueFrames(); break;
            case 108:
                if (!cueFramesReady()) return;
                navigationPreserved();
                require(eval("app.frameAdjusted && Number.isFinite(app.planZoom) && app.planZoom!==1").toBool()
                    && std::abs(eval("planCamera.horizontalMagnification").toDouble()-plan_before_wheel_)>1e-6,"plan_view_native_wheel_did_not_adjust_zoom");
                plan_pan_x_=eval("app.planPanX").toDouble(); plan_pan_z_=eval("app.planPanZ").toDouble();
                planPan(); markCueFrames(); phase_=112; return;
            case 112:
                if (!cueFramesReady()) return;
                navigationPreserved();
                require(eval("app.topView && app.frameAdjusted").toBool()
                    && std::abs(eval("app.planPanX").toDouble()-plan_pan_x_)>1e-6
                    && std::abs(eval("app.planPanZ").toDouble()-plan_pan_z_)>1e-6,"plan_view_native_middle_drag_did_not_pan");
                labelsMatchProjection("top_pan");
                scene_focus_preferences_=panelPreferences();
                eval("scenePanel.forceActiveFocus()");
                key(Qt::Key_F,Qt::ControlModifier|Qt::ShiftModifier,"F"); markCueFrames(); phase_=109; return;
            case 109:
                if (!cueFramesReady()) return;
                navigationPreserved();
                require(eval("app.sceneOnly && !app.navigatorVisible && !app.inspectorVisible && !workbenchDock.visible").toBool(),"scene_focus_did_not_hide_workspace_chrome");
                require(panelPreferences()==scene_focus_preferences_,"scene_focus_changed_persisted_panel_preferences");
                key(Qt::Key_F,Qt::ControlModifier|Qt::ShiftModifier,"F"); markCueFrames(); break;
            case 110:
                if (!cueFramesReady()) return;
                navigationPreserved();
                require(!eval("app.sceneOnly").toBool() && panelPreferences()==scene_focus_preferences_,"scene_focus_toggle_did_not_restore_preferences");
                if (window_->property("compactLayout").toBool() && !window_->property("navigatorVisible").toBool()) {
                    click(item("Navigator",true)); return;
                }
                click(item("networkFilter")); require(item("networkFilter")->hasActiveFocus(),"scene_focus_text_guard_fixture_has_no_focus");
                key(Qt::Key_F,Qt::ControlModifier|Qt::ShiftModifier,"F");
                require(!eval("app.sceneOnly").toBool(),"scene_focus_shortcut_escaped_text_editor_guard");
                eval("scenePanel.forceActiveFocus()");
                key(Qt::Key_F,Qt::ControlModifier|Qt::ShiftModifier,"F");
                require(eval("app.sceneOnly").toBool(),"scene_focus_escape_fixture_did_not_enter");
                key(Qt::Key_Escape);
                require(!eval("app.sceneOnly").toBool() && panelPreferences()==scene_focus_preferences_,"scene_focus_escape_did_not_restore_mode_and_preferences");
                click(item("sceneHome")); markCueFrames(); break;
            case 111:
                if (!cueFramesReady()) return;
                navigationPreserved();
                require(!eval("app.topView || app.fittedView || app.frameAdjusted").toBool(),"plan_view_home_did_not_restore_saved_orbit");
                require(item("workspaceViewStatus")->isVisible() && !item("workspaceViewStatus")->property("text").toString().trimmed().isEmpty(),"workspace_view_status_missing");
                passed("actual_tall_wide_plan_framing_resize_route_focus_zoom_and_scene_focus_preserve_evidence_and_preferences");
                require(controller_->restore(cue_original_project_),"route_cue_original_setup_restore_failed");
                observed_project_=cue_original_project_;
                eval("networkFilter.clear(); app.networkMode='equipment'"); controller_->select(navigation_selection_);
                if (eval("workspaceSettings.values.dockOpen").toBool()!=navigation_dock_open_) click(item("Toggle results",true));
                click(item("sceneHome")); phase_=99; return;
            case 99: {
                require(controller_->editorDocument()==cue_original_project_ && controller_->selectedId()==navigation_selection_
                    && !item("sceneRouteSummary")->isVisible() && !eval("app.fittedView").toBool()
                    && eval("app.networkMode==='equipment'").toBool()
                    && eval("workspaceSettings.values.dockOpen").toBool()==navigation_dock_open_,
                    "route_cue_test_did_not_restore_original_editor_setup");
                SimulationConfig config; config.horizon_h=1.25; controller_->forecastWithConfig(config); phase_=1; return;
            }
            default: throw std::runtime_error("invalid_phase");
            }
            ++phase_;
        } catch (const std::exception& error) { finish(false, QString::fromLatin1(error.what())); }
    }
public:
    Check(QQuickWindow* window, ProjectController* controller, QCoreApplication* app, std::function<int()> modelRequests)
        : QObject(app), window_(window), controller_(controller), app_(app), model_requests_(std::move(modelRequests)) {
        observed_only_=qEnvironmentVariable("BREAKWATER_UI_OBSERVED_ONLY")=="1";
        compare_settings_only_=qEnvironmentVariable("BREAKWATER_UI_COMPARE_SETTINGS_ONLY")=="1";
        if (qEnvironmentVariable("BREAKWATER_UI_MINIMUM") == "1") window->resize(960, 540);
        else if (qEnvironmentVariable("BREAKWATER_UI_SHORT") == "1") window->resize(1008, 672);
        elapsed_.start(); timer_.setInterval(100);
        connect(controller, &ProjectController::changed, this, [this] { ++changes_; });
        connect(window,&QQuickWindow::frameSwapped,this,[this] { ++rendered_frames_; });
        connect(&timer_, &QTimer::timeout, this, [this] { tick(); }); timer_.start();
    }
};
}
void startWorkbenchUiCheck(QQuickWindow* window, ProjectController* controller, QCoreApplication* app, std::function<int()> modelRequests) {
    new Check(window, controller, app,std::move(modelRequests));
}
}
