#ifndef SVGGALLERY_H
#define SVGGALLERY_H

#include "SvgPair.h"

#include <QMainWindow>
#include <QLineEdit>
#include <QLabel>
#include <QGridLayout>
#include <QWidget>
#include <QColor>
#include <QList>
#include <QSlider>
#include <QScrollArea>

// A Simple Gallery of SVGs in a given folder
class SvgGallery : public QMainWindow
{
    Q_OBJECT

public:
    SvgGallery(QWidget *parent = nullptr);
    ~SvgGallery() = default;

private slots:
    void browseDirectory();
    void loadSvgs();

private:
    void initUI();
    void updateBackgroundColor();
    void clearGallery();
    void updateIconSizes();
    
    QString m_currentPath;
    QColor m_backgroundColor;
    int m_iconSize = 32;
    bool m_customEngine = true;
    QLineEdit *m_pathInput;
    QLabel *m_infoLabel;
    QLabel *m_sizeLabel;
    QSlider *m_sizeSlider;
    QScrollArea *m_scrollArea;
    QWidget *m_galleryWidget;
    QGridLayout *m_galleryLayout;
    QList<SvgPair*> m_svgPairs;
};

#endif // SVGGALLERY_H
