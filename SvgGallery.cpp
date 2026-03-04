#include "SvgGallery.h"
#include "ScintillaRelay.h"

#include <QCheckBox>
#include <QColorDialog>
#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QPalette>
#include <QPushButton>
#include <QRegularExpression>
#include <QScrollBar>
#include <QSplitter>
#include <QTextStream>
#include <QVBoxLayout>

SvgGallery::SvgGallery(QWidget *parent)
    : QMainWindow(parent)
    , m_currentPath("")
    , m_backgroundColor(QColor(90, 90, 90)) // Medium dark as default
    , m_editorVisible(false)
{
    initUI();
}

void SvgGallery::initUI()
{
    setWindowTitle(tr("SVG Gallery Viewer"));
    setMinimumSize(900, 700);

    m_splitter = new QSplitter(Qt::Horizontal, this);
    setCentralWidget(m_splitter);

    // Left side: Gallery
    QWidget *galleryContainer = new QWidget();
    QVBoxLayout *mainLayout = new QVBoxLayout(galleryContainer);

    // Top controls
    QHBoxLayout *controlsLayout = new QHBoxLayout();

    m_pathInput = new QLineEdit(this);
    m_pathInput->setPlaceholderText(tr("Enter directory path containing SVG files..."));
    connect(m_pathInput, &QLineEdit::returnPressed, this, &SvgGallery::loadSvgs);
    controlsLayout->addWidget(m_pathInput, 3);

    QPushButton *browseBtn = new QPushButton(tr("Browse..."), this);
    connect(browseBtn, &QPushButton::clicked, this, &SvgGallery::browseDirectory);
    controlsLayout->addWidget(browseBtn);

    QPushButton *loadBtn = new QPushButton(tr("Load SVGs"), this);
    connect(loadBtn, &QPushButton::clicked, this, &SvgGallery::loadSvgs);
    controlsLayout->addWidget(loadBtn);

    mainLayout->addLayout(controlsLayout);

    // Filter
    QHBoxLayout *filterLayout = new QHBoxLayout();
    m_filterInput = new QLineEdit(this);
    m_filterInput->setPlaceholderText(tr("Type to filter by filename..."));
    m_filterInput->setClearButtonEnabled(true);
    connect(m_filterInput, &QLineEdit::textChanged, this, &SvgGallery::filterGallery);
    filterLayout->addWidget(m_filterInput, 1);
    filterLayout->addStretch();
    mainLayout->addLayout(filterLayout);

    // Background color presets
    QHBoxLayout *bgPresetsLayout = new QHBoxLayout();

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
        QColor color = QColorDialog::getColor(m_backgroundColor, this, tr("Choose Background Color"));
        if (color.isValid()) {
            m_backgroundColor = color;
            updateBackgroundColor();
        }
    });
    bgPresetsLayout->addWidget(bgColorBtn);

    QCheckBox *ch_customEngine = new QCheckBox(tr("Custom engine"));
    ch_customEngine->setToolTip(tr("Toggle and reload if icons scale incorrectly."));
    ch_customEngine->setChecked(true);
    connect(ch_customEngine, &QCheckBox::clicked, this, [this](bool checked){
        m_customEngine = checked;
    });
    bgPresetsLayout->addWidget(ch_customEngine);

    bgPresetsLayout->addStretch();
    mainLayout->addLayout(bgPresetsLayout);

    // Icon size controls
    auto setIconSize = [this](int size) {
        m_iconSize = size;
        m_sizeSlider->setValue(m_iconSize);
        m_sizeLabel->setText(tr("%1×%1 px").arg(m_iconSize));
        updateIconSizes();
    };

    QHBoxLayout *sizeControlsLayout = new QHBoxLayout();

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
    m_scrollArea = new QScrollArea(this);
    m_scrollArea->setWidgetResizable(true);
    m_scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);

    // Gallery widget
    m_galleryWidget = new QWidget();
    m_galleryLayout = new QGridLayout(m_galleryWidget);
    m_galleryLayout->setSpacing(15);
    m_galleryLayout->setAlignment(Qt::AlignTop | Qt::AlignLeft);

    m_scrollArea->setWidget(m_galleryWidget);
    mainLayout->addWidget(m_scrollArea);

    m_splitter->addWidget(galleryContainer);

    // Right side: Editor
    m_editorContainer = new QWidget();
    QVBoxLayout *editorLayout = new QVBoxLayout(m_editorContainer);
    editorLayout->setContentsMargins(0, 0, 0, 0);

    // Header
    QHBoxLayout *editorHeaderLayout = new QHBoxLayout();
    m_editorTitle = new QLabel(tr("SVG Source"), this);
    m_editorTitle->setStyleSheet("font-weight: bold; padding: 5px;");
    editorHeaderLayout->addWidget(m_editorTitle);

    editorHeaderLayout->addStretch();

    m_saveButton = new QPushButton(tr("Save && Reload"), this);
    m_saveButton->setToolTip(tr("Save changes and reload SVG"));
    connect(m_saveButton, &QPushButton::clicked, this, &SvgGallery::saveSvgContent);
    editorHeaderLayout->addWidget(m_saveButton);

    QPushButton *closeEditorBtn = new QPushButton("×", this);
    closeEditorBtn->setFixedSize(24, 24);
    closeEditorBtn->setStyleSheet("font-size: 18px; font-weight: bold;");
    closeEditorBtn->setToolTip(tr("Close editor"));
    connect(closeEditorBtn, &QPushButton::clicked, this, &SvgGallery::closeEditor);
    editorHeaderLayout->addWidget(closeEditorBtn);

    editorLayout->addLayout(editorHeaderLayout);

    m_editor = nullptr;

    QLabel *placeholder = new QLabel(tr("Double-click an SVG to view source"), this);
    placeholder->setAlignment(Qt::AlignCenter);
    placeholder->setStyleSheet("color: gray; font-style: italic;");
    editorLayout->addWidget(placeholder);

    m_splitter->addWidget(m_editorContainer);

    m_editorContainer->hide();
    m_splitter->setSizes({800, 0});

    updateBackgroundColor();
}

void SvgGallery::browseDirectory()
{
    QString directory = QFileDialog::getExistingDirectory(
        this,
        tr("Select Directory Containing SVG Files"),
        m_currentPath.isEmpty() ? QDir::homePath() : m_currentPath);

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

    // Calculate text color based on background brightness
    updateTextColors();
}

void SvgGallery::updateTextColors()
{
    // Calculate relative luminance using the formula from WCAG
    // https://www.w3.org/TR/WCAG20/#relativeluminancedef
    qreal r = m_backgroundColor.redF();
    qreal g = m_backgroundColor.greenF();
    qreal b = m_backgroundColor.blueF();

    // Convert to linear RGB
    auto toLinear = [](qreal c) {
        return c <= 0.03928 ? c / 12.92 : qPow((c + 0.055) / 1.055, 2.4);
    };

    qreal luminance = 0.2126 * toLinear(r) + 0.7152 * toLinear(g) + 0.0722 * toLinear(b);
    QColor textColor = luminance > 0.5 ? Qt::black : Qt::white;

    for (SvgPair *widget : m_svgPairs) {
        widget->setTextColor(textColor);
    }
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

    QStringList svgFiles = dir.entryList(QStringList("*.svg"), QDir::Files, QDir::Name);
    if (svgFiles.isEmpty()) {
        m_infoLabel->setText(tr("No SVG files found in: %1").arg(path));
        m_infoLabel->setStyleSheet("padding: 5px; background-color: #4a4020; color: #ffffcc; border-radius: 3px;");
        return;
    }

    // Show initial progress
    m_infoLabel->setText(tr("Loading %1 SVG file(s)...").arg(svgFiles.size()));
    m_infoLabel->setStyleSheet("padding: 5px; background-color: #3a3a3a; color: #e0e0e0; border-radius: 3px;");
    QCoreApplication::processEvents();

    QStringList allPngFiles = dir.entryList(QStringList("*.png"), QDir::Files, QDir::Name);
    clearGallery();

    // Process each SVG and find its corresponding PNGs
    int totalPngsFound = 0;

    for (int index = 0; index < svgFiles.size(); ++index) {
        // Update progress
        m_infoLabel->setText(tr("Loading %1 of %2: %3")
                                 .arg(index + 1)
                                 .arg(svgFiles.size())
                                 .arg(svgFiles[index]));
        QCoreApplication::processEvents();

        QString svgFile = svgFiles[index];
        QString svgPath = dir.absoluteFilePath(svgFile);
        QFileInfo svgInfo(svgFile);
        QString baseName = svgInfo.completeBaseName(); // e.g., "icon" from "icon.svg"

        // Find matching PNGs
        QStringList matchingPngs;
        QRegularExpression pngPattern(QString("^%1(_\\d+)?\\.png$").arg(QRegularExpression::escape(baseName)));

        for (const QString &pngFile : allPngFiles) {
            if (pngPattern.match(pngFile).hasMatch()) {
                matchingPngs.append(dir.absoluteFilePath(pngFile));
            }
        }

        totalPngsFound += matchingPngs.size();

        // Create widget with SVG and its PNGs (one row per SVG)
        SvgPair *svgWidget = new SvgPair(svgPath, matchingPngs, m_iconSize, m_customEngine, this);
        connect(svgWidget, &SvgPair::doubleClicked, this, &SvgGallery::showSvgContent);

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

    // Update text colors for all loaded widgets
    updateTextColors();
}

void SvgGallery::updateIconSizes()
{
    if (m_svgPairs.isEmpty())
        return;

    // Calculate the relative scroll position before resize
    QScrollBar *vScrollBar = m_scrollArea->verticalScrollBar();
    double scrollRatio = 0.0;
    if (vScrollBar->maximum() > 0) {
        scrollRatio = static_cast<double>(vScrollBar->value()) / vScrollBar->maximum();
    }

    // Update icon sizes
    for (SvgPair *widget : m_svgPairs)
        widget->setIconSize(m_iconSize);

    // Force layout update
    m_galleryWidget->updateGeometry();
    QCoreApplication::processEvents();

    // Restore the relative scroll position
    if (vScrollBar->maximum() > 0) {
        vScrollBar->setValue(static_cast<int>(scrollRatio * vScrollBar->maximum()));
    }
}

void SvgGallery::filterGallery()
{
    QString filterText = m_filterInput->text().trimmed();

    int visibleCount = 0;

    // Show/hide widgets based on filter
    for (SvgPair *widget : m_svgPairs) {
        QFileInfo fileInfo(widget->svgPath());
        bool matches = filterText.isEmpty() || fileInfo.fileName().contains(filterText, Qt::CaseInsensitive);
        widget->setVisible(matches);
        if (matches) visibleCount++;
    }

    // Update info label with filter results
    if (!filterText.isEmpty()) {
        m_infoLabel->setText(tr("Showing %1 of %2 items matching '%3'")
                                 .arg(visibleCount).arg(m_svgPairs.size()).arg(filterText));
        m_infoLabel->setStyleSheet("padding: 5px; background-color: #3a4a4a; color: #ccddff; border-radius: 3px;");
    } else if (!m_svgPairs.isEmpty()) {
        // Restore original loaded message
        m_infoLabel->setText(tr("Showing all %1 items").arg(m_svgPairs.size()));
        m_infoLabel->setStyleSheet("padding: 5px; background-color: #2a4a2a; color: #ccffcc; border-radius: 3px;");
    }
}

void SvgGallery::setupScintilla()
{
    if (!m_editor || !m_editor->isAvailable()) {
        qDebug() << "setupScintilla: Editor not available";
        return;
    }

    auto RGB = [](int r, int g, int b) -> unsigned long {
        return ((unsigned long)(((unsigned char)(r)|((unsigned short)((unsigned char)(g))<<8))|(((unsigned long)(unsigned char)(b))<<16)));
    };

    // Default style
    m_editor->styleSetFore(32, RGB(169, 183, 198));
    m_editor->styleSetBack(32, RGB(43, 43, 43));
    m_editor->styleSetBold(32, false);
    m_editor->styleSetSize(32, 10);
    m_editor->styleSetFont(32, "Courier New");
    m_editor->styleClearAll();

    m_editor->setTabWidth(2);

    // Line numbers
    m_editor->setMarginTypeN(0, 1);
    m_editor->setMarginWidthN(0, 40);
    m_editor->setMarginWidthN(1, 0);

    // Caret and selection
    m_editor->setCaretFore(RGB(255, 255, 255));
    m_editor->setSelBack(true, RGB(33, 66, 131));

    // Word wrap with indent preservation
    m_editor->setWrapMode(ScintillaRelay::WrapWord);              // 単語境界でラップ
    m_editor->setWrapIndentMode(ScintillaRelay::WrapIndentSame);  // 元の行と同じインデント
    m_editor->setWrapVisualFlags(ScintillaRelay::WrapVisualFlagEnd);  // 行末にインジケータ表示
}

void SvgGallery::applyXMLHighlighting()
{
    if (!m_editor || !m_editor->isAvailable()) {
        qDebug() << "applyXMLHighlighting: Editor not available";
        return;
    }

    if (!m_editor->isLexillaAvailable()) {
        qDebug() << "applyXMLHighlighting: Lexilla not available, skipping syntax highlighting";
        return;
    }

    if (!m_editor->setLexer("xml")) {
        qDebug() << "Failed to set XML lexer";
        return;
    }

    m_editor->setKeyWords(0,
        "svg path rect circle ellipse line polyline polygon text tspan "
        "g defs use symbol clipPath mask pattern linearGradient radialGradient "
        "stop filter feGaussianBlur feOffset feBlend feColorMatrix "
        "animate animateTransform");

    m_editor->setKeyWords(1,
        "xmlns width height viewBox x y x1 y1 x2 y2 cx cy r rx ry "
        "d fill stroke stroke-width opacity fill-opacity stroke-opacity "
        "transform translate rotate scale matrix id class style "
        "points stroke-linecap stroke-linejoin stroke-dasharray "
        "gradientTransform gradientUnits offset stop-color stop-opacity");

    auto RGB = [](int r, int g, int b) {
        return ((unsigned long)(((unsigned char)(r)|((unsigned short)((unsigned char)(g))<<8))|(((unsigned long)(unsigned char)(b))<<16)));
    };

    m_editor->styleSetFore(0, RGB(169, 183, 198));
    m_editor->styleSetFore(1, RGB(80, 179, 124));
    m_editor->styleSetBold(1, true);
    m_editor->styleSetFore(2, RGB(80, 179, 124));
    m_editor->styleSetFore(3, RGB(170, 118, 152));
    m_editor->styleSetFore(4, RGB(170, 118, 152));
    m_editor->styleSetFore(5, RGB(104, 151, 187));
    m_editor->styleSetFore(6, RGB(89, 135, 106));
    m_editor->styleSetFore(7, RGB(89, 135, 106));
    m_editor->styleSetFore(8, RGB(169, 183, 198));
    m_editor->styleSetFore(9, RGB(128, 128, 128));
    m_editor->styleSetItalic(9, true);
    m_editor->styleSetFore(10, RGB(104, 151, 187));
    m_editor->styleSetFore(11, RGB(80, 179, 124));
    m_editor->styleSetBold(11, true);
    m_editor->styleSetFore(12, RGB(204, 120, 50));

    m_editor->colourise(0, -1);
    qDebug() << "XML syntax highlighting applied";
}

void SvgGallery::showSvgContent(const QString &svgPath)
{
    m_currentSvgPath = svgPath;

    if (!m_editor) {
        m_editor = new ScintillaRelay(m_editorContainer);
        QLayoutItem *item = m_editorContainer->layout()->itemAt(1);
        if (item && item->widget()) {
            item->widget()->deleteLater();
        }
        qobject_cast<QVBoxLayout*>(m_editorContainer->layout())->addWidget(m_editor);
        setupScintilla();
    }

    QFile file(svgPath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qDebug() << "Failed to open SVG file:" << svgPath;
        return;
    }

    QString content = file.readAll();
    file.close();

    QFileInfo fileInfo(svgPath);
    m_editorTitle->setText(tr("SVG Source: %1").arg(fileInfo.fileName()));

    // Always editable
    m_editor->setReadOnly(false);
    m_editor->clearAll();

    QByteArray utf8 = content.toUtf8();
    m_editor->insertText(0, utf8.constData());

    applyXMLHighlighting();
    m_editor->gotoPos(0);

    if (!m_editorVisible) {
        m_editorContainer->show();
        m_splitter->setSizes({500, 400});
        m_editorVisible = true;
    }
}

void SvgGallery::saveSvgContent()
{
    if (!m_editor || m_currentSvgPath.isEmpty()) {
        qDebug() << "No SVG to save";
        return;
    }

    QByteArray content = m_editor->getText();

    QFile file(m_currentSvgPath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        m_infoLabel->setText(tr("Error: Failed to save %1").arg(QFileInfo(m_currentSvgPath).fileName()));
        m_infoLabel->setStyleSheet("padding: 5px; background-color: #4a2020; color: #ffcccc; border-radius: 3px;");
        return;
    }

    file.write(content);
    file.close();

    m_infoLabel->setText(tr("Saved and reloading: %1").arg(QFileInfo(m_currentSvgPath).fileName()));
    m_infoLabel->setStyleSheet("padding: 5px; background-color: #2a4a2a; color: #ccffcc; border-radius: 3px;");

    reloadCurrentSvg();
}

void SvgGallery::reloadCurrentSvg()
{
    if (m_currentSvgPath.isEmpty()) return;

    for (SvgPair *widget : m_svgPairs) {
        if (widget->svgPath() == m_currentSvgPath) {
            widget->reloadSvg();
            break;
        }
    }

    showSvgContent(m_currentSvgPath);
}

void SvgGallery::closeEditor()
{
    m_editorContainer->hide();
    m_splitter->setSizes({width(), 0});
    m_editorVisible = false;
    m_currentSvgPath.clear();
}
