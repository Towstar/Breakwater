#include "verification/WorkbenchUiCheck.hpp"
#include "ProjectController.hpp"
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPointer>
#include <QQuickItem>
#include <QQuickWindow>
#include <QTextStream>
#include <QTimer>
#include <stdexcept>

namespace breakwater::desktop {
namespace {
class PanelCheck final : public QObject {
    QPointer<QQuickWindow> window_; QPointer<ProjectController> controller_;
    QCoreApplication* app_; QTimer timer_; QElapsedTimer elapsed_, phaseTime_;
    int phase_ = 0, index_ = 0; QJsonArray checks_, observed_;
    QString hash_; QJsonObject network_; quint64 revision_ = 0;
    const QStringList buttons_{"toggleInvestigator", "openSensorEditor", "openTelemetry", "openPipeEvidence"};
    const QStringList visible_{"showAgent", "sensorEditorVisible", "telemetryVisible", "pipeTransportVisible"};
    const QStringList close_{"", "sensorPanelClose", "telemetryPanelClose", "pipeTransportClose"};
    void require(bool value, const char* code) { if (!value) throw std::runtime_error(code); }
    QQuickItem* find(QQuickItem* root, const QString& name) {
        if (!root) return nullptr;
        if (root->objectName() == name) return root;
        for (auto* child : root->childItems()) if (auto* found = find(child, name)) return found;
        return nullptr;
    }
    QQuickItem* item(const QString& name) { auto* found = find(window_->contentItem(), name); require(found, "item_missing"); return found; }
    void click(QQuickItem* control) {
        require(control->isVisible() && control->isEnabled() && control->width() > 0 && control->height() > 0, "click_target_unavailable");
        const auto p = control->mapToScene(QPointF(control->width()/2, control->height()/2));
        require(p.x() >= 0 && p.y() >= 0 && p.x() < window_->width() && p.y() < window_->height(), "click_target_outside_window");
        QMouseEvent press(QEvent::MouseButtonPress, p, p, p, Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
        QMouseEvent release(QEvent::MouseButtonRelease, p, p, p, Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
        QCoreApplication::sendEvent(window_, &press); QCoreApplication::sendEvent(window_, &release);
        phaseTime_.restart();
    }
    void state(int active) {
        for (int i=0; i<buttons_.size(); ++i) {
            require(window_->property(visible_[i].toLatin1().constData()).toBool() == (i==active), "panel_visibility_not_exclusive");
            require(item(buttons_[i])->property("checked").toBool() == (i==active), "checked_state_not_actual_visibility");
        }
        require(controller_->networkHash() == hash_ && controller_->simulationRevision() == revision_ && controller_->document().value("network").toObject() == network_, "panel_navigation_changed_controls");
    }
    void geometry(int index) {
        auto* control = item(close_[index]); QQuickItem* popup = nullptr;
        for(auto* p=control; p; p=p->parentItem()) if (QString::fromLatin1(p->metaObject()->className()).contains("PopupItem")) {popup=p;break;}
        require(popup, "popup_item_missing"); const auto pos=popup->mapToScene(QPointF(0,0));
        require(pos.x()>=220 && pos.y()>=76 && pos.x()+popup->width()<=window_->width() && pos.y()+popup->height()<=window_->height(), "popup_covers_sidebar_or_exceeds_window");
        const auto buttonPoint=item(buttons_[index])->mapToScene(QPointF(item(buttons_[index])->width()/2,item(buttons_[index])->height()/2));
        require(buttonPoint.x()<pos.x(), "toggle_not_exposed");
        const QStringList titles{"", "Sensor editor", "Synthetic monitoring and replay", "Experimental pipe transport"};
        QQuickItem* title = nullptr;
        QList<QQuickItem*> pending{popup};
        while(!pending.empty()) { auto* candidate=pending.takeLast(); if(candidate->property("text").toString()==titles[index]) { title=candidate;break; } for(auto* child:candidate->childItems())pending.append(child); }
        require(title && title->isVisible(),"popup_title_missing");
        const auto titlePoint=title->mapToItem(popup,QPointF(0,0));
        require(titlePoint.x()>=14 && titlePoint.y()>=0 && titlePoint.y()+title->height()<=54,"popup_title_inset_or_header_invalid");
        observed_.append(QJsonObject{{"panel",buttons_[index]},{"x",pos.x()},{"y",pos.y()},{"width",popup->width()},{"height",popup->height()},{"toggle_x",buttonPoint.x()},{"title_inset_x",titlePoint.x()},{"title_y",titlePoint.y()}});
    }
    void snapshot(const QString& name) {
        const auto folder=qEnvironmentVariable("BREAKWATER_PANEL_EVIDENCE_DIR"); require(!folder.isEmpty(),"evidence_path_missing");
        const auto img=window_->grabWindow(); require(!img.isNull() && img.save(folder+'/'+name,"PNG"),"snapshot_failed");
    }
    void finish(bool ok,const QString& code) {
        timer_.stop(); QTextStream out(stdout); out<<QJsonDocument(QJsonObject{{"check","actual_sidebar_panel_navigation"},{"passed",ok},{"code",code},{"phase",phase_},{"panel_index",index_},{"checks",checks_},{"observed",observed_},{"elapsed_ms",elapsed_.elapsed()},{"window_width",window_->width()},{"window_height",window_->height()},{"device_pixel_ratio",window_->devicePixelRatio()},{"hosted_requests","not_requested"},{"scientific_operations","not_requested"}}).toJson(QJsonDocument::Compact)<<'\n';out.flush();app_->exit(ok?0:8);
    }
    void tick() {
        try {
            require(window_&&controller_,"destroyed");require(elapsed_.elapsed()<24000,"overall_timeout");
            switch(phase_) {
            case 0:
                if(!window_->isExposed()||!window_->property("sceneReady").toBool())return;
                if(!window_->isActive()){window_->requestActivate();return;}
                hash_=controller_->networkHash(); revision_=controller_->simulationRevision();network_=controller_->document().value("network").toObject();
                state(-1); index_=0; click(item(buttons_[index_])); phase_=1;return;
            case 1:
                state(index_); if(index_>0)geometry(index_);checks_.append(buttons_[index_]+"_actual_click_opens_checked");
                click(item(buttons_[index_]));phase_=2;return;
            case 2:
                state(-1); checks_.append(buttons_[index_]+"_same_underlying_button_closes");
                if(++index_<buttons_.size()){click(item(buttons_[index_]));phase_=1;return;}
                index_=1;click(item(buttons_[index_]));phase_=3;return;
            case 3:
                state(index_);geometry(index_);if(phaseTime_.elapsed()<350)return;
                snapshot(QString::number(index_)+"-panel-open.png");click(item(close_[index_]));phase_=4;return;
            case 4:
                state(-1);checks_.append(close_[index_]+"_actual_click_closes_and_clears_checked");
                if(++index_<buttons_.size()){click(item(buttons_[index_]));phase_=3;return;}
                index_=0;click(item(buttons_[index_]));phase_=5;return;
            case 5:
                state(index_);checks_.append(buttons_[index_]+"_exclusive_navigation");
                if(++index_<buttons_.size()){click(item(buttons_[index_]));return;}
                click(item(close_[3]));phase_=6;return;
            case 6:
                state(-1);checks_.append("all_panel_navigation_preserves_network_and_controls");finish(true,"ok");return;
            }
        }catch(const std::exception& e){finish(false,QString::fromLatin1(e.what()));}
    }
public:
    PanelCheck(QQuickWindow* window,ProjectController* controller,QCoreApplication* app):QObject(app),window_(window),controller_(controller),app_(app){elapsed_.start();timer_.setInterval(100);connect(&timer_,&QTimer::timeout,this,[this]{tick();});timer_.start();}
};
}
void startWorkbenchUiCheck(QQuickWindow* window,ProjectController* controller,QCoreApplication* app){new PanelCheck(window,controller,app);}
}

