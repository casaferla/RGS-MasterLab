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

int main(int argc,char** argv) {
    if(argc!=2)return 2;
    QGuiApplication app(argc,argv);
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
    constexpr std::array<const char*,10> controls{{
        "eqTopToolbarRegion","eqSpectrumToggleButton",
        "eqBandStripRegion","eqFrequencyControl","eqGainControl",
        "eqQControl","eqFilterGroup","filterButton_BELL",
        "filterButton_LOW_SHELF","filterButton_HIGH_SHELF"}};
    for(const auto* name:controls) {
        if(!equal_geometry(item(original,name),item(dynamic,name),name))
            return 5;
    }
    if(!equal_geometry(item(original,"eqOverallToggleButton"),
                       item(dynamic,"dynamicEqActivityToggleButton"),
                       "Overall-to-Activity toolbar slot"))return 6;
    if(!equal_geometry(item(original,"parametricEqGraph"),
                       item(dynamic,"parametricEqGraph"),
                       "EQ graph"))return 7;
    if (item(dynamic,"filterButton_NOTCH")
        || item(dynamic,"filterButton_HIGH_PASS")
        || item(dynamic,"filterButton_LOW_PASS")
        || !item(dynamic,"filterButton_RESERVED_NOT_A_FILTER")) {
        std::cerr<<"Unsupported Dynamic EQ filters or missing slot\n";
        return 8;
    }
    if(dynamic->property("manualEditable").toBool())return 9;
    auto* scope=item(dynamic,"parametricEqGraph");
    if(!scope || scope->property("manualEditable").toBool())return 10;
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
    std::cout<<"Dynamic EQ clone fixed-position Qt geometry: PASS\n";
    return 0;
}
