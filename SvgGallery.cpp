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
#include <QStandardPaths>
#include <QTextStream>
#include <QVBoxLayout>

SvgGallery::SvgGallery(QWidget *parent)
    : QMainWindow(parent)
    , m_currentPath("")
    , m_backgroundColor(QColor(90, 90, 90)) // Medium dark as default
    , m_editorVisible(false)
{
    initUI();
    QCoreApplication::setAttribute(Qt::AA_SynthesizeMouseForUnhandledTouchEvents);
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
    ch_customEngine->setChecked(false);
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
    m_infoLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
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
#ifdef Q_OS_ANDROID
    if (!m_androidFolder) {
        m_androidFolder = new AndroidFolder(this);
        connect(m_androidFolder, &AndroidFolder::ready, this, [this](bool ok) {
            if (ok) {
                m_pathInput->setText(m_androidFolder->treeUri());
                loadSvgs();
            }
        });
    }
    m_androidFolder->openDialog();
#else
    QString directory = QFileDialog::getExistingDirectory(
        this,
        tr("Select Directory Containing SVG Files"),
        m_currentPath.isEmpty() ? QDir::homePath() : m_currentPath);

    if (!directory.isEmpty()) {
        m_pathInput->setText(directory);
        loadSvgs();
    }
#endif
}

// ============================================================================
// Message display helpers
// ============================================================================

void SvgGallery::showSuccess(const QString &message)
{
    m_infoLabel->setText(message);
    m_infoLabel->setStyleSheet("padding: 5px; background-color: #2a4a2a; color: #ccffcc; border-radius: 3px;");
}

void SvgGallery::showError(const QString &message)
{
    m_infoLabel->setText(message);
    m_infoLabel->setStyleSheet("padding: 5px; background-color: #4a2020; color: #ffcccc; border-radius: 3px;");
    qDebug() << "Error:" << message;
}

void SvgGallery::showWarning(const QString &message)
{
    m_infoLabel->setText(message);
    m_infoLabel->setStyleSheet("padding: 5px; background-color: #4a4020; color: #ffffcc; border-radius: 3px;");
    qDebug() << "Warning:" << message;
}

void SvgGallery::showInfo(const QString &message)
{
    m_infoLabel->setText(message);
    m_infoLabel->setStyleSheet("padding: 5px; background-color: #3a3a3a; color: #e0e0e0; border-radius: 3px;");
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
#ifdef Q_OS_ANDROID
    if (!m_androidFolder || !m_androidFolder->isReady()) {
        showError(tr("Please select a directory first."));
        return;
    }

    QStringList allFiles = m_androidFolder->fileNames();
    QStringList svgFiles, allPngFiles;
    for (const QString &f : allFiles) {
        if (f.endsWith(QLatin1String(".svg"), Qt::CaseInsensitive))
            svgFiles.append(f);
        else if (f.endsWith(QLatin1String(".png"), Qt::CaseInsensitive))
            allPngFiles.append(f);
    }
    svgFiles.sort();
    allPngFiles.sort();

    if (svgFiles.isEmpty()) {
        showWarning(tr("No SVG files found."));
        return;
    }

    // SAFはファイルパスを持たないので、キャッシュディレクトリに一時書き出し
    QString cacheDir = QStandardPaths::writableLocation(QStandardPaths::CacheLocation)
                       + QLatin1String("/svggallery/");
    QDir().mkpath(cacheDir);

    showInfo(tr("Loading %1 SVG file(s)...").arg(svgFiles.size()));
    QCoreApplication::processEvents();
    clearGallery();

    int totalPngsFound = 0;
    for (int index = 0; index < svgFiles.size(); ++index) {
        const QString &svgFile = svgFiles[index];
        showInfo(tr("Loading %1 of %2: %3").arg(index + 1).arg(svgFiles.size()).arg(svgFile));
        QCoreApplication::processEvents();

        // SVGをキャッシュに書き出す
        QString svgPath = cacheDir + svgFile;
        QByteArray svgData = m_androidFolder->read(svgFile);
        if (!svgData.isEmpty()) {
            QFile f(svgPath);
            if (f.open(QIODevice::WriteOnly)) {
                f.write(svgData);
                f.close();
            }
        }

        // PNGのマッチング
        QString baseName = QFileInfo(svgFile).completeBaseName();
        QStringList matchingPngs;
        QRegularExpression pngPattern(
            QString("^%1(_\\d+)?\\.png$").arg(QRegularExpression::escape(baseName)));
        for (const QString &pngFile : allPngFiles) {
            if (pngPattern.match(pngFile).hasMatch()) {
                QString pngPath = cacheDir + pngFile;
                QByteArray pngData = m_androidFolder->read(pngFile);
                if (!pngData.isEmpty()) {
                    QFile f(pngPath);
                    if (f.open(QIODevice::WriteOnly)) {
                        f.write(pngData);
                        f.close();
                    }
                }
                matchingPngs.append(pngPath);
            }
        }
        totalPngsFound += matchingPngs.size();

        SvgPair *svgWidget = new SvgPair(svgPath, matchingPngs, m_iconSize, m_customEngine, this);
        connect(svgWidget, &SvgPair::doubleClicked, this, &SvgGallery::showSvgContent);
        m_galleryLayout->addWidget(svgWidget, index, 0);
        m_svgPairs.append(svgWidget);
    }

    m_currentPath = m_androidFolder->treeUri();
    QString message = tr("Loaded %1 SVG file(s)").arg(svgFiles.size());
    if (totalPngsFound > 0)
        message += tr(" with %1 corresponding PNG(s)").arg(totalPngsFound);
    showSuccess(message);
    updateTextColors();

#else
    // ── デスクトップ（既存コード） ────────────────────────────
    QString path = m_pathInput->text().trimmed();
    if (path.isEmpty()) {
        showError(tr("Please enter a directory path."));
        return;
    }

    QDir dir(path);
    if (!dir.exists()) {
        showError(tr("Error: Directory does not exist: %1").arg(path));
        return;
    }

    QStringList svgFiles = dir.entryList(QStringList("*.svg"), QDir::Files, QDir::Name);
    if (svgFiles.isEmpty()) {
        showWarning(tr("No SVG files found in: %1").arg(path));
        return;
    }

    showInfo(tr("Loading %1 SVG file(s)...").arg(svgFiles.size()));
    QCoreApplication::processEvents();

    QStringList allPngFiles = dir.entryList(QStringList("*.png"), QDir::Files, QDir::Name);
    clearGallery();

    int totalPngsFound = 0;
    for (int index = 0; index < svgFiles.size(); ++index) {
        showInfo(tr("Loading %1 of %2: %3")
                     .arg(index + 1)
                     .arg(svgFiles.size())
                     .arg(svgFiles[index]));
        QCoreApplication::processEvents();

        QString svgFile = svgFiles[index];
        QString svgPath = dir.absoluteFilePath(svgFile);
        QFileInfo svgInfo(svgFile);
        QString baseName = svgInfo.completeBaseName();

        QStringList matchingPngs;
        QRegularExpression pngPattern(
            QString("^%1(_\\d+)?\\.png$").arg(QRegularExpression::escape(baseName)));
        for (const QString &pngFile : allPngFiles) {
            if (pngPattern.match(pngFile).hasMatch())
                matchingPngs.append(dir.absoluteFilePath(pngFile));
        }
        totalPngsFound += matchingPngs.size();

        SvgPair *svgWidget = new SvgPair(svgPath, matchingPngs, m_iconSize, m_customEngine, this);
        connect(svgWidget, &SvgPair::doubleClicked, this, &SvgGallery::showSvgContent);
        m_galleryLayout->addWidget(svgWidget, index, 0);
        m_svgPairs.append(svgWidget);
    }

    m_currentPath = path;
    QString message = tr("Loaded %1 SVG file(s)").arg(svgFiles.size());
    if (totalPngsFound > 0)
        message += tr(" with %1 corresponding PNG(s)").arg(totalPngsFound);
    message += tr(" from: %1").arg(path);

    showSuccess(message);
    updateTextColors();
#endif
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

    for (SvgPair *widget : m_svgPairs) {
        QFileInfo fileInfo(widget->svgPath());
        bool matches = filterText.isEmpty() || fileInfo.fileName().contains(filterText, Qt::CaseInsensitive);
        widget->setVisible(matches);
        if (matches) visibleCount++;
    }

    if (!filterText.isEmpty()) {
        showInfo(tr("Showing %1 of %2 items matching '%3'")
                     .arg(visibleCount).arg(m_svgPairs.size()).arg(filterText));
    } else if (!m_svgPairs.isEmpty()) {
        showSuccess(tr("Showing all %1 items").arg(m_svgPairs.size()));
    }
}

void SvgGallery::setupScintilla()
{
    if (!m_editor)
    {
        showError("Fatal: no editor at all!");
        return;
    }
    if (!m_editor->is_available()) {
        showError(tr("setupScintilla: %1").arg(m_editor->error_string()));
        return;
    }

    auto RGB = [](int r, int g, int b) -> unsigned long {
        return ((unsigned long)(((unsigned char)(r)|((unsigned short)((unsigned char)(g))<<8))|(((unsigned long)(unsigned char)(b))<<16)));
    };

    // Default style
    m_editor->style_set_fore(32, RGB(169, 183, 198));
    m_editor->style_set_back(32, RGB(43, 43, 43));
    m_editor->style_set_bold(32, false);
    m_editor->style_set_size(32, 10);
    m_editor->style_set_font(32, "Courier New");
    m_editor->style_clear_all();

    m_editor->set_tab_width(2);

    // Line numbers
    m_editor->style_set_fore(33, RGB(90, 90, 90));
    m_editor->style_set_back(33, RGB(33, 33, 33));


    m_editor->set_margin_type_n(0, 1);
    m_editor->set_margin_width_n(0, 40);
    m_editor->set_margin_width_n(1, 0);
    m_editor->set_margin_back_n(0, RGB(43, 43, 43));

    // Caret and selection
    m_editor->set_caret_fore(RGB(255, 255, 255));
    m_editor->set_sel_back(true, RGB(33, 66, 131));

    // Word wrap with indent preservation
    m_editor->set_wrap_mode(ScintillaRelay::WrapWord);              // 単語境界でラップ
    m_editor->set_wrap_indent_mode(ScintillaRelay::WrapIndentSame);  // 元の行と同じインデント
    m_editor->set_wrap_visual_flags(ScintillaRelay::WrapVisualFlagEnd);  // 行末にインジケータ表示
}

void SvgGallery::applyXMLHighlighting()
{
    if (!m_editor || !m_editor->is_available()) {
        showError(tr("applyXMLHighlighting: No Editor: %1").arg(m_editor->error_string()));
        return;
    }

    if (!m_editor->is_lexilla_available()) {
        showError(tr(
            "applyXMLHighlighting: No Lexilla: %1").arg(m_editor->error_string()));
        return;
    }

    if (!m_editor->set_lexer("xml")) {
        showError("Failed to set XML lexer");
        return;
    }

    m_editor->set_keywords(0,
        "svg path rect circle ellipse line polyline polygon text tspan "
        "g defs use symbol clipPath mask pattern linearGradient radialGradient "
        "stop filter feGaussianBlur feOffset feBlend feColorMatrix "
        "animate animateTransform");

    m_editor->set_keywords(1,
        "xmlns width height viewBox x y x1 y1 x2 y2 cx cy r rx ry "
        "d fill stroke stroke-width opacity fill-opacity stroke-opacity "
        "transform translate rotate scale matrix id class style "
        "points stroke-linecap stroke-linejoin stroke-dasharray "
        "gradientTransform gradientUnits offset stop-color stop-opacity");

    auto RGB = [](int r, int g, int b) {
        return ((unsigned long)(((unsigned char)(r)|((unsigned short)((unsigned char)(g))<<8))|(((unsigned long)(unsigned char)(b))<<16)));
    };

    m_editor->style_set_fore(0, RGB(169, 183, 198));
    m_editor->style_set_fore(1, RGB(80, 179, 124));
    m_editor->style_set_bold(1, true);
    m_editor->style_set_fore(2, RGB(80, 179, 124));
    m_editor->style_set_fore(3, RGB(170, 118, 152));
    m_editor->style_set_fore(4, RGB(170, 118, 152));
    m_editor->style_set_fore(5, RGB(104, 151, 187));
    m_editor->style_set_fore(6, RGB(89, 135, 106));
    m_editor->style_set_fore(7, RGB(89, 135, 106));
    m_editor->style_set_fore(8, RGB(169, 183, 198));
    m_editor->style_set_fore(9, RGB(128, 128, 128));
    m_editor->style_set_italic(9, true);
    m_editor->style_set_fore(10, RGB(104, 151, 187));
    m_editor->style_set_fore(11, RGB(80, 179, 124));
    m_editor->style_set_bold(11, true);
    m_editor->style_set_fore(12, RGB(204, 120, 50));

    m_editor->colorize(0, -1);
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
    m_editor->set_readonly(false);
    m_editor->clear_all();

    QByteArray utf8 = content.toUtf8();
    m_editor->insert_text(0, utf8.constData());

    applyXMLHighlighting();
    m_editor->goto_pos(0);

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
    QByteArray content = m_editor->text();

    // キャッシュに書き込む（既存コード）
    QFile file(m_currentSvgPath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        showError(tr("Error: Failed to save %1").arg(QFileInfo(m_currentSvgPath).fileName()));
        return;
    }
    file.write(content);
    file.close();

#ifdef Q_OS_ANDROID
    // SAFの元フォルダにも書き戻す
    if (m_androidFolder && m_androidFolder->isReady()) {
        const QString fileName = QFileInfo(m_currentSvgPath).fileName();
        if (!m_androidFolder->write(fileName, content)) {
            showError(tr("Error: Failed to save to original folder: %1").arg(fileName));
            return;
        }
    }
#endif

    showSuccess(tr("Saved and reloading: %1").arg(QFileInfo(m_currentSvgPath).fileName()));
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
