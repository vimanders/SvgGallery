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

        QPixmap p(size);
        p.fill(Qt::transparent);
        QPainter painter(&p);
        renderer.setAspectRatioMode(Qt::KeepAspectRatio);
        renderer.render(&painter);

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
    const QStringList &pngPaths,
    int iconSize,
    bool customEngine,
    QWidget *parent)
: QWidget(parent)
, m_svgPath(svgPath)
, m_iconSize(iconSize)
, m_customEngine(customEngine)
{
    // Main layout
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(10, 10, 10, 10);
    mainLayout->setSpacing(5);
    
    // Filename label at the top
    QFileInfo fileInfo(svgPath);
    m_filenameLabel = new QLabel(fileInfo.fileName(), this);
    m_filenameLabel->setAlignment(Qt::AlignLeft);
    m_filenameLabel->setStyleSheet("font-weight: bold; font-size: 11px;");
    mainLayout->addWidget(m_filenameLabel);
    
    // Horizontal layout for all icons (SVG + PNGs)
    m_iconsLayout = new QHBoxLayout();
    m_iconsLayout->setSpacing(15);
    
    // Add SVG first
    createIconPair(svgPath, "SVG", m_iconSize, true);
    
    // Add PNGs
    for (const QString &pngPath : pngPaths) {
        QFileInfo pngInfo(pngPath);
        QString baseName = pngInfo.completeBaseName();
        
        // Extract size from filename (e.g., "icon_48" -> 48)
        int pngSize = 32; // default
        QStringList parts = baseName.split('_');
        if (parts.size() >= 2) {
            bool ok;
            int size = parts.last().toInt(&ok);
            if (ok && size > 0) {
                pngSize = size;
            }
        }
        
        // If no size suffix found, try to detect from image
        if (pngSize == 32 && baseName.split('_').size() < 2) {
            QPixmap pixmap(pngPath);
            if (!pixmap.isNull()) {
                pngSize = pixmap.width();
            }
        }
        
        QString label = QString("PNG %1×%1").arg(pngSize);
        createIconPair(pngPath, label, pngSize, false);
    }
    
    m_iconsLayout->addStretch();
    mainLayout->addLayout(m_iconsLayout);
    
    // Widget style
    setStyleSheet(
        "SvgPair {"
        "    background-color: rgba(200, 200, 200, 30);"
        "    border: 1px solid rgba(150, 150, 150, 50);"
        "    border-radius: 5px;"
        "}"
    );
}

void SvgPair::createIconPair(const QString &path, const QString &label, int size, bool isSvg)
{
    IconPair pair;
    pair.originalSize = size;
    
    // Load icon
    QIcon icon;
    if (isSvg && m_customEngine) {
        icon = QIcon(new SvgIconEngine(path));
    } else {
        icon = QIcon(path);
    }
    
    // Create vertical layout for this icon pair
    QVBoxLayout *pairLayout = new QVBoxLayout();
    pairLayout->setSpacing(2);
    
    // Type label (SVG, PNG 32x32, etc.)
    pair.typeLabel = new QLabel(label, this);
    pair.typeLabel->setAlignment(Qt::AlignCenter);
    pair.typeLabel->setStyleSheet("font-size: 9px; font-weight: bold; color: #666;");
    pairLayout->addWidget(pair.typeLabel);
    
    // Horizontal layout for disabled/enabled buttons
    QHBoxLayout *buttonsLayout = new QHBoxLayout();
    buttonsLayout->setSpacing(5);
    
    // Disabled button
    pair.disabledButton = new QToolButton(this);
    pair.disabledButton->setIcon(icon);
    
    // For PNGs, use original size; for SVGs, use current iconSize
    int displaySize = isSvg ? m_iconSize : size;
    pair.disabledButton->setIconSize(QSize(displaySize, displaySize));
    pair.disabledButton->setAutoRaise(true);
    pair.disabledButton->setEnabled(false);
    pair.disabledButton->setFixedSize(displaySize + 4, displaySize + 4);
    
    // Enabled button
    pair.enabledButton = new QToolButton(this);
    pair.enabledButton->setIcon(icon);
    pair.enabledButton->setIconSize(QSize(displaySize, displaySize));
    pair.enabledButton->setAutoRaise(true);
    pair.enabledButton->setEnabled(true);
    pair.enabledButton->setCheckable(true);
    pair.enabledButton->setFixedSize(displaySize + 4, displaySize + 4);
    
    // Disabled label
    pair.disabledLabel = new QLabel(tr("Off"), this);
    pair.disabledLabel->setAlignment(Qt::AlignCenter);
    pair.disabledLabel->setStyleSheet("color: gray; font-size: 8px;");
    
    // Enabled label
    pair.enabledLabel = new QLabel(tr("On"), this);
    pair.enabledLabel->setAlignment(Qt::AlignCenter);
    pair.enabledLabel->setStyleSheet("color: black; font-size: 8px;");
    
    // Add disabled button and label
    QVBoxLayout *disabledLayout = new QVBoxLayout();
    disabledLayout->setSpacing(1);
    disabledLayout->addWidget(pair.disabledButton, 0, Qt::AlignCenter);
    disabledLayout->addWidget(pair.disabledLabel);
    
    // Add enabled button and label
    QVBoxLayout *enabledLayout = new QVBoxLayout();
    enabledLayout->setSpacing(1);
    enabledLayout->addWidget(pair.enabledButton, 0, Qt::AlignCenter);
    enabledLayout->addWidget(pair.enabledLabel);
    
    buttonsLayout->addLayout(disabledLayout);
    buttonsLayout->addLayout(enabledLayout);
    
    pairLayout->addLayout(buttonsLayout);
    
    // Add to main icons layout
    m_iconsLayout->addLayout(pairLayout);
    
    // Store the pair
    m_iconPairs.append(pair);
}

void SvgPair::setIconSize(int size)
{
    m_iconSize = size;
    
    // Update only SVG icons (first in the list)
    // PNGs keep their original size
    for (int i = 0; i < m_iconPairs.size(); ++i) {
        IconPair &pair = m_iconPairs[i];
        
        // First icon is always SVG
        bool isSvg = (i == 0);
        
        if (isSvg) {
            int displaySize = m_iconSize;
            
            pair.disabledButton->setIconSize(QSize(displaySize, displaySize));
            pair.disabledButton->setFixedSize(displaySize + 4, displaySize + 4);
            
            pair.enabledButton->setIconSize(QSize(displaySize, displaySize));
            pair.enabledButton->setFixedSize(displaySize + 4, displaySize + 4);
        }
        // PNGs don't change size - they stay at their original size
    }
    
    updateGeometry();
}
