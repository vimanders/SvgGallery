#ifndef SVGGALLERY_H
#define SVGGALLERY_H

#include "SvgPair.h"

#include <QColor>
#include <QGridLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMainWindow>
#include <QPushButton>
#include <QScrollArea>
#include <QSlider>
#include <QSplitter>

class ScintillaRelay;

// A Simple Gallery of SVGs in a given folder
class SvgGallery : public QMainWindow
{
    Q_OBJECT

public:
    explicit SvgGallery(QWidget *parent = nullptr);

private slots:
    void browseDirectory();
    void loadSvgs();
    void updateIconSizes();
    void filterGallery();
    void showSvgContent(const QString &svgPath);
    void saveSvgContent();
    void closeEditor();

private:
    void initUI();
    void updateBackgroundColor();
    void updateTextColors();
    void clearGallery();
    void setupScintilla();
    void applyXMLHighlighting();
    void reloadCurrentSvg();

    // Message display helpers
    void showSuccess(const QString &message);
    void showError(const QString &message);
    void showWarning(const QString &message);
    void showInfo(const QString &message);

    // UI Components
    QLineEdit *m_pathInput;
    QLineEdit *m_filterInput;
    QLabel *m_infoLabel;
    QLabel *m_sizeLabel;
    QSlider *m_sizeSlider;
    QScrollArea *m_scrollArea;
    QWidget *m_galleryWidget;
    QGridLayout *m_galleryLayout;
    QSplitter *m_splitter;

    // Editor components
    QWidget *m_editorContainer;
    QLabel *m_editorTitle;
    QPushButton *m_saveButton;
    ScintillaRelay *m_editor;

    // State
    QString m_currentPath;
    QString m_currentSvgPath;
    QColor m_backgroundColor;
    int m_iconSize = 32;
    bool m_customEngine = true;
    bool m_editorVisible;

    // Gallery items
    QList<SvgPair*> m_svgPairs;
};

#endif // SVGGALLERY_H
