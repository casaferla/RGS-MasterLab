#include <QGuiApplication>
#include <QQuickView>
#include <QQuickItem>
#include <QEventLoop>
#include <QTimer>
#include <QVariantMap>
#include <QVariantList>
#include <QVariant>
#include <QFont>
#include <QFontDatabase>
#include <QRawFont>
#include <QImage>
#include <QDir>
#include <QFileInfo>
#include <QSet>
#include <QtGlobal>
#include <QString>
#include <QUrl>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <numbers>
#include <stdexcept>
#include <string>
#include <utility>

namespace {
void require(bool yes, const char* message) {
    if(!yes)throw std::runtime_error(message);
}
QVariantMap card(QString name,QString value,QString status) {
    return {{"name",std::move(name)},{"value",std::move(value)},
            {"status",std::move(status)}};
}
QVariantMap row(QString name,QString prepared,QString gold,
                QString processed,QString pDelta,QString xDelta) {
    return {{"name",std::move(name)},{"prepared",std::move(prepared)},
            {"gold",std::move(gold)},{"processed",std::move(processed)},
            {"preparedDelta",std::move(pDelta)},
            {"processedDelta",std::move(xDelta)}};
}
QVariantMap demo_presentation() {
    // EXPLICIT PRIVATE DISPLAY FIXTURES, never claims real M19/M20/M21
    // measurements, accepted GOLD or qualified M16 true peak.
    QVariantList levels{
        card("INTEGRATED LUFS","-14.2 LUFS","CURRENT"),
        card("FULL-SCOPE RMS","-12.6 dBFS","CURRENT"),
        card("SAMPLE PEAK","-1.7 dBFS","CURRENT"),
        card("TRUE PEAK","—","UNAVAILABLE"),
        card("CREST","10.9 dB","CURRENT")
    };
    QVariantList spectrum,correlation,bands;
    constexpr int kPts=96;
    for(int i=0;i<kPts;++i) {
        const auto x=static_cast<double>(i)/(kPts-1);
        const auto shape=0.13+0.34*std::exp(-std::pow((x-0.24)/0.15,2.0))
            +0.23*std::exp(-std::pow((x-0.63)/0.10,2.0))
            +0.045*std::sin(48.0*x)+0.025*std::sin(120.0*x);
        const auto y=std::clamp(shape,0.07,0.94);
        spectrum.append(QVariantMap{{"x",x},{"y",y},{"valid",true}});
        const auto rho=0.66+0.19*std::sin(15.0*x)
                           +0.08*std::sin(44.0*x);
        correlation.append(QVariantMap{
            {"x",x},{"y",std::clamp((rho+1.0)/2.0,0.0,1.0)},
            {"valid",true}});
    }
    for(int i=0;i<35;++i) {
        const auto x=static_cast<double>(i)/35.0;
        const auto y=std::clamp(0.15+0.65*std::exp(
            -std::pow((x-0.38)/0.30,2.0))
            +0.055*std::sin(i*1.4),0.02,0.93);
        bands.append(QVariantMap{
            {"x",x},{"y",y},{"eligible",i>=3&&i<=30}});
    }
    QVariantList rows{
        row("Integrated LUFS","-18.2","-14.4","-14.2","+3.8 LU","-0.2 LU"),
        row("Full-scope RMS","-17.3","-13.1","-12.6","+4.2 dB","-0.5 dB"),
        row("Sample Peak","-4.8","-1.6","-1.7","+3.2 dB","+0.1 dB"),
        row("True Peak","MISSING","MISSING","MISSING","MISSING","MISSING"),
        row("Crest","12.5","11.5","10.9","-1.0 dB","+0.6 dB"),
        row("Width Ratio","0.91","1.06","1.02","+0.15","+0.04"),
        row("Correlation P10","0.41","0.36","0.38","-0.05","-0.02"),
        row("Correlation P50","0.75","0.70","0.72","-0.05","-0.02"),
        row("Correlation P90","0.94","0.91","0.92","-0.03","-0.01")
    };
    return {
        {"ready",true},
        {"scopeLabel",QStringLiteral("Full Track · independently scoped GOLD")},
        {"rateLabel",QStringLiteral("MASTER 48 kHz · stereo")},
        {"preparedState",QStringLiteral("CURRENT")},
        {"goldState",QStringLiteral("INDEPENDENT")},
        {"processedState",QStringLiteral("CURRENT")},
        {"levels",levels},{"spectrumPoints",spectrum},{"bands",bands},
        {"widthText",QStringLiteral("1.02")},
        {"correlationText",QStringLiteral("P10 0.38  ·  P50 0.72  ·  P90 0.92")},
        {"correlationPoints",correlation},{"rows",rows}
    };
}
void configure_font(QGuiApplication& app) {
#if defined(Q_OS_WIN)
    // Existing M21 CI proved that offscreen Windows can produce boxes
    // instead of glyphs unless an installed font is registered.
    const std::array<QString,2> paths{
        QStringLiteral("C:/Windows/Fonts/segoeui.ttf"),
        QStringLiteral("C:/Windows/Fonts/arial.ttf")};
    QString fontFamily;
    for(const auto& path:paths) {
        if(!QFileInfo::exists(path))continue;
        const auto id=QFontDatabase::addApplicationFont(path);
        if(id<0)continue;
        const auto families=QFontDatabase::applicationFontFamilies(id);
        if(!families.isEmpty()) {
            fontFamily=families.front();break;
        }
    }
    if(fontFamily.isEmpty()) {
        const auto available=QFontDatabase::families();
        if(available.contains(QStringLiteral("Segoe UI")))
            fontFamily=QStringLiteral("Segoe UI");
        else if(available.contains(QStringLiteral("Arial")))
            fontFamily=QStringLiteral("Arial");
    }
    require(!fontFamily.isEmpty(),"UX_REVIEW_WINDOWS_FONT_NOT_AVAILABLE");
    QFont font(fontFamily);
    font.setPointSize(10);
    app.setFont(font);
    const auto raw=QRawFont::fromFont(font);
    require(raw.isValid(),"UX_REVIEW_FONT_ATLAS_INVALID");
    for(auto glyph:raw.glyphIndexesForString(QStringLiteral("ANALYSIS 123")))
        require(glyph!=0,"UX_REVIEW_FONT_ASCII_GLYPH_MISSING");
#else
    (void)app;
#endif
}
void settle(int delayMs) {
    QEventLoop loop;
    QTimer::singleShot(delayMs,&loop,&QEventLoop::quit);
    loop.exec();
}
void render_and_save(QQuickView& view,int page,const QString& path) {
    require(view.rootObject()!=nullptr,"UX_REVIEW_ROOT_LOST");
    require(view.rootObject()->setProperty("selectedPage",page),
            "UX_REVIEW_TAB_PROPERTY_FAILED");
    settle(520);
    const auto result=view.grabWindow();
    require(!result.isNull(),"UX_REVIEW_QT_SCREENSHOT_NULL");
    require(result.width()>=1300&&result.height()>=790,
            "UX_REVIEW_SCREENSHOT_WRONG_DIMENSIONS");
    QSet<QRgb> colors;
    for(int y=0;y<result.height();y+=3)
        for(int x=0;x<result.width();x+=3)
            colors.insert(result.pixel(x,y));
    require(colors.size()>=85,"UX_REVIEW_BLANK_OR_FONT_BOX_SCREENSHOT");
    require(result.save(path,"PNG"),"UX_REVIEW_PNG_WRITE_FAILED");
    std::cout<<"QT_RENDERED_DEMO_PNG "<<path.toStdString()
             <<" COLORS "<<colors.size()<<'\n';
}
}
int main(int argc,char** argv) {
    try {
        QGuiApplication app(argc,argv);
        configure_font(app);
        QQuickView view;
        view.setColor(QColor("#071117"));
        view.setResizeMode(QQuickView::SizeRootObjectToView);
        view.setSource(QUrl::fromLocalFile(
            QString::fromUtf8(RGSML_ANALYSIS_REVIEW_QML)));
        if(view.status()!=QQuickView::Ready) {
            std::cerr<<"UX_REVIEW_QML_PARSE_ERROR\n";
            for(const auto& e:view.errors())
                std::cerr<<e.toString().toStdString()<<'\n';
            return 2;
        }
        auto* root=view.rootObject();
        require(root!=nullptr,"UX_REVIEW_ROOT_NULL");
        auto* missing=root->findChild<QObject*>("analysisMissingOverlay");
        auto* levels=root->findChild<QObject*>("m19LevelsPage");
        auto* tonal=root->findChild<QObject*>("m20TonalStereoPage");
        auto* metrics=root->findChild<QObject*>("m21MetricsPage");
        require(missing&&levels&&tonal&&metrics,
                "UX_REVIEW_ALL_THREE_PAGES_REQUIRED");
        require(missing->property("visible").toBool(),
                "UX_REVIEW_MISSING_DATA_MUST_BE_VISIBLE");
        view.resize(1360,820);
        view.show();
        settle(260);
        require(root->setProperty("presentation",demo_presentation()),
                "UX_REVIEW_PRESENTATION_BINDING_FAILED");
        settle(260);
        require(!missing->property("visible").toBool(),
                "UX_REVIEW_MISSING_DATA_OVERLAY_NOT_DISMISSED");
        const auto target=qEnvironmentVariable("RGSML_ANALYSIS_SCREENSHOT_DIR");
        require(!target.isEmpty(),"UX_REVIEW_EXPLICIT_OUTPUT_DIR_REQUIRED");
        require(QDir().mkpath(target),"UX_REVIEW_CREATE_OUTPUT_DIR_FAILED");
        const std::array<const char*,3> names{
            "rgsml-analysis-m19-levels.png",
            "rgsml-analysis-m20-tonal-stereo.png",
            "rgsml-analysis-m21-metrics-delta.png"};
        for(int page=0;page<3;++page) {
            render_and_save(view,page,
                QDir(target).filePath(QString::fromLatin1(names[page])));
            require(root->property("selectedPage").toInt()==page,
                    "UX_REVIEW_TAB_SELECTION_INCORRECT");
        }
        require(root->setProperty(
            "presentation",QVariantMap{{"ready",false}}),
            "UX_REVIEW_CLEAR_DATA_FAILED");
        settle(120);
        require(missing->property("visible").toBool(),
                "UX_REVIEW_STALE_DATA_NOT_CLEARED");
        std::cout<<"RGSML_M19_M20_M21_GENUINE_QT_SCREENSHOT_REVIEW_PASS_NOT_FQ\n";
        return 0;
    }catch(const std::exception& e) {
        std::cerr<<"UX_REVIEW_FAIL "<<e.what()<<'\n';
        return 3;
    }
}
