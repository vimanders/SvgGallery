#include "SvgPair.h"
#include <QByteArray>
#include <QFileInfo>
#include <QIcon>
#include <QIconEngine>
#include <QPainter>
#include <QPixmap>
#include <QSvgRenderer>
#include <QTextStream>

struct SvgIconEngine : QIconEngine
{
    QByteArray svg; // SVG text encoded in UTF-8

    SvgIconEngine() = default;
    SvgIconEngine(QByteArray const& svg) : svg(svg) {}
    SvgIconEngine(QString const& path)
    {
        QFile f(path);
        if (!f.open(QIODeviceBase::Text | QIODeviceBase::ReadOnly))
            return;

        QTextStream t(&f);
        QString s = t.readAll();
        if (!s.contains("</svg>", Qt::CaseInsensitive))
            return;

        svg = s.toUtf8();
    }

    QPixmap pixmap(
        QSize const& size,
        QIcon::Mode mode,
        QIcon::State state) override
    {
        QSvgRenderer renderer(svg);
        if (!renderer.isValid())
            return {};

        // QPixmap has a loadFromData() method which uses the
        // SVG size as pixelsize. I'd rather suggest to use the
        // QSvgRenderer instead to have control over the pixel
        // size of the rendered SVG.
        QPixmap p(size);
        p.fill(Qt::transparent);
        QPainter painter(&p);
        renderer.setAspectRatioMode(Qt::KeepAspectRatio);
        renderer.render(&painter);

        // Pixmap -> Icon -> Pixmap to honor mode and state as
        // color transformations are performed on pixel level
        QIcon icon(p);
        return icon.pixmap(size, mode, state);
    }

    void paint(
        QPainter* painter,
        QRect const& rect,
        QIcon::Mode mode,
        QIcon::State state) override
    {
        QPixmap p = pixmap(rect.size(), mode, state);
        painter->drawPixmap(rect, p);
    }

    QIconEngine* clone() const override {
        return new SvgIconEngine(svg);
    }
};

SvgPair::SvgPair(
    const QString &svgPath,
    int iconSize,
    bool customEngine,
    QWidget *parent)
: QWidget(parent)
, m_svgPath(svgPath)
, m_iconSize(iconSize)
{
    // Main layout
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(10, 10, 10, 10);
    mainLayout->setSpacing(5);
    
    // Horizontal layout for the two buttons
    QHBoxLayout *buttonsLayout = new QHBoxLayout();
    buttonsLayout->setSpacing(10);
    
    // Load the SVG via file (scales correctly in IDE)
    // or custom engine (scales always correctly)
    QIcon icon = customEngine ? QIcon(new SvgIconEngine(svgPath)) : QIcon(svgPath);

    // Disabled button
    m_disabledButton = new QToolButton(this);
    m_disabledButton->setIcon(icon);
    m_disabledButton->setIconSize(QSize(m_iconSize, m_iconSize));
    m_disabledButton->setAutoRaise(true);
    m_disabledButton->setEnabled(false);
    m_disabledButton->setFixedSize(m_iconSize + 4, m_iconSize + 4);
    
    // Enabled button
    m_enabledButton = new QToolButton(this);
    m_enabledButton->setIcon(icon);
    m_enabledButton->setIconSize(QSize(m_iconSize, m_iconSize));
    m_enabledButton->setAutoRaise(true);
    m_enabledButton->setEnabled(true);
    m_enabledButton->setFixedSize(m_iconSize + 4, m_iconSize + 4);
    m_enabledButton->setCheckable(true);
    
    // Disabled label
    m_disabledLabel = new QLabel(tr("Disabled"), this);
    m_disabledLabel->setAlignment(Qt::AlignCenter);
    m_disabledLabel->setStyleSheet("color: gray; font-size: 9px;");
    
    // Enabled label
    m_enabledLabel = new QLabel(tr("Enabled"), this);
    m_enabledLabel->setAlignment(Qt::AlignCenter);
    m_enabledLabel->setStyleSheet("color: black; font-size: 9px;");
    
    // Add disabled button and its label
    QVBoxLayout *disabledLayout = new QVBoxLayout();
    disabledLayout->setSpacing(2);
    disabledLayout->addWidget(m_disabledButton, 0, Qt::AlignCenter);
    disabledLayout->addWidget(m_disabledLabel);
    
    // Add enabled button and its label
    QVBoxLayout *enabledLayout = new QVBoxLayout();
    enabledLayout->setSpacing(2);
    enabledLayout->addWidget(m_enabledButton, 0, Qt::AlignCenter);
    enabledLayout->addWidget(m_enabledLabel);
    
    buttonsLayout->addLayout(disabledLayout);
    buttonsLayout->addLayout(enabledLayout);
    
    mainLayout->addLayout(buttonsLayout);
    
    // Filename label
    QFileInfo fileInfo(svgPath);
    m_filenameLabel = new QLabel(fileInfo.fileName(), this);
    m_filenameLabel->setAlignment(Qt::AlignCenter);
    m_filenameLabel->setWordWrap(true);
    m_filenameLabel->setMaximumWidth((m_iconSize * 2) + 40);
    m_filenameLabel->setStyleSheet("font-size: 10px;");
    
    mainLayout->addWidget(m_filenameLabel);
    
    // Widget style
    setStyleSheet(
        "SvgPair {"
        "    background-color: rgba(200, 200, 200, 30);"
        "    border: 1px solid rgba(150, 150, 150, 50);"
        "    border-radius: 5px;"
        "}"
    );
}

void SvgPair::setIconSize(int size)
{
    m_iconSize = size;
    
    m_disabledButton->setIconSize(QSize(m_iconSize, m_iconSize));
    m_disabledButton->setFixedSize(m_iconSize + 4, m_iconSize + 4);
    
    m_enabledButton->setIconSize(QSize(m_iconSize, m_iconSize));
    m_enabledButton->setFixedSize(m_iconSize + 4, m_iconSize + 4);
    
    m_filenameLabel->setMaximumWidth((m_iconSize * 2) + 40);
    
    updateGeometry();
}
