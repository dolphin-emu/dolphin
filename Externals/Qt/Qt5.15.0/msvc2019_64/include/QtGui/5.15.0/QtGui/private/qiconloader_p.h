/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtGui module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl-3.0.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or (at your option) the GNU General
** Public license version 3 or any later version approved by the KDE Free
** Qt Foundation. The licenses are as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL2 and LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-2.0.html and
** https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#ifndef QICONLOADER_P_H
#define QICONLOADER_P_H

#include <QtGui/private/qtguiglobal_p.h>

#ifndef QT_NO_ICON
//
//  W A R N I N G
//  -------------
//
// This file is not part of the Qt API.  It exists purely as an
// implementation detail.  This header file may change from version to
// version without notice, or even be removed.
//
// We mean it.
//

#include <QtGui/QIcon>
#include <QtGui/QIconEngine>
#include <QtGui/QPixmapCache>
#include <private/qicon_p.h>
#include <private/qfactoryloader_p.h>
#include <QtCore/QHash>
#include <QtCore/QVector>
#include <QtCore/QTypeInfo>

QT_BEGIN_NAMESPACE

class QIconLoader;

struct QIconDirInfo
{
    enum Type { Fixed, Scalable, Threshold, Fallback };
    QIconDirInfo(const QString &_path = QString()) :
            path(_path),
            size(0),
            maxSize(0),
            minSize(0),
            threshold(0),
            scale(1),
            type(Threshold) {}
    QString path;
    short size;
    short maxSize;
    short minSize;
    short threshold;
    short scale;
    Type type;
};
Q_DECLARE_TYPEINFO(QIconDirInfo, Q_MOVABLE_TYPE);

class QIconLoaderEngineEntry
 {
public:
    virtual ~QIconLoaderEngineEntry() {}
    virtual QPixmap pixmap(const QSize &size,
                           QIcon::Mode mode,
                           QIcon::State state) = 0;
    QString filename;
    QIconDirInfo dir;
};

struct ScalableEntry : public QIconLoaderEngineEntry
{
    QPixmap pixmap(const QSize &size, QIcon::Mode mode, QIcon::State state) override;
    QIcon svgIcon;
};

struct PixmapEntry : public QIconLoaderEngineEntry
{
    QPixmap pixmap(const QSize &size, QIcon::Mode mode, QIcon::State state) override;
    QPixmap basePixmap;
};

typedef QList<QIconLoaderEngineEntry*> QThemeIconEntries;

struct QThemeIconInfo
{
    QThemeIconEntries entries;
    QString iconName;
};

class QIconLoaderEngine : public QIconEngine
{
public:
    QIconLoaderEngine(const QString& iconName = QString());
    ~QIconLoaderEngine();

    void paint(QPainter *painter, const QRect &rect, QIcon::Mode mode, QIcon::State state) override;
    QPixmap pixmap(const QSize &size, QIcon::Mode mode, QIcon::State state) override;
    QSize actualSize(const QSize &size, QIcon::Mode mode, QIcon::State state) override;
    QIconEngine *clone() const override;
    bool read(QDataStream &in) override;
    bool write(QDataStream &out) const override;

    Q_GUI_EXPORT static QIconLoaderEngineEntry *entryForSize(const QThemeIconInfo &info, const QSize &size, int scale = 1);

private:
    QString key() const override;
    bool hasIcon() const;
    void ensureLoaded();
    void virtual_hook(int id, void *data) override;

    QIconLoaderEngine(const QIconLoaderEngine &other);
    QThemeIconInfo m_info;
    QString m_iconName;
    uint m_key;

    friend class QIconLoader;
};

class QIconCacheGtkReader;

class QIconTheme
{
public:
    QIconTheme(const QString &name);
    QIconTheme() : m_valid(false) {}
    QStringList parents() { return m_parents; }
    QVector<QIconDirInfo> keyList() { return m_keyList; }
    QStringList contentDirs() { return m_contentDirs; }
    bool isValid() { return m_valid; }
private:
    QStringList m_contentDirs;
    QVector<QIconDirInfo> m_keyList;
    QStringList m_parents;
    bool m_valid;
public:
    QVector<QSharedPointer<QIconCacheGtkReader>> m_gtkCaches;
};

class Q_GUI_EXPORT QIconLoader
{
public:
    QIconLoader();
    QThemeIconInfo loadIcon(const QString &iconName) const;
    uint themeKey() const { return m_themeKey; }

    QString themeName() const { return m_userTheme.isEmpty() ? m_systemTheme : m_userTheme; }
    void setThemeName(const QString &themeName);
    QString fallbackThemeName() const;
    void setFallbackThemeName(const QString &themeName);
    QIconTheme theme() { return themeList.value(themeName()); }
    void setThemeSearchPath(const QStringList &searchPaths);
    QStringList themeSearchPaths() const;
    void setFallbackSearchPaths(const QStringList &searchPaths);
    QStringList fallbackSearchPaths() const;
    QIconDirInfo dirInfo(int dirindex);
    static QIconLoader *instance();
    void updateSystemTheme();
    void invalidateKey() { m_themeKey++; }
    void ensureInitialized();
    bool hasUserTheme() const { return !m_userTheme.isEmpty(); }

private:
    QThemeIconInfo findIconHelper(const QString &themeName,
                                  const QString &iconName,
                                  QStringList &visited) const;
    QThemeIconInfo lookupFallbackIcon(const QString &iconName) const;

    uint m_themeKey;
    bool m_supportsSvg;
    bool m_initialized;

    mutable QString m_userTheme;
    mutable QString m_userFallbackTheme;
    mutable QString m_systemTheme;
    mutable QStringList m_iconDirs;
    mutable QHash <QString, QIconTheme> themeList;
    mutable QStringList m_fallbackDirs;
};

QT_END_NAMESPACE

#endif // QT_NO_ICON

#endif // QICONLOADER_P_H
