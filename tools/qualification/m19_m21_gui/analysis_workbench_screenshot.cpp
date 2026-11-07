// Real Qt 6 Quick screenshot review of isolated M19/M20/M21 UI.
// Every number is a labelled static DEMO fixture, never Product data.
#include <QCoreApplication>
#include <QDir>
#include <QEventLoop>
#include <QFont>
#include <QFontDatabase>
#include <QFileInfo>
#include <QGuiApplication>
#include <QImage>
#include <QQuickItem>
#include <QQuickView>
#include <QSet>
#include <QString>
#include <QTimer>
#include <QUrl>
#include <QVariantList>
#include <QVariantMap>
#include <array>
#include <algorithm>
#include <cmath>
#include <iostream>
#include <memory>
#include <stdexcept>

namespace {
void require(bool condition,const char* message) {
    if(!condition)throw std::runtime_error(message);
}
// Visual QQuickItem parenting may differ from QObject ownership after a
// LayoutItemProxy handoff; findChild<QObject*> can miss valid Repeater delegates.
QQuickItem* findVisualByObjectName(QQuickItem* root, const QString& name) {
    if(!root)return nullptr;
    if(root->objectName()==name)return root;
    for(auto* item:root->childItems()) {
        if(auto* match=findVisualByObjectName(item,name))return match;
    }
    return nullptr;
}

QVariantMap metrics_fixture() {
    QVariantList rows;
    const auto add=[&](const QString& label,const QString& prep,
                       const QString& gold,const QString& proc,
                       const QString& pDelta,const QString& xDelta) {
        rows.append(QVariantMap{
            {"name",label},{"prepared",prep},{"gold",gold},
            {"processed",proc},{"preparedDelta",pDelta},
            {"processedDelta",xDelta}});
    };
    add("Integrated LUFS","-18.4","-13.8","-14.2","+4.6","+0.4");
    add("Full-scope RMS","-21.6","-17.1","-17.5","+4.5","+0.4");
    add("Sample Peak","-3.8","-1.5","-1.2","+2.3","-0.3");
    add("True Peak","UNAVAILABLE","UNAVAILABLE","UNAVAILABLE",
        "UNAVAILABLE","UNAVAILABLE");
    add("Crest","17.8","15.6","16.3","-2.2","-0.7");
    add("Stereo Width","0.93","1.09","1.06","+0.16","+0.03");
    add("Correlation P10","0.12","0.07","0.09","-0.05","-0.02");
    add("Correlation P50","0.72","0.62","0.66","-0.10","-0.04");
    add("Correlation P90","0.95","0.91","0.93","-0.04","-0.02");
    return {{"metrics",rows}};
}
QVariantMap demo_data() {
    QVariantList spectrum;
    for(int i=0;i<=180;i++){
        const double x=static_cast<double>(i)/180.0;
        // Illustration for the visual graph ONLY: not audio analysis.
        const double bass=0.28*std::exp(-std::pow((x-0.15)/0.09,2));
        const double mid=0.36*std::exp(-std::pow((x-0.46)/0.08,2));
        const double presence=0.16*std::exp(-std::pow((x-0.70)/0.10,2));
        const double ripple=0.022*std::sin(95.0*x)+0.015*std::cos(47.0*x);
        const double y=std::clamp(0.12+bass+mid+presence+ripple,0.02,0.92);
        spectrum.append(QVariantMap{{"x",x},{"y",y}});
    }
    QVariantList bands;
    const std::array<const char*,12> labels{
        "63","125","250","500","1k","2k","3k","4k","6k","8k","12k","16k"};
    const std::array<double,12> fractions{
        0.33,0.42,0.68,0.81,0.74,0.56,0.65,0.59,0.40,0.32,0.22,0.12};
    for(std::size_t i=0;i<labels.size();++i)
        bands.append(QVariantMap{
            {"label",QString::fromLatin1(labels[i])},
            {"level",fractions[i]},
            {"supported",i<11}});
    auto data=metrics_fixture();
    data.insert("ready",true);
    data.insert("levels",QVariantMap{
        {"lufs","-14.2"},{"rms","-17.5"},{"samplePeak","-1.2"},
        {"truePeak","N/A"},{"crest","16.3"}});
    data.insert("spectrum",spectrum);
    data.insert("bands",bands);
    data.insert("stereo",QVariantMap{
        {"width","1.06"},{"P10","0.09"},{"P50","0.66"},{"P90","0.93"}});
    return data;
}
void display_frames() {
    QEventLoop eventLoop;
    QTimer::singleShot(450,&eventLoop,&QEventLoop::quit);
    eventLoop.exec();
    QCoreApplication::processEvents();
}
void screenshot(QQuickView& view,const QString& dir,
                const int w,const int h,const int tab,
                const QString& id,const bool standalone)
{
    view.resize(w,h);
    require(view.rootObject()!=nullptr,"GUI_ROOT_NOT_LOADED");
    require(view.rootObject()->setProperty("activeTab",tab),"GUI_TAB_SET_FAILED");
    require(view.rootObject()->setProperty("standaloneModuleMode",standalone),
            "GUI_STANDALONE_MODE_SET_FAILED");
    require(view.rootObject()->property("standaloneModuleMode").toBool()==standalone,
            "GUI_STANDALONE_MODE_READBACK_FAILED");
    view.show();
    display_frames();
    const auto capture=view.grabWindow();
    require(!capture.isNull(),"QT_WINDOW_GRAB_NULL");
    require(capture.width()>=w&&capture.height()>=h,
            "QT_CAPTURE_TRUNCATED");
    QSet<QRgb> colors;
    for(int y=0;y<capture.height();y+=3)
        for(int x=0;x<capture.width();x+=3)
            colors.insert(capture.pixel(x,y));
    require(colors.size()>=35,"QT_CAPTURE_BLANK_OR_FONT_MISSING");
    const QString filename=dir+
        (standalone?QStringLiteral("/standalone-"):QStringLiteral("/analysis-"))+id+
        QStringLiteral("-")+QString::number(w)+QStringLiteral("x")+
        QString::number(h)+QStringLiteral(".png");
    require(capture.save(filename,"PNG"),"QT_CAPTURE_PNG_WRITE_FAILED");
    std::cout<<"RGSML_REAL_QT_SCREENSHOT "<<filename.toStdString()
             <<" "<<w<<"x"<<h<<" colors="<<colors.size()<<'\n';
}
} // namespace
int main(int argc,char** argv) {
    try {
        QGuiApplication app(argc,argv);
        if(argc!=2){
            std::cerr<<"usage: analysis_workbench_screenshot OUTPUT_FOLDER\n";
            return 2;
        }
        // Windows Qt offscreen sometimes lacks a native font atlas.
        // Register an installed font, not a redistributed font file.
#ifdef Q_OS_WIN
        const auto fontId=QFontDatabase::addApplicationFont(
            QStringLiteral("C:/Windows/Fonts/segoeui.ttf"));
        if(fontId>=0){
            const auto families=QFontDatabase::applicationFontFamilies(fontId);
            if(!families.isEmpty())app.setFont(QFont(families.first(),10));
        }
#endif
        const QString dir=QString::fromLocal8Bit(argv[1]);
        require(QDir().mkpath(dir),"SCREENSHOT_OUTPUT_DIRECTORY_FAILED");
        QQuickView view;
        view.setResizeMode(QQuickView::SizeRootObjectToView);
        view.setSource(QUrl::fromLocalFile(
            QString::fromUtf8(RGSML_ANALYSIS_QML_SOURCE)));
        if(view.status()!=QQuickView::Ready){
            for(const auto& err:view.errors())
                std::cerr<<err.toString().toStdString()<<'\n';
            return 3;
        }
        require(view.rootObject()!=nullptr,"GUI_QML_COMPONENT_MISSING");
        require(view.rootObject()->setProperty("fixtureMode",true),
                "GUI_DEMO_WATERMARK_MISSING");
        require(view.rootObject()->setProperty("presentation",demo_data()),
                "GUI_DEMO_DATA_BINDING_FAILED");
        const std::array<QString,3> names{
            QStringLiteral("m19-levels"),QStringLiteral("m20-tonal-stereo"),
            QStringLiteral("m21-metrics-delta")};
        for(int tab=0;tab<3;++tab){
            screenshot(view,dir,1440,900,tab,names[static_cast<std::size_t>(tab)],false);
            screenshot(view,dir,1184,688,tab,names[static_cast<std::size_t>(tab)],false);
        }

        // Each individual analysis panel is a distinct native top-level QQuickView.
        // Product docking and authenticated data are NOT wired by this design harness.
        const std::array<QString,3> components{
            QStringLiteral("M19LevelsPanelPreview.qml"),
            QStringLiteral("M20TonalStereoPanelPreview.qml"),
            QStringLiteral("M21MetricsDeltaPanelPreview.qml")};
        const std::array<QString,3> expectedRoots{
            QStringLiteral("m19IndividualPanelPreview"),
            QStringLiteral("m20IndividualPanelPreview"),
            QStringLiteral("m21IndividualPanelPreview")};
        const QDir qmlFolder(QFileInfo(
            QString::fromUtf8(RGSML_ANALYSIS_QML_SOURCE)).absolutePath());
        std::array<std::unique_ptr<QQuickView>,3> individualWindows;
        for(int tab=0;tab<3;++tab){
            const auto index=static_cast<std::size_t>(tab);
            auto panel=std::make_unique<QQuickView>();
            panel->setResizeMode(QQuickView::SizeRootObjectToView);
            panel->setSource(QUrl::fromLocalFile(qmlFolder.filePath(components[index])));
            if(panel->status()!=QQuickView::Ready){
                for(const auto& err:panel->errors())
                    std::cerr<<err.toString().toStdString()<<'\n';
                throw std::runtime_error("GUI_INDIVIDUAL_QML_COMPONENT_NOT_READY");
            }
            require(panel->rootObject()!=nullptr,"GUI_INDIVIDUAL_ROOT_MISSING");
            require(panel->rootObject()->objectName()==expectedRoots[index],
                    "GUI_INDIVIDUAL_ROOT_ID_MISMATCH");
            require(panel->rootObject()->property("activeTab").toInt()==tab,
                    "GUI_INDIVIDUAL_SELECTED_MODULE_WRONG");
            require(panel->rootObject()->property("standaloneModuleMode").toBool(),
                    "GUI_INDIVIDUAL_MODE_INACTIVE");
            require(panel->rootObject()->setProperty("fixtureMode",true),
                    "GUI_INDIVIDUAL_FIXTURE_BADGE_MISSING");
            require(panel->rootObject()->setProperty("presentation",demo_data()),
                    "GUI_INDIVIDUAL_FIXTURE_BINDING_FAILED");
            screenshot(*panel,dir,1440,900,tab,names[index],true);
            screenshot(*panel,dir,1184,688,tab,names[index],true);
            individualWindows[index]=std::move(panel);
        }
        for(int tab=0;tab<3;++tab){
            const auto index=static_cast<std::size_t>(tab);
            require(individualWindows[index]!=nullptr
                    && individualWindows[index]->isVisible(),
                    "GUI_THREE_INDIVIDUAL_WINDOWS_NOT_SIMULTANEOUS");
            require(individualWindows[index]->rootObject()->objectName()
                    ==expectedRoots[index],"GUI_INDIVIDUAL_ROOT_CHANGED");
        }
        require(individualWindows[0].get()!=individualWindows[1].get()
                && individualWindows[1].get()!=individualWindows[2].get()
                && individualWindows[0].get()!=individualWindows[2].get(),
                "GUI_QQUICKVIEW_OBJECT_REUSED");
        // Rehost each analysis QML panel without cloning the module.
        // This validates Preview-only Qt layout transfers; Main.qml remains idle.
        std::array<std::unique_ptr<QQuickView>,3> lifecycleHosts;
        for(int tab=0;tab<3;++tab){
            auto host=std::make_unique<QQuickView>();
            host->setResizeMode(QQuickView::SizeRootObjectToView);
            host->setSource(QUrl::fromLocalFile(
                qmlFolder.filePath(QStringLiteral("AnalysisModuleDockHostPreview.qml"))));
            if(host->status()!=QQuickView::Ready){
                for(const auto& err:host->errors())
                    std::cerr<<err.toString().toStdString()<<'\n';
                throw std::runtime_error("ANALYSIS_DOCK_HOST_QML_LOAD_FAILED");
            }
            auto* const hostRoot=host->rootObject();
            require(hostRoot!=nullptr,"ANALYSIS_DOCK_HOST_MISSING");
            require(hostRoot->setProperty("moduleIndex",tab),
                    "ANALYSIS_DOCK_MODULE_SELECTION_FAILED");
            require(hostRoot->setProperty("fixtureMode",false),
                    "ANALYSIS_DOCK_MUST_HAVE_NO_FAKE_DATA");
            host->resize(1184,688);
            host->show();
            display_frames();
            const auto* const module=hostRoot->findChild<QObject*>(
                expectedRoots[static_cast<std::size_t>(tab)]);
            require(module!=nullptr,"ANALYSIS_DOCK_REAL_PANEL_MISSING");
            require(module->property("fixtureMode").toBool()==false,
                    "ANALYSIS_DOCK_FIXTURE_DATA_LEAK");
            require(hostRoot->property("detached").toBool()==false,
                    "ANALYSIS_DOCK_INITIAL_STATE_WRONG");
            require(QMetaObject::invokeMethod(hostRoot,"requestUndock"),
                    "ANALYSIS_UNDOCK_METHOD_MISSING");
            display_frames();
            auto* const floating=hostRoot->findChild<QQuickWindow*>(
                "analysisNativeFloatingWindow");
            require(floating!=nullptr && floating->isVisible(),
                    "ANALYSIS_NATIVE_FLOATING_WINDOW_NOT_VISIBLE");
            require(floating->minimumWidth()>=1184
                    && floating->minimumHeight()>=688,
                    "ANALYSIS_NATIVE_WINDOW_BELOW_MIN_SIZE");
            require(hostRoot->findChild<QObject*>(
                expectedRoots[static_cast<std::size_t>(tab)])==module,
                "ANALYSIS_UNDOCK_DUPLICATED_PANEL");
            require(QMetaObject::invokeMethod(hostRoot,"requestDock"),
                    "ANALYSIS_DOCK_METHOD_MISSING");
            display_frames();
            require(hostRoot->property("detached").toBool()==false
                    && !floating->isVisible(),
                    "ANALYSIS_REDOCK_DID_NOT_HIDE_NATIVE_WINDOW");
            require(hostRoot->findChild<QObject*>(
                expectedRoots[static_cast<std::size_t>(tab)])==module,
                "ANALYSIS_REDOCK_RECREATED_PANEL");
            require(hostRoot->property("transitionEpoch").toInt()==2,
                    "ANALYSIS_REDOCK_TRANSITION_EPOCH_WRONG");
            auto* undock=hostRoot->findChild<QObject*>("analysisUndockButton");
            auto* redock=hostRoot->findChild<QObject*>("analysisRedockButton");
            require(undock && redock,"ANALYSIS_SHARED_BUTTONS_MISSING");
            require(undock->property("tone").toString()=="secondary"
                    && redock->property("tone").toString()=="secondary",
                    "ANALYSIS_SHARED_BUTTON_STYLE_MISSING");
            if(tab < 2){
                const QString target=tab==0
                    ? QStringLiteral("m19MetricValue_lufs")
                    : QStringLiteral("m20StereoWidthReadout");
                auto* visualRoot=qobject_cast<QQuickItem*>(hostRoot);
                auto* value=findVisualByObjectName(visualRoot,target);
                require(value!=nullptr,"ANALYSIS_VISUAL_READOUT_MISSING");
                require(value->property("readOnly").toBool(),
                        "ANALYSIS_READOUT_MUST_BE_READ_ONLY");
                require(!value->property("dataAvailable").toBool(),
                        "ANALYSIS_UNAUTHENTICATED_MEASUREMENT_LEAK");
                if(tab==0){
                    require(value->property("heroReadout").toBool(),
                            "ANALYSIS_M19_INPUT_GAIN_STYLE_MISSING");
                    require(value->property("numericPixelSize").toInt()==36,
                            "ANALYSIS_M19_INPUT_GAIN_FONT_SIZE_WRONG");
                }
            }
            lifecycleHosts[static_cast<std::size_t>(tab)]=std::move(host);
        }
        std::cout<<"RGSML_M19_M21_THREE_DOCK_UNDOCK_NATIVE_QT_LIFECYCLES_PASS_NO_PRODUCT_BINDING\n";
        std::cout<<"RGSML_M19_M21_GUI_12_WINDOWS_3_DISTINCT_QML_ROOTS_PASS_NOT_PRODUCT\n";
        return 0;
    }catch(const std::exception& e){
        std::cerr<<"RGSML_GUI_SCREENSHOT_FAILURE "<<e.what()<<'\n';
        return 1;
    }
}
