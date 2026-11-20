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
    void compactToolbarBoundaryRetainsAllCanonicalLabels_data() {
        QTest::addColumn<double>("sourceWidth");
        QTest::newRow("82-pixel-source-label") << 82.0;
        QTest::newRow("83-pixel-source-label") << 83.0;
        QTest::newRow("84-pixel-source-label") << 84.0;
    }
    void compactToolbarBoundaryRetainsAllCanonicalLabels() {
        QFETCH(double, sourceWidth);
        // 1008x672 normal WARP evidence, bb250d7, yaw 113.120243125:
        // scene-picking-warp-646c6e57dcfa409f9ef82f5bbdf23b66/benchmark.json.
        // The five retained rectangles recover their anchors. The culled source
        // anchor is projected from the same fitted camera and canonical position.
        const auto candidate = [](const QString& id,int priority,double x,double y,double width) {
            auto value=row(id,priority,x,y); value["width"]=width; value["height"]=26; return value;
        };
        const QJsonArray candidates{
            candidate("tank_a",3,394.37982177734375,125.08451080322266,49),
            candidate("tank_b",3,367.1260681152344,173.54283142089844,48),
            candidate("source_a",4,411.424,94.780,sourceWidth),
            candidate("source_b",4,314.2571716308594,153.665283203125,83),
            candidate("treatment",4,423.06439208984375,202.5261688232422,68),
            candidate("trucking",4,455.8133544921875,139.07737731933594,119)};
        const QJsonObject bounds{{"x",10},{"y",96},{"width",767},{"height",166}};
        const QJsonObject reserved{{"x",14},{"y",210},{"width",84},{"height",86}};
        const auto original=candidates, result=place(candidates,bounds,{reserved});
        QCOMPARE(candidates,original); QCOMPARE(result.size(),6);
        QCOMPARE(result.first().toObject().value("id"),QJsonValue("tank_a"));
        QJsonArray reversed; for (auto i=candidates.size();i>0;--i) reversed.append(candidates[i-1]);
        QCOMPARE(result,place(reversed,bounds,{reserved}));
        for (qsizetype i=0;i<result.size();++i) {
            const auto a=result[i].toObject();
            const QRectF rectangle(a["x"].toDouble(),a["y"].toDouble(),a["width"].toDouble(),a["height"].toDouble());
            QVERIFY(QRectF(10,96,767,166).contains(rectangle));
            QVERIFY(!rectangle.adjusted(-4,-4,4,4).intersects(QRectF(14,210,84,86)));
            for (qsizetype j=0;j<i;++j) {
                const auto b=result[j].toObject();
                QVERIFY(!rectangle.adjusted(-4,-4,4,4).intersects(
                    QRectF(b["x"].toDouble(),b["y"].toDouble(),b["width"].toDouble(),b["height"].toDouble())));
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
