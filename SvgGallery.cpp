#include "SvgGallery.h"

#include <QCheckBox>
#include <QColorDialog>
#include <QDebug>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QPalette>
#include <QPushButton>
#include <QRegularExpression>
#include <QScrollArea>
#include <QVBoxLayout>

SvgGallery::SvgGallery(QWidget *parent)
    : QMainWindow(parent)
    , m_currentPath("")
    , m_backgroundColor(QColor(90, 90, 90))  // Medium dark as default
{
    initUI();
}

void SvgGallery::initUI()
{
    setWindowTitle(tr("SVG Gallery Viewer"));
    setMinimumSize(900, 700);
    
    // Central widget
    QWidget *centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);
    QVBoxLayout *mainLayout = new QVBoxLayout(centralWidget);
    
    // Top controls - Path and Load
    QHBoxLayout *controlsLayout = new QHBoxLayout();
    
    // Path input
    m_pathInput = new QLineEdit(this);
    m_pathInput->setPlaceholderText(tr("Enter directory path containing SVG files..."));
    connect(m_pathInput, &QLineEdit::returnPressed, this, &SvgGallery::loadSvgs);
    controlsLayout->addWidget(m_pathInput, 3);
    
    // Browse button
    QPushButton *browseBtn = new QPushButton(tr("Browse..."), this);
    connect(browseBtn, &QPushButton::clicked, this, &SvgGallery::browseDirectory);
    controlsLayout->addWidget(browseBtn);
    
    // Load button
    QPushButton *loadBtn = new QPushButton(tr("Load SVGs"), this);
    connect(loadBtn, &QPushButton::clicked, this, &SvgGallery::loadSvgs);
    controlsLayout->addWidget(loadBtn);
    
    mainLayout->addLayout(controlsLayout);
    
    // Background color presets row
    QHBoxLayout *bgPresetsLayout = new QHBoxLayout();
    QLabel *bgLabel = new QLabel(tr("Background:"), this);
    bgPresetsLayout->addWidget(bgLabel);
    
    QPushButton *bgPresetNative = new QPushButton(tr("Native"), this);
    bgPresetNative->setToolTip("RGB(236, 236, 236)");
    connect(bgPresetNative, &QPushButton::clicked, this, [this] {
        m_backgroundColor = QColor(236, 236, 236);
        updateBackgroundColor();
    });
    bgPresetsLayout->addWidget(bgPresetNative);

    QPushButton *bgPresetBright = new QPushButton(tr("Bright"), this);
    bgPresetBright->setToolTip("RGB(110, 110, 110)");
    connect(bgPresetBright, &QPushButton::clicked, this, [this] {
        m_backgroundColor = QColor(110, 110, 110);
        updateBackgroundColor();
    });
    bgPresetsLayout->addWidget(bgPresetBright);

    QPushButton *bgPresetMedium = new QPushButton(tr("Medium (Default)"), this);
    bgPresetMedium->setToolTip("RGB(90, 90, 90)");
    connect(bgPresetMedium, &QPushButton::clicked, this, [this] {
        m_backgroundColor = QColor(90, 90, 90);
        updateBackgroundColor();
    });
    bgPresetsLayout->addWidget(bgPresetMedium);
    
    QPushButton *bgPresetDark = new QPushButton(tr("Dark"), this);
    bgPresetDark->setToolTip("RGB(40, 40, 40)");
    connect(bgPresetDark, &QPushButton::clicked, this, [this] {
        m_backgroundColor = QColor(40, 40, 40);
        updateBackgroundColor();
    });
    bgPresetsLayout->addWidget(bgPresetDark);
    
    QPushButton *bgColorBtn = new QPushButton(tr("Custom Color..."), this);
    connect(bgColorBtn, &QPushButton::clicked, this, [this]{
        QColor color = QColorDialog::getColor(
            m_backgroundColor,
            this,
            tr("Choose Background Color"));

        if (color.isValid()) {
            m_backgroundColor = color;
            updateBackgroundColor();
        }
    });
    bgPresetsLayout->addWidget(bgColorBtn);

    QCheckBox *ch_customEngine = new QCheckBox(tr("Custom engine"));
    ch_customEngine->setToolTip(tr(
        "Toggle and reload if icons scale incorrectly.\n\n"
        "The Qt documentation states that the icon engine is determined by the\n"
        "first image format to be added to an icon. SVGs are scaled correctly\n"
        "when starting this tool from an IDE but apparently does not so when\n"
        "starting as a standalone application. I guess I'm missing a library?\n"
        "Anyway, the custom engine should ensure correct scaling."));
    ch_customEngine->setChecked(true);
    connect(ch_customEngine, &QCheckBox::clicked, this, [this](bool checked){
        m_customEngine = checked;
    });
    bgPresetsLayout->addWidget(ch_customEngine);

    bgPresetsLayout->addStretch();
    mainLayout->addLayout(bgPresetsLayout);
    
    // Icon size controls row    
    auto setIconSize = [this](int size) {
        m_iconSize = size;
        m_sizeSlider->setValue(m_iconSize);
        m_sizeLabel->setText(tr("%1×%1 px").arg(m_iconSize));
        updateIconSizes();
    };

    QHBoxLayout *sizeControlsLayout = new QHBoxLayout();
    QLabel *sizeControlLabel = new QLabel(tr("Icon Size:"), this);
    sizeControlsLayout->addWidget(sizeControlLabel);
    
    QPushButton *sizePreset1 = new QPushButton("32×32", this);
    connect(sizePreset1, &QPushButton::clicked, this, [setIconSize]{setIconSize(32);});
    sizeControlsLayout->addWidget(sizePreset1);
    
    QPushButton *sizePreset2 = new QPushButton("48×48", this);
    connect(sizePreset2, &QPushButton::clicked, this, [setIconSize]{setIconSize(48);});
    sizeControlsLayout->addWidget(sizePreset2);
    
    QPushButton *sizePreset3 = new QPushButton("64×64", this);
    connect(sizePreset3, &QPushButton::clicked, this, [setIconSize]{setIconSize(64);});
    sizeControlsLayout->addWidget(sizePreset3);
    
    // Size slider
    m_sizeSlider = new QSlider(Qt::Horizontal, this);
    m_sizeSlider->setMinimum(16);
    m_sizeSlider->setMaximum(128);
    m_sizeSlider->setValue(m_iconSize);
    m_sizeSlider->setTickPosition(QSlider::TicksBelow);
    m_sizeSlider->setTickInterval(16);
    connect(m_sizeSlider, &QSlider::valueChanged, this, [this](int size) {
        m_iconSize = size;
        m_sizeLabel->setText(tr("%1×%1 px").arg(m_iconSize));
        updateIconSizes();
    });
    sizeControlsLayout->addWidget(m_sizeSlider, 2);
    
    // Size label
    m_sizeLabel = new QLabel(tr("%1×%1 px").arg(m_iconSize), this);
    m_sizeLabel->setMinimumWidth(70);
    sizeControlsLayout->addWidget(m_sizeLabel);
    
    sizeControlsLayout->addStretch();
    mainLayout->addLayout(sizeControlsLayout);
    
    // Info label
    m_infoLabel = new QLabel(tr(
        "Load a directory to view SVG files. "
        "Each icon is shown in both disabled and enabled states."),
        this);
    m_infoLabel->setStyleSheet("padding: 5px; background-color: #3a3a3a; color: #e0e0e0; border-radius: 3px;");
    mainLayout->addWidget(m_infoLabel);
    
    // Scroll area for gallery
    QScrollArea *scrollArea = new QScrollArea(this);
    scrollArea->setWidgetResizable(true);
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    
    // Gallery widget
    m_galleryWidget = new QWidget();
    m_galleryLayout = new QGridLayout(m_galleryWidget);
    m_galleryLayout->setSpacing(15);
    m_galleryLayout->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    
    scrollArea->setWidget(m_galleryWidget);
    mainLayout->addWidget(scrollArea);
    
    // Set initial background
    updateBackgroundColor();
}

void SvgGallery::browseDirectory()
{
    QString directory = QFileDialog::getExistingDirectory(
        this,
        tr("Select Directory Containing SVG Files"),
        m_currentPath.isEmpty() ? QDir::homePath() : m_currentPath
    );
    
    if (!directory.isEmpty()) {
        m_pathInput->setText(directory);
        loadSvgs();
    }
}

void SvgGallery::updateBackgroundColor()
{
    QPalette palette = m_galleryWidget->palette();
    palette.setColor(QPalette::Window, m_backgroundColor);
    m_galleryWidget->setAutoFillBackground(true);
    m_galleryWidget->setPalette(palette);
}

void SvgGallery::clearGallery()
{
    for (SvgPair *widget : m_svgPairs)
        widget->deleteLater();
    m_svgPairs.clear();
}

void SvgGallery::loadSvgs()
{
    QString path = m_pathInput->text().trimmed();
    
    if (path.isEmpty()) {
        m_infoLabel->setText(tr("Please enter a directory path."));
        m_infoLabel->setStyleSheet("padding: 5px; background-color: #4a2020; color: #ffcccc; border-radius: 3px;");
        return;
    }
    
    QDir dir(path);
    
    if (!dir.exists()) {
        m_infoLabel->setText(tr("Error: Directory does not exist: %1").arg(path));
        m_infoLabel->setStyleSheet("padding: 5px; background-color: #4a2020; color: #ffcccc; border-radius: 3px;");
        return;
    }
    
    // Find all SVG files
    QStringList svgFilters;
    svgFilters << "*.svg";
    QStringList svgFiles = dir.entryList(svgFilters, QDir::Files, QDir::Name);
    
    if (svgFiles.isEmpty()) {
        m_infoLabel->setText(tr("No SVG files found in: %1").arg(path));
        m_infoLabel->setStyleSheet("padding: 5px; background-color: #4a4020; color: #ffffcc; border-radius: 3px;");
        return;
    }
    
    // Find all PNG files
    QStringList pngFilters;
    pngFilters << "*.png";
    QStringList allPngFiles = dir.entryList(pngFilters, QDir::Files, QDir::Name);
    
    // Clear existing gallery
    clearGallery();
    
    // Process each SVG and find its corresponding PNGs
    int totalPngsFound = 0;
    
    for (int index = 0; index < svgFiles.size(); ++index) {
        QString svgFile = svgFiles[index];
        QString svgPath = dir.absoluteFilePath(svgFile);
        QFileInfo svgInfo(svgFile);
        QString baseName = svgInfo.completeBaseName(); // e.g., "icon" from "icon.svg"
        
        // Find matching PNGs
        QStringList matchingPngs;
        QRegularExpression pngPattern(QString("^%1(_\\d+)?\\.png$").arg(QRegularExpression::escape(baseName)));
        
        for (const QString &pngFile : allPngFiles) {
            QRegularExpressionMatch match = pngPattern.match(pngFile);
            if (match.hasMatch()) {
                matchingPngs.append(dir.absoluteFilePath(pngFile));
            }
        }
        
        totalPngsFound += matchingPngs.size();
        
        // Create widget with SVG and its PNGs (one row per SVG)
        SvgPair *svgWidget = new SvgPair(svgPath, matchingPngs, m_iconSize, m_customEngine, this);
        
        // Add to layout - one widget per row (column 0, spanning all columns)
        m_galleryLayout->addWidget(svgWidget, index, 0);
        m_svgPairs.append(svgWidget);
    }
    
    m_currentPath = path;
    QString message = tr("Loaded %1 SVG file(s)").arg(svgFiles.size());
    if (totalPngsFound > 0) {
        message += tr(" with %1 corresponding PNG(s)").arg(totalPngsFound);
    }
    message += tr(" from: %1").arg(path);
    
    m_infoLabel->setText(message);
    m_infoLabel->setStyleSheet("padding: 5px; background-color: #2a4a2a; color: #ccffcc; border-radius: 3px;");
}

void SvgGallery::updateIconSizes()
{
    for (SvgPair *widget : m_svgPairs)
        widget->setIconSize(m_iconSize);
}
