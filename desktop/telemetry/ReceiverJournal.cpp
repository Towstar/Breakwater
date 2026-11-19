#include "ReceiverJournal.hpp"
#include "breakwater/telemetry.hpp"
#include <QDateTime>
#include <QJsonDocument>
#include <QRegularExpression>
#include <cmath>
#include <stdexcept>

namespace breakwater::desktop::telemetry {
namespace {
void require(bool condition) { if (!condition) throw std::invalid_argument("Invalid or unsupported receiver-timed local recording."); }
QByteArray wire(const QJsonObject& object) { return QJsonDocument(object).toJson(QJsonDocument::Compact)+'\n'; }
void keys(const QJsonObject& object,const QStringList& fields) {
    require(object.size()==fields.size()); for (const auto& field:fields) require(object.contains(field));
}
bool hash(const QString& value) {
    static const QRegularExpression pattern(QStringLiteral("^[0-9a-f]{64}\\z")); return pattern.match(value).hasMatch();
}
double clock(const QJsonObject& object,const char* key) {
    const auto value=object.value(QLatin1String(key)); require(value.isDouble() && std::isfinite(value.toDouble()) && value.toDouble()>=0);
    return value.toDouble();
}
}
QJsonObject journal_header(const QString& configurationHash,const QString& physicalHash) {
    require(hash(configurationHash) && hash(physicalHash));
    return {{"recording_version",1},{"kind","receiver_journal"},{"configuration_hash",configurationHash},{"physical_hash",physicalHash}};
}
void validate_journal_header(const QJsonObject& header,const QString& configurationHash,const QString& physicalHash) {
    require(header==journal_header(configurationHash,physicalHash));
}
JournalReceipt parse_journal_receipt(const QJsonObject& object) {
    keys(object,{"type","received_monotonic_s","received_event_time_s","received_utc","raw_frame_base64"});
    require(object.value("type")=="frame");
    JournalReceipt receipt;
    receipt.monotonic_s=clock(object,"received_monotonic_s"); receipt.event_s=clock(object,"received_event_time_s");
    receipt.utc=object.value("received_utc").toString();
    require(receipt.utc.size()<=64 && receipt.utc.endsWith('Z') && QDateTime::fromString(receipt.utc,Qt::ISODateWithMs).isValid());
    const auto encoded=object.value("raw_frame_base64").toString().toLatin1();
    require(!encoded.isEmpty() && encoded.size()<=static_cast<qsizetype>(breakwater::telemetry::maximum_frame_bytes*4/3+4));
    const auto decoded=QByteArray::fromBase64Encoding(encoded,QByteArray::AbortOnBase64DecodingErrors);
    require(bool(decoded) && decoded.decoded.toBase64()==encoded);
    receipt.frame=decoded.decoded;
    require(!receipt.frame.isEmpty() && receipt.frame.size()<=static_cast<qsizetype>(breakwater::telemetry::maximum_frame_bytes) && receipt.frame.endsWith('\n'));
    return receipt;
}
void validate_journal_end(const QJsonObject& footer) {
    keys(footer,{"type","complete","truncated","termination"});
    require(footer.value("type")=="journal_end" && footer.value("complete").isBool() && footer.value("truncated").isBool());
    const auto reason=footer.value("termination").toString();
    require(reason=="received_end" || reason=="prefix" || reason=="size_limit" || reason=="source_fault");
    require(footer.value("complete").toBool()==(reason=="received_end") &&
            footer.value("truncated").toBool()==(reason=="size_limit"));
}
void ReceiverJournal::reset(const QString& configurationHash,const QString& physicalHash) {
    data_=wire(journal_header(configurationHash,physicalHash)); truncated_=false;
}
void ReceiverJournal::append(const JournalReceipt& receipt) {
    if (truncated_) return;
    const QJsonObject entry{{"type","frame"},{"received_monotonic_s",receipt.monotonic_s},{"received_event_time_s",receipt.event_s},
        {"received_utc",receipt.utc},{"raw_frame_base64",QString::fromLatin1(receipt.frame.toBase64())}};
    (void)parse_journal_receipt(entry);
    const auto serialized=wire(entry);
    if (data_.isEmpty()) throw std::invalid_argument("Register the receiver journal before appending.");
    if (data_.size()+serialized.size()+256>journalByteLimit) { truncated_=true; return; }
    data_+=serialized;
}
QByteArray ReceiverJournal::exportBytes(bool complete,bool sourceFault) const {
    if (data_.isEmpty()) throw std::invalid_argument("There is no local receiver journal to export.");
    const auto reason=truncated_ ? "size_limit" : sourceFault ? "source_fault" : complete ? "received_end" : "prefix";
    const QJsonObject footer{{"type","journal_end"},{"complete",!truncated_ && !sourceFault && complete},
        {"truncated",truncated_},{"termination",reason}};
    validate_journal_end(footer);
    return data_+wire(footer);
}
}
