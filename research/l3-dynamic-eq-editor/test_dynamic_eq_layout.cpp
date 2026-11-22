#include <QFileInfo>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQuickItem>
#include <QQuickWindow>
#include <QDir>
#include <QImage>
#include <cmath>
#include <iostream>
#include <string_view>
#include <array>

static QQuickItem* item(QObject* root,const char* name) {
    return qobject_cast<QQuickItem*>(
        root->findChild<QObject*>(QString::fromLatin1(name)));
}
static bool equal_geometry(QQuickItem* a,QQuickItem* b,
                            std::string_view label) {
    if (!a || !b) {
        std::cerr<<"Missing common control "<<label<<"\n";
        return false;
    }
    const auto close=[](double x,double y) {
        return std::abs(x-y)<=0.5;
    };
    if (!close(a->x(),b->x()) || !close(a->y(),b->y())
        || !close(a->width(),b->width())
        || !close(a->height(),b->height())) {
        std::cerr<<"Geometry drift in "<<label<<"\n";
        return false;
    }
    return true;
}

static bool equal_position(QQuickItem* a,QQuickItem* b,
                            std::string_view label) {
    if(!a || !b)return false;
    // The requirement concerns absolute on-screen placement, NOT local
    // delegate coordinates within two independently-polished RowLayouts.
    const QPointF sceneA=a->mapToScene(QPointF(0,0));
    const QPointF sceneB=b->mapToScene(QPointF(0,0));
    const bool xok=std::abs(sceneA.x()-sceneB.x())<=1.0;
    const bool yok=std::abs(sceneA.y()-sceneB.y())<=1.0;
    if(!xok || !yok)
        std::cerr<<"Position drift in "<<label<<"\n";
    return xok && yok;
}

int main(int argc,char** argv) {
    if(argc!=3)return 2;
    QGuiApplication app(argc,argv);
    const QString stage=QString::fromLocal8Bit(argv[2]);
    QQmlApplicationEngine engine;
    engine.load(QUrl::fromLocalFile(
        QFileInfo(QString::fromLocal8Bit(argv[1])).absoluteFilePath()));
    if(engine.rootObjects().size()!=1)return 3;
    QObject* root=engine.rootObjects().first();
    QObject* original=root->findChild<QObject*>(
        QStringLiteral("originalEqResearch"));
    QObject* dynamic=root->findChild<QObject*>(
        QStringLiteral("dynamicEqResearch"));
    if(!original || !dynamic)return 4;
    app.processEvents();
    // Force Qt Quick to polish/layout BOTH editor subtrees before testing
    // individual Repeater button positions. Simple processEvents() alone
    // is insufficient for offscreen scene graph geometry.
    auto* quickWindow=qobject_cast<QQuickWindow*>(root);
    if(!quickWindow)return 14;
    quickWindow->show();
    app.processEvents();
    const auto firstFrame=quickWindow->grabWindow();
    if(firstFrame.isNull())return 15;
    app.processEvents();
    if(stage==QStringLiteral("load")) {
        std::cout<<"Dynamic EQ QML scene: PASS\n";
        return 0;
    }
    if(stage==QStringLiteral("measure")) {
        QQuickItem* a=item(original,"filterButton_BELL");
        QQuickItem* b=item(dynamic,"filterButton_BELL");
        if(!a || !b)return 16;
        const auto pa=a->mapToScene(QPointF{0,0});
        const auto pb=b->mapToScene(QPointF{0,0});
        const auto ten=[](double n) {return std::lround(n*10.0);};
        std::cout<<"ox"<<ten(pa.x())<<"-oy"<<ten(pa.y())
                 <<"-dx"<<ten(pb.x())<<"-dy"<<ten(pb.y())
                 <<"-ow"<<ten(a->width())<<"-dw"<<ten(b->width())
                 <<"-oh"<<ten(a->height())<<"-dh"<<ten(b->height());
        return 0;
    }
    constexpr std::array<const char*,10> controls{{
        "eqTopToolbarRegion","eqSpectrumToggleButton",
        "eqBandStripRegion","eqFrequencyControl","eqGainControl",
        "eqQControl","eqFilterGroup","filterButton_BELL",
        "filterButton_LOW_SHELF","filterButton_HIGH_SHELF"}};
    if(stage==QStringLiteral("common")
       || stage.startsWith(QStringLiteral("filter-position-"))) {
        std::size_t begin=0U,end=7U;
        if(stage==QStringLiteral("filter-position-bell")) {begin=7U;end=8U;}
        else if(stage==QStringLiteral("filter-position-low")) {begin=8U;end=9U;}
        else if(stage==QStringLiteral("filter-position-high")) {begin=9U;end=10U;}
        for(std::size_t i=begin;i<end;++i) {
            const auto* name=controls[i];
            const bool filters=i>=7U;
            const bool match=filters
                ?equal_position(item(original,name),item(dynamic,name),name)
                :equal_geometry(item(original,name),item(dynamic,name),name);
            if(!match)return 5;
        }
    }
    if(stage==QStringLiteral("common")) {
        if(!equal_geometry(item(original,"eqOverallToggleButton"),
                           item(dynamic,"dynamicEqActivityToggleButton"),
                           "Overall-to-Activity toolbar slot"))return 6;
        if(!equal_geometry(item(original,"parametricEqGraph"),
                           item(dynamic,"parametricEqGraph"),
                           "EQ graph"))return 7;
        if(dynamic->property("manualEditable").toBool())return 9;
        auto* scope=item(dynamic,"parametricEqGraph");
        if(!scope || scope->property("manualEditable").toBool())return 10;
    }
    if(stage==QStringLiteral("filter-safe")) {
        if(item(dynamic,"filterButton_NOTCH")
            || item(dynamic,"filterButton_HIGH_PASS")
            || item(dynamic,"filterButton_LOW_PASS")
            || !item(dynamic,"filterButton_RESERVED_NOT_A_FILTER"))
            return 8;
    }
    const QString screenshotDir=qEnvironmentVariable(
        "RGSML_DYNAMIC_EQ_SCREENSHOT_DIR");
    if(!screenshotDir.isEmpty()) {
        auto* window=qobject_cast<QQuickWindow*>(root);
        if(!window)return 11;
        window->show();
        app.processEvents();
        const QImage shot=window->grabWindow();
        if(shot.isNull() || !QDir().mkpath(screenshotDir))return 12;
        if(!shot.save(QDir(screenshotDir).filePath(
            QStringLiteral("DynamicEq-Structural-Research.png")),"PNG"))
            return 13;
    }
    std::cout<<"Dynamic EQ Qt research stage "<<stage.toStdString()<<" PASS\n";
    return 0;
}
