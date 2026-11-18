// Export only the supplied logo's vector "icon" group, never the wordmark.
// Build with Qt Core/Gui/Svg, then run: generator logo.svg output.ico [preview.png]
#include <QBuffer>
#include <QDataStream>
#include <QGuiApplication>
#include <QIcon>
#include <QImage>
#include <QPainter>
#include <QSaveFile>
#include <QSvgRenderer>
#include <QTextStream>

int main(int argc, char** argv) {
    QGuiApplication app(argc, argv);
    const auto args = app.arguments();
    if (args.size() < 3 || args.size() > 4) return 2;
    QSvgRenderer logo(args[1]);
    const auto bounds = logo.boundsOnElement("icon");
    if (!logo.isValid() || !logo.elementExists("icon") || bounds.isEmpty()) return 2;
    const QList<int> sizes{16, 20, 24, 32, 40, 48, 64, 128, 256};
    QList<QByteArray> frames;
    for (const auto size : sizes) {
        QImage frame(size, size, QImage::Format_ARGB32_Premultiplied);
        frame.fill(Qt::transparent);
        {
            QPainter painter(&frame);
            painter.setRenderHint(QPainter::Antialiasing);
            painter.setPen(Qt::NoPen);
            painter.setBrush(QColor("#15191b"));
            painter.drawRoundedRect(QRectF(0, 0, size, size), size * 0.18, size * 0.18);
            const auto width = size * 0.80, height = width * bounds.height() / bounds.width();
            logo.render(&painter, "icon", QRectF((size - width) / 2, (size - height) / 2, width, height));
        }
        QBuffer png;
        if (!png.open(QIODevice::WriteOnly) || !frame.save(&png, "PNG")) return 3;
        frames.append(png.data());
        if (size == 256 && args.size() == 4 && !frame.save(args[3], "PNG")) return 3;
    }
    QSaveFile file(args[2]);
    if (!file.open(QIODevice::WriteOnly)) return 3;
    QDataStream stream(&file); stream.setByteOrder(QDataStream::LittleEndian);
    stream << quint16(0) << quint16(1) << quint16(sizes.size());
    quint32 offset = 6 + 16 * sizes.size();
    for (qsizetype i = 0; i < sizes.size(); ++i) {
        const auto dimension = quint8(sizes[i] == 256 ? 0 : sizes[i]);
        stream << dimension << dimension << quint8(0) << quint8(0) << quint16(1) << quint16(32)
               << quint32(frames[i].size()) << offset;
        offset += frames[i].size();
    }
    for (const auto& frame : frames) if (stream.writeRawData(frame.constData(), frame.size()) != frame.size()) return 3;
    if (stream.status() != QDataStream::Ok || !file.commit()) return 3;
    const QIcon exported(args[2]);
    if (exported.isNull() || exported.availableSizes().size() != sizes.size()) return 3;
    for (const auto size : sizes) if (exported.pixmap(size, size).isNull()) return 3;
    QTextStream(stdout) << "Exported nine symbol-only Windows icon sizes (16–256 pixels).\n";
    return 0;
}
