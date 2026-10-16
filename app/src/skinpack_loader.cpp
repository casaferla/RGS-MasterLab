// Experimental R1 reference loader — UNQUALIFIED; not included in application build.
// Source from the previously delivered R1 integration kit. Audit native ZIP parser before use.
#include "skinpack_loader.hpp"

#include <QBuffer>
#include <QCryptographicHash>
#include <QFile>
#include <QImageReader>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QReadWriteLock>
#include <QSet>

extern "C" {
#include <mz.h>
#include <mz_zip.h>
#include <mz_zip_rw.h>
}

#include <algorithm>
#include <cstdint>
#include <limits>
#include <utility>
#include <map>
#include <optional>

namespace rgsml::app {
namespace {
constexpr qint64 kMaxArchive = 20LL * 1024 * 1024;
constexpr qint64 kMaxTotal = 60LL * 1024 * 1024;
constexpr qint64 kMaxEntry = 16LL * 1024 * 1024;
constexpr int kMaxEntries = 128;
constexpr int kMaxDimension = 8192;
constexpr qint64 kMaxPixels = 32LL * 1024 * 1024;

struct Payloads final {
    std::map<QString, QByteArray> files;
};

bool safePath(const QString& path)
{
    if (path.isEmpty() || path.size() > 512 || path.startsWith('/') || path.endsWith('/')
        || path.contains('\\') || path.contains(':') || path.contains(QChar(0))) return false;
    for (const auto& part : path.split('/')) {
        if (part.isEmpty() || part == "." || part == "..") return false;
        for (const QChar c : part) {
            const ushort x = c.unicode();
            const bool asciiSafe = (x >= 'A' && x <= 'Z') || (x >= 'a' && x <= 'z')
                || (x >= '0' && x <= '9') || c == '_' || c == '-' || c == '.';
            if (!asciiSafe) return false;
        }
    }
    const auto ext = path.mid(path.lastIndexOf('.')).toLower();
    return ext == ".json" || ext == ".png" || ext == ".webp"
        || ext == ".md" || ext == ".txt";
}

std::optional<Payloads> unzipBounded(const QByteArray& archive, QString* error)
{
    void* reader = mz_zip_reader_create();
    if (!reader) { *error = "ZIP reader initialization failed"; return std::nullopt; }
    const auto release = [&reader] { mz_zip_reader_close(reader); mz_zip_reader_delete(&reader); };
    if (mz_zip_reader_open_buffer(reader,
        reinterpret_cast<const uint8_t*>(archive.constData()),
        static_cast<int32_t>(archive.size()), 1) != MZ_OK) {
        *error = "Corrupt or unsupported ZIP container";
        release(); return std::nullopt;
    }
    Payloads result;
    QSet<QString> normalizedNames;
    qint64 totalUncompressed = 0;
    int count = 0;
    int rc = mz_zip_reader_goto_first_entry(reader);
    if (rc != MZ_OK) { *error = "Empty ZIP"; release(); return std::nullopt; }
    do {
        mz_zip_file* info = nullptr;
        if (++count > kMaxEntries || mz_zip_reader_entry_get_info(reader, &info) != MZ_OK
            || !info || !info->filename || info->filename_size == 0) {
            *error = "ZIP entry count or metadata invalid"; release(); return std::nullopt;
        }
        const auto raw = QByteArray(info->filename, static_cast<qsizetype>(info->filename_size));
        const QString path = QString::fromUtf8(raw.constData(), raw.size());
        if (path.toUtf8() != raw || !safePath(path)
            || normalizedNames.contains(path.toCaseFolded())
            || mz_zip_reader_entry_is_dir(reader) == MZ_OK
            || (info->external_fa >> 16 & 0170000U) == 0120000U
            || (info->flag & MZ_ZIP_FLAG_ENCRYPTED) != 0
            || info->aes_version != 0 || info->comment_size != 0
            || info->disk_number != 0
            || (info->compression_method != MZ_COMPRESS_METHOD_STORE
                && info->compression_method != MZ_COMPRESS_METHOD_DEFLATE)
            || info->compressed_size < 0 || info->uncompressed_size < 0
            || info->uncompressed_size > kMaxEntry
            || info->compressed_size > kMaxArchive
            || totalUncompressed > kMaxTotal - info->uncompressed_size) {
            *error = "Unsafe ZIP entry: " + path;
            release(); return std::nullopt;
        }
        normalizedNames.insert(path.toCaseFolded());
        totalUncompressed += info->uncompressed_size;
        if (mz_zip_reader_entry_open(reader) != MZ_OK) {
            *error = "Cannot open ZIP entry: " + path;
            release(); return std::nullopt;
        }
        QByteArray data;
        data.reserve(static_cast<qsizetype>(info->uncompressed_size));
        char buffer[65536];
        qint64 remaining = info->uncompressed_size;
        bool ok = true;
        while (remaining > 0) {
            const auto readLimit = static_cast<int32_t>(std::min<qint64>(sizeof(buffer), remaining));
            const int32_t got = mz_zip_reader_entry_read(reader, buffer, readLimit);
            if (got <= 0 || got > readLimit) { ok = false; break; }
            data.append(buffer, got);
            remaining -= got;
        }
        if (mz_zip_reader_entry_close(reader) != MZ_OK || !ok) {
            *error = "ZIP entry data or checksum invalid: " + path;
            release(); return std::nullopt;
        }
        result.files.emplace(path, std::move(data));
        rc = mz_zip_reader_goto_next_entry(reader);
    } while (rc == MZ_OK);
    if (rc != MZ_END_OF_LIST) { *error = "ZIP entry iteration failed"; release(); return std::nullopt; }
    release();
    if (!result.files.contains("manifest.json") || !result.files.contains("skin.json")) {
        *error = "Missing manifest.json or skin.json"; return std::nullopt;
    }
    return result;
}

std::optional<QJsonObject> parseObject(const QByteArray& data, QString* error)
{
    if (data.size() > 1024 * 1024) { *error = "JSON too large"; return std::nullopt; }
    QJsonParseError parseError;
    const auto doc = QJsonDocument::fromJson(data, &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
        *error = "Invalid JSON structure"; return std::nullopt;
    }
    return doc.object();
}

bool colorOf(const QJsonValue& raw, QColor* result)
{
    if (!raw.isString()) return false;
    const auto text = raw.toString();
    if (text.size() != 7 || !text.startsWith('#')) return false;
    for (const QChar x : text.mid(1)) {
        if (!((x >= '0' && x <= '9') || (x >= 'a' && x <= 'f')
                || (x >= 'A' && x <= 'F'))) return false;
    }
    *result = QColor(text);
    return result->isValid();
}

bool validSha(const QString& s)
{
    if (s.size() != 64) return false;
    for (const QChar c : s) if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))) return false;
    return true;
}

bool validateManifest(const QJsonObject& manifest, const Payloads& archive, QString* error)
{
    if (manifest.value("format").toString() != "rgsml.skinpack"
        || manifest.value("formatVersion").toString() != "0.1"
        || manifest.value("hostContract").toString() != "rgsml.skin.host/0.1"
        || manifest.value("entry").toString() != "skin.json"
        || manifest.value("geometry").toString() == "override") {
        *error = "Incompatible SkinPack contract"; return false;
    }
    const auto trust = manifest.value("trust").toObject();
    if (!trust.value("executableCode").isBool() || trust.value("executableCode").toBool()
        || !trust.value("networkAccess").isBool() || trust.value("networkAccess").toBool()) {
        *error = "SkinPack trust contract invalid"; return false;
    }
    const auto index = manifest.value("assetIndex").toObject();
    if (index.isEmpty() || index.size() > kMaxEntries) { *error = "Asset index missing"; return false; }
    QSet<QString> indexed;
    for (auto it = index.begin(); it != index.end(); ++it) {
        if (!it.value().isObject()) { *error = "Asset descriptor invalid"; return false; }
        const auto obj = it.value().toObject();
        const auto path = obj.value("path").toString();
        const auto hash = obj.value("sha256").toString();
        const auto bytes = obj.value("bytes");
        const auto payload = archive.files.find(path);
        if (!safePath(path) || !validSha(hash) || !bytes.isDouble()
            || bytes.toDouble() < 0 || bytes.toDouble() > kMaxEntry
            || bytes.toDouble() != static_cast<double>(payload == archive.files.end() ? -1 : payload->second.size())
            || payload == archive.files.end()
            || indexed.contains(path.toCaseFolded())
            || QCryptographicHash::hash(payload->second, QCryptographicHash::Sha256).toHex() != hash.toLatin1()) {
            *error = "Asset integrity mismatch: " + path; return false;
        }
        indexed.insert(path.toCaseFolded());
    }
    return true;
}

bool getMarble(const QJsonObject& manifest, const QJsonObject& skin,
               const Payloads& archive, QImage* image, QColor* base, QColor* border,
               QString* error)
{
    if (skin.value("$schema").toString() != "rgsml.skin.definition/0.1"
        || skin.value("geometry").toString() != "inherit-host"
        || skin.value("semanticColors").toString() != "inherit-host") {
        *error = "Unsupported skin semantics"; return false;
    }
    const auto profiles = skin.value("profiles").toObject();
    const auto surface = profiles.value("surface").toObject();
    const auto slot = surface.value("slots").toObject().value("transport.background").toObject();
    const auto materialId = slot.value("material").toString();
    const auto materials = skin.value("materials").toObject();
    const auto material = materials.value(materialId).toObject();
    if (materialId.isEmpty() || material.value("kind").toString() != "texture"
        || material.value("fit").toString() != "tile-x"
        || !colorOf(material.value("baseColor"), base)
        || !colorOf(material.value("borderColor"), border)) {
        *error = "Unsupported transport surface material"; return false;
    }
    const auto assetId = material.value("asset").toString();
    const auto asset = manifest.value("assetIndex").toObject().value(assetId).toObject();
    const auto path = asset.value("path").toString();
    const auto found = archive.files.find(path);
    if (found == archive.files.end() || !path.endsWith(".png")) {
        *error = "Missing or unsupported surface texture"; return false;
    }
    QBuffer io;
    io.setData(found->second);
    if (!io.open(QIODevice::ReadOnly)) { *error = "Cannot read texture"; return false; }
    QImageReader decoder(&io, "png");
    decoder.setAutoTransform(false);
    const QSize size = decoder.size();
    if (!size.isValid() || size.width() > kMaxDimension || size.height() > kMaxDimension
        || qint64(size.width()) * size.height() > kMaxPixels || !decoder.canRead()) {
        *error = "Texture dimensions or format invalid"; return false;
    }
    *image = decoder.read();
    if (image->isNull() || image->size() != size) { *error = "Texture decode failed"; return false; }
    return true;
}
} // namespace

SkinImageProvider::SkinImageProvider(std::shared_ptr<SkinImageStore> store)
    : QQuickImageProvider(QQuickImageProvider::Image), store_(std::move(store)) {}

QImage SkinImageProvider::requestImage(const QString& id, QSize* size, const QSize&)
{
    QReadLocker guard(&store_->lock);
    const QImage result = id.startsWith("transport") ? store_->transport : QImage{};
    if (size) *size = result.size();
    return result;
}

SkinPackLoader::SkinPackLoader(std::shared_ptr<SkinImageStore> store, QObject* parent)
    : QObject(parent), store_(std::move(store)) {}

bool SkinPackLoader::reject(const QString& reason)
{
    error_ = reason;
    emit changed();
    return false;
}

bool SkinPackLoader::openFromUrl(const QUrl& fileUrl)
{
    if (!fileUrl.isLocalFile()) return reject("Only local .rgsskin files are supported");
    if (!fileUrl.toLocalFile().endsWith(".rgsskin", Qt::CaseInsensitive))
        return reject("File extension must be .rgsskin");
    QFile input(fileUrl.toLocalFile());
    if (!input.open(QIODevice::ReadOnly) || input.size() < 22 || input.size() > kMaxArchive)
        return reject("Cannot open SkinPack or package exceeds 20 MiB");
    const auto bytes = input.read(kMaxArchive + 1);
    if (bytes.size() != input.size() || bytes.size() > kMaxArchive)
        return reject("SkinPack read or length failed");
    QString error;
    auto archive = unzipBounded(bytes, &error);
    if (!archive) return reject(error);
    auto manifest = parseObject(archive->files.at("manifest.json"), &error);
    auto skin = parseObject(archive->files.at("skin.json"), &error);
    if (!manifest || !skin) return reject(error);
    if (!validateManifest(*manifest, *archive, &error)) return reject(error);
    QImage candidate;
    QColor base, border;
    if (!getMarble(*manifest, *skin, *archive, &candidate, &base, &border, &error))
        return reject(error);
    const auto name = manifest->value("name").toString();
    if (name.isEmpty() || name.size() > 128) return reject("Invalid SkinPack name");
    // Atomic publication: do not mutate current skin before checks pass.
    {
        QWriteLocker guard(&store_->lock);
        store_->transport = std::move(candidate);
    }
    active_ = true;
    name_ = name;
    backgroundColor_ = base;
    borderColor_ = border;
    error_.clear();
    ++revision_;
    emit changed();
    return true;
}

void SkinPackLoader::reset()
{
    {
        QWriteLocker guard(&store_->lock);
        store_->transport = QImage{};
    }
    active_ = false;
    name_ = "RGS Original";
    error_.clear();
    ++revision_;
    emit changed();
}

} // namespace rgsml::app
