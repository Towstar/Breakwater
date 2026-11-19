#pragma once
#include <QByteArray>
#include <QJsonObject>
#include <QString>

namespace breakwater::desktop::telemetry {
inline constexpr qsizetype journalLineLimit = 256*1024*4/3+2048;
inline constexpr qsizetype journalByteLimit = 32*1024*1024;
struct JournalReceipt {
    QByteArray frame;
    double monotonic_s=0,event_s=0;
    QString utc;
};
QJsonObject journal_header(const QString& configurationHash,const QString& physicalHash);
void validate_journal_header(const QJsonObject&,const QString& configurationHash,const QString& physicalHash);
JournalReceipt parse_journal_receipt(const QJsonObject&);
void validate_journal_end(const QJsonObject&);

// A bounded local format, separate from G1 wire frames. Original received bytes
// are base64-encoded without normalization. A frozen export has an explicit footer.
class ReceiverJournal {
public:
    void reset(const QString& configurationHash,const QString& physicalHash);
    void append(const JournalReceipt&);
    QByteArray exportBytes(bool complete,bool sourceFault) const;
    qsizetype size() const { return data_.size(); }
    bool truncated() const { return truncated_; }
private:
    QByteArray data_;
    bool truncated_=false;
};
}
