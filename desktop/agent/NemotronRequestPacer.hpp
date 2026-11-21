#pragma once
#include <QList>
#include <QMutex>
#include <QtTypes>
#include <memory>

namespace breakwater::desktop::agent {
// One process-wide, conservative allowance. No credentials or endpoint data are
// stored here. Clients on different Qt threads share the same monotonic gate.
// Explicit instances isolate tests; separate processes still need coordination.
class NemotronRequestPacer final {
public:
    static std::shared_ptr<NemotronRequestPacer> shared();
    // Zero grants the sole POST lease; positive is a known wait, -1 is busy. FIFO
    // admission prevents a retrying client from starving another waiting client.
    qint64 acquire(const void* client, int minimumIntervalMs);
    void release(const void* client);
    void defer(qint64 milliseconds);
    qint64 cooldownRemainingMs() const;
    // -1 means a POST or another queued request owns admission.
    qint64 admissionDelayMs() const;
private:
    mutable QMutex mutex_;
    QList<const void*> queue_;
    const void* owner_ = nullptr;
    qint64 next_post_ms_ = 0;
    qint64 last_post_ms_ = 0;
    qint64 cooldown_ms_ = 0;
};
}
