#ifndef SVGPAIR_H
#define SVGPAIR_H

#include <QWidget>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QToolButton>
#include <QSvgWidget>
#include <QString>

// Displays an SVG twice, one on a disabled and one on an enabled toolbutton
class SvgPair : public QWidget
{
    Q_OBJECT

public:
    explicit SvgPair(
        const QString &svgPath,
        int iconSize = 32,
        bool customEngine = true,
        QWidget *parent = nullptr);
    
    QString svgPath() const { return m_svgPath; }
    void setIconSize(int size);

private:
    QString m_svgPath;
    int m_iconSize;
    QToolButton *m_disabledButton;
    QToolButton *m_enabledButton;
    QLabel *m_disabledLabel;
    QLabel *m_enabledLabel;
    QLabel *m_filenameLabel;
};

#endif // SVGPAIR_H
