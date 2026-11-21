#include "NemotronRequestPacer.hpp"
#include <QMutexLocker>
#include <algorithm>
#include <chrono>
#include <limits>

namespace breakwater::desktop::agent {
namespace {
qint64 nowMs() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
}
}
std::shared_ptr<NemotronRequestPacer> NemotronRequestPacer::shared() {
    static const auto pacer = std::make_shared<NemotronRequestPacer>();
    return pacer;
}
qint64 NemotronRequestPacer::acquire(const void* client, int minimumIntervalMs) {
    QMutexLocker lock(&mutex_);
    if (!queue_.contains(client)) queue_.append(client);
    if (owner_ || queue_.first() != client) return -1;
    const auto now = nowMs();
    const auto interval = minimumIntervalMs + (minimumIntervalMs > 0 ? 1 : 0);
    const auto wait = std::max({next_post_ms_, last_post_ms_ + interval, cooldown_ms_}) - now;
    if (wait > 0) return wait;
    queue_.removeFirst(); owner_ = client;
    next_post_ms_ = now + interval;
    last_post_ms_ = now;
    return 0;
}
void NemotronRequestPacer::release(const void* client) {
    QMutexLocker lock(&mutex_);
    if (owner_ == client) owner_ = nullptr;
    queue_.removeAll(client);
}
void NemotronRequestPacer::defer(qint64 milliseconds) {
    QMutexLocker lock(&mutex_);
    const auto now = nowMs();
    // Excessive server waits remain prohibitive; never clip them into an early
    // retry. qint64 saturation is far beyond any supported investigation budget.
    // Round the boundary outward, avoiding a sub-millisecond early admission
    // when this clock's millisecond conversion floors a time point.
    const auto until = now + std::min(std::max<qint64>(0, milliseconds), std::numeric_limits<qint64>::max() / 4) + (milliseconds > 0 ? 1 : 0);
    cooldown_ms_ = std::max(cooldown_ms_, until);
}
qint64 NemotronRequestPacer::cooldownRemainingMs() const {
    QMutexLocker lock(&mutex_);
    return std::max<qint64>(0, cooldown_ms_ - nowMs());
}
qint64 NemotronRequestPacer::admissionDelayMs() const {
    QMutexLocker lock(&mutex_);
    return owner_ || !queue_.isEmpty() ? -1 : std::max<qint64>(0, std::max(next_post_ms_, cooldown_ms_) - nowMs());
}
}
