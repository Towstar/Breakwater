#include <QFile>
#include <QJSEngine>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRectF>
#include <QtTest>

class LabelPlacementTests final:public QObject {
    Q_OBJECT
    QJSEngine engine_;
    QJsonArray place(const QJsonArray& candidates,const QJsonObject& bounds,const QJsonArray& exclusions={}) {
        const auto call=QStringLiteral("JSON.stringify(place(%1,%2,%3))")
            .arg(QString::fromUtf8(QJsonDocument(candidates).toJson(QJsonDocument::Compact)),
                 QString::fromUtf8(QJsonDocument(bounds).toJson(QJsonDocument::Compact)),
                 QString::fromUtf8(QJsonDocument(exclusions).toJson(QJsonDocument::Compact)));
        const auto result=engine_.evaluate(call);
        if (result.isError()) qFatal("Label placement script failed: %s",qPrintable(result.toString()));
        return QJsonDocument::fromJson(result.toString().toUtf8()).array();
    }
    static QJsonObject row(const QString& id,int priority,double x=200,double y=140) {
        return {{"id",id},{"text",id},{"priority",priority},{"anchorX",x},{"anchorY",y},{"width",100},{"height",25}};
    }
private slots:
    void initTestCase() {
        QFile file(QStringLiteral(BREAKWATER_SOURCE_DIR "/desktop/qml/LabelPlacement.js")); QVERIFY(file.open(QIODevice::ReadOnly));
        auto source=QString::fromUtf8(file.readAll()); source.remove(".pragma library");
        const auto result=engine_.evaluate(source); QVERIFY2(!result.isError(),qPrintable(result.toString()));
    }
    void selectionPrecedesOtherLabelsAndInputOrderIsStable() {
        const QJsonArray candidates{row("ordinary",4),row("selected",0),row("hovered",1)};
        const auto original=candidates; const QJsonObject bounds{{"x",10},{"y",90},{"width",500},{"height",260}};
        const auto result=place(candidates,bounds); QCOMPARE(result.size(),3); QCOMPARE(result.first().toObject().value("id"),QJsonValue("selected"));
        QCOMPARE(candidates,original);
        QCOMPARE(result,place({candidates[2],candidates[0],candidates[1]},bounds));
    }
    void denseLabelsAvoidEachOtherReservedControlsAndViewportEdges() {
        const QJsonObject bounds{{"x",10},{"y",90},{"width",600},{"height",300}};
        const QJsonObject reserved{{"x",10},{"y",300},{"width",84},{"height",86}};
        QJsonArray candidates;
        for (int i=0;i<128;++i) candidates.append(row(QString("asset-%1").arg(i,3,10,QChar('0')),i==64 ? 0:4,20+(i%16)*36,100+(i/16)*36));
        const auto result=place(candidates,bounds,{reserved}); QVERIFY(result.size()>0); QVERIFY(result.size()<128);
        QCOMPARE(result.first().toObject().value("id"),QJsonValue("asset-064"));
        for (qsizetype i=0;i<result.size();++i) {
            const auto a=result[i].toObject(); const QRectF rectangle(a["x"].toDouble(),a["y"].toDouble(),a["width"].toDouble(),a["height"].toDouble());
            QVERIFY(QRectF(10,90,600,300).contains(rectangle)); QVERIFY(!rectangle.intersects(QRectF(10,300,84,86)));
            for (qsizetype j=0;j<i;++j) { const auto b=result[j].toObject();
                QVERIFY(!rectangle.intersects(QRectF(b["x"].toDouble(),b["y"].toDouble(),b["width"].toDouble(),b["height"].toDouble())));
            }
        }
    }
    void tinyOrInvalidViewportNeverCreatesClippedOrInvalidLabels() {
        QCOMPARE(place({row("too-wide",0)},{{"x",0},{"y",0},{"width",40},{"height",40}}).size(),0);
        QCOMPARE(place({row("invalid",0)},{{"x",0},{"y",0},{"width",0},{"height",40}}).size(),0);
        auto bad=row("bad",0); bad["width"]=-1;
        QCOMPARE(place({bad},{{"x",0},{"y",0},{"width",400},{"height",400}}).size(),0);
    }
};
QTEST_GUILESS_MAIN(LabelPlacementTests)
#include "label_placement_tests.moc"
