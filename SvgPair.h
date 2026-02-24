#ifndef SVGPAIR_H
#define SVGPAIR_H

#include <QWidget>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QToolButton>
#include <QSvgWidget>
#include <QString>
#include <QStringList>

// Displays an SVG and its corresponding PNGs (if found)
// Each image is shown twice: disabled and enabled toolbuttons
class SvgPair : public QWidget
{
    Q_OBJECT

public:
    explicit SvgPair(
        const QString &svgPath,
        const QStringList &pngPaths,
        int iconSize = 32,
        bool customEngine = true,
        QWidget *parent = nullptr);
    
    QString svgPath() const { return m_svgPath; }
    void setIconSize(int size);

private:
    struct IconPair {
        QToolButton *disabledButton;
        QToolButton *enabledButton;
        QLabel *disabledLabel;
        QLabel *enabledLabel;
        QLabel *typeLabel;
        int originalSize; // For PNGs to know their original size
    };

    void createIconPair(const QString &path, const QString &label, int size, bool isSvg);
    
    QString m_svgPath;
    int m_iconSize;
    bool m_customEngine;
    QHBoxLayout *m_iconsLayout;
    QLabel *m_filenameLabel;
    QList<IconPair> m_iconPairs;
};

#endif // SVGPAIR_H
