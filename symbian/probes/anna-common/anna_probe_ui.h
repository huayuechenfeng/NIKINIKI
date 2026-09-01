#ifndef NIKINIKI_ANNA_PROBE_UI_H
#define NIKINIKI_ANNA_PROBE_UI_H

#include <QtCore/QDir>
#include <QtCore/QString>
#include <QtCore/QStringList>
#include <QtCore/QtGlobal>
#include <QtGui/QApplication>
#include <QtGui/QDesktopWidget>
#include <QtGui/QFont>
#include <QtGui/QLabel>
#include <QtGui/QVBoxLayout>
#include <QtGui/QWidget>

#ifdef Q_OS_SYMBIAN
#include <e32base.h>
#include <hal.h>
#include <hal_data.h>
#include <sysutil.h>
#endif

namespace nikiniki_anna_probe {

inline QString singleLine(QString value)
{
    value.replace(QLatin1Char('\r'), QLatin1Char(' '));
    value.replace(QLatin1Char('\n'), QLatin1Char(' '));
    return value.simplified();
}

inline int ramBytes(HALData::TAttribute attribute)
{
#ifdef Q_OS_SYMBIAN
    TInt value = -1;
    if (HAL::Get(attribute, value) == KErrNone)
        return value;
#else
    Q_UNUSED(attribute);
#endif
    return -1;
}

inline QString megabytes(int bytes)
{
    if (bytes < 0)
        return QString::fromLatin1("unknown");
    return QString::fromLatin1("%1 MB")
        .arg(bytes / (1024 * 1024));
}

inline QString firmwareVersion()
{
#ifdef Q_OS_SYMBIAN
    TBuf<KSysUtilVersionTextLength> value;
    const TInt error = SysUtil::GetSWVersion(value);
    if (error == KErrNone) {
        const QString result = QString::fromUtf16(
            reinterpret_cast<const ushort *>(value.Ptr()),
            value.Length());
        return singleLine(result).left(72);
    }
    return QString::fromLatin1("error %1").arg(error);
#else
    return QString::fromLatin1("host build");
#endif
}

inline QString installDrive()
{
    const QString path = QApplication::applicationFilePath();
    if (path.size() >= 2 && path.at(1) == QLatin1Char(':'))
        return path.left(2).toUpper();
    return QString::fromLatin1("unknown");
}

inline QString screenGeometryText()
{
    const QRect geometry = QApplication::desktop()->screenGeometry();
    return QString::fromLatin1("%1x%2")
        .arg(geometry.width()).arg(geometry.height());
}

inline QStringList baseLines()
{
    QStringList lines;
    lines << QString::fromLatin1("Compile Qt: %1").arg(
                 QString::fromLatin1(QT_VERSION_STR));
    lines << QString::fromLatin1("Runtime Qt: %1").arg(
                 QString::fromLatin1(qVersion()));
#ifdef Q_OS_SYMBIAN
    lines << QString::fromLatin1("Symbian/S60 API: %1 / %2")
                 .arg(static_cast<int>(QSysInfo::symbianVersion()))
                 .arg(static_cast<int>(QSysInfo::s60Version()));
#endif
    lines << QString::fromLatin1("Firmware: %1").arg(firmwareVersion());
    lines << QString::fromLatin1("RAM total/free: %1 / %2")
                 .arg(megabytes(ramBytes(HALData::EMemoryRAM)))
                 .arg(megabytes(ramBytes(HALData::EMemoryRAMFree)));
    lines << QString::fromLatin1("Install drive: %1").arg(installDrive());
    lines << QString::fromLatin1("Screen: %1").arg(screenGeometryText());
    return lines;
}

class ReportWindow : public QWidget
{
public:
    explicit ReportWindow(const QString &windowTitle)
        : QWidget(0), m_label(new QLabel(this))
    {
        setWindowTitle(windowTitle);
        setAttribute(Qt::WA_QuitOnClose, true);
        setStyleSheet(QString::fromLatin1(
            "QWidget { background: #171421; } "
            "QLabel { color: #ffffff; padding: 10px; }"));

        QFont font = m_label->font();
        font.setPixelSize(16);
        font.setBold(true);
        m_label->setFont(font);
        m_label->setAlignment(Qt::AlignLeft | Qt::AlignTop);
        m_label->setWordWrap(true);

        QVBoxLayout *layout = new QVBoxLayout(this);
        layout->setMargin(5);
        layout->addWidget(m_label);
    }

    void setReport(
        const QString &heading,
        const QStringList &lines,
        bool pass)
    {
        const QString colour = pass
            ? QString::fromLatin1("#65e572")
            : QString::fromLatin1("#ffcf5a");
        m_label->setStyleSheet(QString::fromLatin1(
            "QLabel { color: white; padding: 10px; "
            "border: 4px solid %1; }").arg(colour));
        m_label->setText(
            heading + QString::fromLatin1("\n\n") +
            lines.join(QString::fromLatin1("\n")) +
            QString::fromLatin1("\n\nPHOTO THIS SCREEN"));
    }

protected:
    QLabel *m_label;
};

} // namespace nikiniki_anna_probe

#endif
