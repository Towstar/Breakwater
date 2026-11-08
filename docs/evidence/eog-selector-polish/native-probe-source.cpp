#include "verification/WorkbenchUiCheck.hpp"
#include "ProjectController.hpp"
#include "eog/EogDatasetController.hpp"
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
#include <algorithm>
#include <stdexcept>
namespace breakwater::desktop { namespace {
class SelectorCheck final:public QObject {
 QPointer<QQuickWindow> window_;QPointer<ProjectController> controller_;QPointer<eog::EogDatasetController> eog_;QCoreApplication*app_;QTimer timer_;QElapsedTimer elapsed_,phaseTime_;int phase_=0,index_=0,expected_=0;QJsonArray checks_,observed_;QString identity_,hash_;quint64 revision_=0;QJsonObject network_;QList<QQuickItem*>choices_;
 void require(bool value,const char*code){if(!value)throw std::runtime_error(code);}
 QQuickItem* find(QQuickItem*root,const QString&name){if(root->objectName()==name)return root;for(auto*c:root->childItems())if(auto*f=find(c,name))return f;return nullptr;}
 void gather(QQuickItem*root){if(root->metaObject()->indexOfProperty("displayText")>=0&&root->metaObject()->indexOfProperty("delegateModel")>=0)choices_.append(root);for(auto*c:root->childItems())gather(c);}
 void click(QQuickItem*item){const auto p=item->mapToScene(QPointF(item->width()/2,item->height()/2));require(item->isVisible()&&p.x()>0&&p.y()>0&&p.x()<window_->width()&&p.y()<window_->height(),"selector_not_clickable");QMouseEvent down(QEvent::MouseButtonPress,p,p,p,Qt::LeftButton,Qt::LeftButton,Qt::NoModifier),up(QEvent::MouseButtonRelease,p,p,p,Qt::LeftButton,Qt::NoButton,Qt::NoModifier);QCoreApplication::sendEvent(window_,&down);QCoreApplication::sendEvent(window_,&up);phaseTime_.restart();}
 void key(int value){QKeyEvent down(QEvent::KeyPress,value,Qt::NoModifier),up(QEvent::KeyRelease,value,Qt::NoModifier);QCoreApplication::sendEvent(window_,&down);QCoreApplication::sendEvent(window_,&up);}
 QObject*popup(){return choices_[index_]->property("popup").value<QObject*>();}
 void same(){require(eog_->datasetIdentity()==identity_&&controller_->networkHash()==hash_&&controller_->simulationRevision()==revision_&&controller_->document().value("network").toObject()==network_,"selection_changed_source_or_workbench_controls");}
 void finish(bool ok,const QString&code){timer_.stop();QTextStream out(stdout);out<<QJsonDocument(QJsonObject{{"check","eog_selectors_actual_native_events"},{"passed",ok},{"code",code},{"phase",phase_},{"selector_index",index_},{"checks",checks_},{"observed",observed_},{"elapsed_ms",elapsed_.elapsed()},{"window_width",window_->width()},{"window_height",window_->height()},{"device_pixel_ratio",window_->devicePixelRatio()},{"hosted_requests","not_requested"},{"scientific_operations","not_requested"}}).toJson(QJsonDocument::Compact)<<'\n';out.flush();app_->exit(ok?0:8);}
 void tick(){try{require(window_&&controller_,"destroyed");require(elapsed_.elapsed()<25000,"overall_timeout");switch(phase_){
 case 0:if(!window_->isExposed()||!window_->property("sceneReady").toBool())return;if(!window_->isActive()){window_->requestActivate();return;}eog_=qobject_cast<eog::EogDatasetController*>(window_->property("eog").value<QObject*>());require(eog_,"dataset_controller_missing");window_->setProperty("workspace","eog");eog_->loadFolder(QUrl::fromLocalFile(QStringLiteral("C:/Users/lucas/Documents/Codex/2026-11-07/for-x20/work/EogChallenge/history/mock_test_2026-10-10T1530")));phase_=1;return;
 case 1:{if(!eog_->loaded()||!window_->property("eogSceneReady").toBool())return;identity_=eog_->datasetIdentity();hash_=controller_->networkHash();revision_=controller_->simulationRevision();network_=controller_->document().value("network").toObject();auto*panel=find(window_->contentItem(),"eogDatasetPanel");require(panel,"dataset_panel_missing");gather(panel);require(choices_.size()==4,"four_native_selectors_missing");std::sort(choices_.begin(),choices_.end(),[](auto*a,auto*b){const auto x=a->mapToScene(QPointF(0,0)),y=b->mapToScene(QPointF(0,0));return x.y()<y.y()-2||(qAbs(x.y()-y.y())<=2&&x.x()<y.x());});for(auto*c:choices_)require(c->height()==40&&c->width()>0,"selector_geometry_invalid");checks_.append("four_readable_40px_native_comboboxes");click(choices_[0]);phase_=2;return;}
 case 2:{require(popup()&&popup()->property("visible").toBool(),"actual_mouse_did_not_open_popup");if(phaseTime_.elapsed()<350)return;auto*content=popup()->property("contentItem").value<QQuickItem*>();require(content&&content->height()>0&&popup()->property("height").toDouble()<=280,"popup_list_not_bounded_or_rendered");const auto folder=qEnvironmentVariable("BREAKWATER_SELECTOR_EVIDENCE_DIR");const auto img=window_->grabWindow();require(!img.isNull()&&img.save(folder+'/'+QString::number(index_+1)+"-dropdown-open.png","PNG"),"snapshot_failed");const int count=choices_[index_]->property("count").toInt(),before=choices_[index_]->property("currentIndex").toInt();require(count>0&&before>=0,"selector_model_unavailable");expected_=before+1<count?before+1:qMax(0,before-1);observed_.append(QJsonObject{{"selector",index_},{"count",count},{"index_before",before},{"popup_height",popup()->property("height").toDouble()},{"popup_width",popup()->property("width").toDouble()}});checks_.append(QString::number(index_)+"_actual_mouse_open_bounded_popup");key(expected_<before?Qt::Key_Up:Qt::Key_Down);key(Qt::Key_Return);phase_=3;return;}
 case 3:{require(!popup()->property("visible").toBool(),"keyboard_selection_did_not_close_popup");require(choices_[index_]->property("currentIndex").toInt()==expected_,"keyboard_selection_wrong_index");const char*names[]={"systemId","facilityId","assetId","metric"};require(choices_[index_]->property("currentValue").toString()==eog_->property(names[index_]).toString(),"keyboard_choice_not_bound_to_backend");same();checks_.append(QString::number(index_)+"_actual_keyboard_choice_matches_backend");if(++index_==4){checks_.append("all_selector_navigation_preserves_dataset_and_controls");finish(true,"ok");return;}click(choices_[index_]);phase_=2;return;}
 }}catch(const std::exception&e){finish(false,QString::fromLatin1(e.what()));}}
public:SelectorCheck(QQuickWindow*w,ProjectController*c,QCoreApplication*a):QObject(a),window_(w),controller_(c),app_(a){elapsed_.start();timer_.setInterval(100);connect(&timer_,&QTimer::timeout,this,[this]{tick();});timer_.start();}
};}void startWorkbenchUiCheck(QQuickWindow*w,ProjectController*c,QCoreApplication*a){new SelectorCheck(w,c,a);}}

