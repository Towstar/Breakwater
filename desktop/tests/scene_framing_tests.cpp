#include <QFile>
#include <QJSEngine>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPointF>
#include <QRectF>
#include <QtTest>
#include <cmath>

class SceneFramingTests final : public QObject {
    Q_OBJECT
    QJSEngine engine_;
    static QString json(const QJsonObject& value) {
        return QString::fromUtf8(QJsonDocument(value).toJson(QJsonDocument::Compact));
    }
    QJSValue evaluate(const QString& source) {
        const auto result=engine_.evaluate(source);
        if (result.isError()) qFatal("Scene framing script failed: %s",qPrintable(result.toString()));
        return result;
    }
    QJsonObject fit(const QJsonObject& bounds,const QJsonObject& viewport,const QJsonObject& insets,double zoom=1) {
        const auto source=QStringLiteral("JSON.stringify(fitPlan(%1,%2,%3,%4))")
            .arg(json(bounds),json(viewport),json(insets),QString::number(zoom,'g',17));
        return QJsonDocument::fromJson(evaluate(source).toString().toUtf8()).object();
    }
    static QJsonObject bounds(double minX,double maxX,double minZ,double maxZ) {
        return {{"minX",minX},{"maxX",maxX},{"minZ",minZ},{"maxZ",maxZ}};
    }
    static QJsonObject viewport(double width,double height) { return {{"width",width},{"height",height}}; }
    static QJsonObject insets(double left=14,double right=14,double top=96,double bottom=72) {
        return {{"left",left},{"right",right},{"top",top},{"bottom",bottom}};
    }
    static QPointF project(double x,double z,const QJsonObject& fit,const QJsonObject& viewport) {
        const double magnification=fit["magnification"].toDouble();
        return {(x-fit["cameraX"].toDouble())*magnification+viewport["width"].toDouble()/2,
                (z-fit["cameraZ"].toDouble())*magnification+viewport["height"].toDouble()/2};
    }
    static QRectF projectedPaddedBounds(const QJsonObject& model,const QJsonObject& fit,const QJsonObject& viewport) {
        return {project(model["minX"].toDouble()-90,model["minZ"].toDouble()-90,fit,viewport),
                project(model["maxX"].toDouble()+90,model["maxZ"].toDouble()+90,fit,viewport)};
    }
    static QRectF usable(const QJsonObject& fit) {
        const auto value=fit["usable"].toObject();
        return {value["x"].toDouble(),value["y"].toDouble(),value["width"].toDouble(),value["height"].toDouble()};
    }
    static void checkFits(const QJsonObject& model,const QJsonObject& fit,const QJsonObject& viewport) {
        QVERIFY(!fit.isEmpty());
        QVERIFY(std::isfinite(fit["magnification"].toDouble()) && fit["magnification"].toDouble()>0);
        const auto actual=projectedPaddedBounds(model,fit,viewport), available=usable(fit);
        QVERIFY2(available.adjusted(-1e-6,-1e-6,1e-6,1e-6).contains(actual),"Padded world bounds leave the usable viewport");
        QVERIFY(std::abs(actual.width()-available.width())<1e-6 || std::abs(actual.height()-available.height())<1e-6);
        QVERIFY(std::abs(actual.center().x()-available.center().x())<1e-6);
        QVERIFY(std::abs(actual.center().y()-available.center().y())<1e-6);
    }
private slots:
    void initTestCase() {
        QFile file(QStringLiteral(BREAKWATER_SOURCE_DIR "/desktop/qml/SceneFraming.js"));
        QVERIFY(file.open(QIODevice::ReadOnly));
        auto source=QString::fromUtf8(file.readAll()); source.remove(".pragma library");
        const auto result=engine_.evaluate(source); QVERIFY2(!result.isError(),qPrintable(result.toString()));
    }
    void projectedBoundsFitUsableViewport_data() {
        QTest::addColumn<QJsonObject>("model"); QTest::addColumn<QJsonObject>("view"); QTest::addColumn<QJsonObject>("reserved");
        QTest::newRow("canonical-landscape") << bounds(-240,100,-150,190) << viewport(1440,900) << insets();
        QTest::newRow("tall-layout-wide-short-view") << bounds(-40,40,-2400,2400) << viewport(1100,320) << insets();
        QTest::newRow("wide-layout-tall-view") << bounds(-2400,2400,-40,40) << viewport(360,900) << insets();
        QTest::newRow("single-equipment") << bounds(123,123,-456,-456) << viewport(960,540) << insets();
        QTest::newRow("full-layout-limits") << bounds(-10000,10000,-10000,10000) << viewport(1008,672) << insets(14,14,104,132);
        QTest::newRow("asymmetric-occlusion") << bounds(-300,120,-700,1100) << viewport(960,540) << insets(96,24,110,32);
        QTest::newRow("no-insets") << bounds(-100,100,-100,100) << viewport(800,600) << insets(0,0,0,0);
    }
    void projectedBoundsFitUsableViewport() {
        QFETCH(QJsonObject,model); QFETCH(QJsonObject,view); QFETCH(QJsonObject,reserved);
        const auto result=fit(model,view,reserved);
        checkFits(model,result,view);
        QCOMPARE(result["spanX"].toDouble(),model["maxX"].toDouble()-model["minX"].toDouble()+180);
        QCOMPARE(result["spanZ"].toDouble(),model["maxZ"].toDouble()-model["minZ"].toDouble()+180);
    }
    void viewportResizeRecomputesAUsefulFit() {
        const auto model=bounds(-40,40,-2400,2400), reserved=insets();
        const auto shortView=viewport(1200,300), tallView=viewport(1200,900);
        const auto shortFit=fit(model,shortView,reserved), tallFit=fit(model,tallView,reserved);
        checkFits(model,shortFit,shortView); checkFits(model,tallFit,tallView);
        QVERIFY(tallFit["magnification"].toDouble()>shortFit["magnification"].toDouble()*3);
    }
    void zoomScalesViewWithoutMovingTheUsableCenter() {
        const auto model=bounds(-200,400,-300,500), view=viewport(1008,672), reserved=insets(80,14,100,48);
        const auto fitted=fit(model,view,reserved), zoomed=fit(model,view,reserved,2);
        checkFits(model,fitted,view);
        const auto normalBounds=projectedPaddedBounds(model,fitted,view), zoomBounds=projectedPaddedBounds(model,zoomed,view);
        QCOMPARE(zoomed["magnification"].toDouble(),2*fitted["magnification"].toDouble());
        QVERIFY(std::abs(zoomBounds.width()-2*normalBounds.width())<1e-6);
        QVERIFY(std::abs(zoomBounds.height()-2*normalBounds.height())<1e-6);
        QVERIFY(std::abs(zoomBounds.center().x()-usable(zoomed).center().x())<1e-6);
        QVERIFY(std::abs(zoomBounds.center().y()-usable(zoomed).center().y())<1e-6);
        QVERIFY(!usable(zoomed).contains(zoomBounds)); // Zoom-in is intentional, not another fit guarantee.
    }
    void inputsRemainUnchanged() {
        QVERIFY(evaluate(QStringLiteral("(function(){const b={minX:-10,maxX:30,minZ:-700,maxZ:800},v={width:1008,height:672},"
            "i={left:14,right:30,top:96,bottom:72};const before=JSON.stringify([b,v,i]);"
            "const result=fitPlan(b,v,i,1);return !!result&&before===JSON.stringify([b,v,i])})()")).toBool());
    }
    void invalidInputsReturnNull_data() {
        QTest::addColumn<QString>("edit");
        QTest::newRow("null-bounds") << "b=null";
        QTest::newRow("missing-bound") << "delete b.maxX";
        QTest::newRow("string-bound") << "b.minX='0'";
        QTest::newRow("boolean-bound") << "b.minX=true";
        QTest::newRow("reversed-x") << "b.maxX=b.minX-1";
        QTest::newRow("reversed-z") << "b.maxZ=b.minZ-1";
        QTest::newRow("nan-bound") << "b.minX=NaN";
        QTest::newRow("infinite-bound") << "b.maxZ=Infinity";
        QTest::newRow("null-viewport") << "v=null";
        QTest::newRow("missing-height") << "delete v.height";
        QTest::newRow("zero-width") << "v.width=0";
        QTest::newRow("negative-height") << "v.height=-1";
        QTest::newRow("infinite-width") << "v.width=Infinity";
        QTest::newRow("null-insets") << "i=null";
        QTest::newRow("missing-inset") << "delete i.left";
        QTest::newRow("negative-inset") << "i.top=-1";
        QTest::newRow("string-inset") << "i.left='10'";
        QTest::newRow("boolean-inset") << "i.right=true";
        QTest::newRow("width-consumed") << "i.left=v.width;i.right=0";
        QTest::newRow("height-consumed") << "i.top=v.height;i.bottom=0";
        QTest::newRow("zero-zoom") << "z=0";
        QTest::newRow("negative-zoom") << "z=-1";
        QTest::newRow("nan-zoom") << "z=NaN";
        QTest::newRow("missing-zoom") << "z=undefined";
        QTest::newRow("string-zoom") << "z='1'";
        QTest::newRow("span-overflow") << "b.minX=-Number.MAX_VALUE;b.maxX=Number.MAX_VALUE";
        QTest::newRow("magnification-overflow") << "z=Number.MAX_VALUE";
        QTest::newRow("camera-overflow") << "v.width=Number.MAX_VALUE;v.height=1e-20;i={left:0,right:Number.MAX_VALUE/2,top:0,bottom:0}";
    }
    void invalidInputsReturnNull() {
        QFETCH(QString,edit);
        QVERIFY(evaluate(QStringLiteral("(function(){let b={minX:-1,maxX:1,minZ:-1,maxZ:1},v={width:640,height:480},"
            "i={left:10,right:10,top:10,bottom:10},z=1;%1;return fitPlan(b,v,i,z)===null})()").arg(edit)).toBool());
    }
    void smallPositiveZoomHasFiniteSafetyFloor() {
        const auto result=fit(bounds(-1,1,-1,1),viewport(640,480),insets(10,10,10,10),1e-12);
        QCOMPARE(result["magnification"].toDouble(),1e-6);
        QVERIFY(std::isfinite(result["cameraX"].toDouble()) && std::isfinite(result["cameraZ"].toDouble()));
    }
};
QTEST_GUILESS_MAIN(SceneFramingTests)
#include "scene_framing_tests.moc"
