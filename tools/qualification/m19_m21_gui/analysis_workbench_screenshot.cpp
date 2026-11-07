// Real Qt 6 Quick screenshot review of isolated M19/M20/M21 UI.
// Every number is a labelled static DEMO fixture, never Product data.
#include <QCoreApplication>
#include <QDir>
#include <QEventLoop>
#include <QFont>
#include <QFontDatabase>
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
#include <stdexcept>

namespace {
void require(bool condition,const char* message) {
    if(!condition)throw std::runtime_error(message);
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
                const QString& id)
{
    view.resize(w,h);
    require(view.rootObject()!=nullptr,"GUI_ROOT_NOT_LOADED");
    require(view.rootObject()->setProperty("activeTab",tab),"GUI_TAB_SET_FAILED");
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
    const QString filename=dir+QStringLiteral("/analysis-")+id+
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
            screenshot(view,dir,1440,900,tab,names[static_cast<std::size_t>(tab)]);
            screenshot(view,dir,1184,688,tab,names[static_cast<std::size_t>(tab)]);
        }
        std::cout<<"RGSML_M19_M21_GUI_6_AUTHENTIC_QT_SCREENSHOTS_PASS_NOT_PRODUCT\n";
        return 0;
    }catch(const std::exception& e){
        std::cerr<<"RGSML_GUI_SCREENSHOT_FAILURE "<<e.what()<<'\n';
        return 1;
    }
}
