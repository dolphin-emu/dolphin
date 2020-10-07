/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtQml module of the Qt Toolkit.
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

#ifndef QQMLENGINE_H
#define QQMLENGINE_H

#include <QtCore/qurl.h>
#include <QtCore/qobject.h>
#include <QtCore/qmap.h>
#include <QtQml/qjsengine.h>
#include <QtQml/qqml.h>
#include <QtQml/qqmlerror.h>

QT_BEGIN_NAMESPACE

class QQmlAbstractUrlInterceptor;

class Q_QML_EXPORT QQmlImageProviderBase
{
public:
    enum ImageType {
        Image,
        Pixmap,
        Texture,
        Invalid,
        ImageResponse
        // ### Qt6: reorder these, and give Invalid a fixed large value
    };

    enum Flag {
        ForceAsynchronousImageLoading  = 0x01
    };
    Q_DECLARE_FLAGS(Flags, Flag)

    virtual ~QQmlImageProviderBase();

    virtual ImageType imageType() const = 0;
    virtual Flags flags() const = 0;

private:
    friend class QQuickImageProvider;
    QQmlImageProviderBase();
};
Q_DECLARE_OPERATORS_FOR_FLAGS(QQmlImageProviderBase::Flags)

class QQmlComponent;
class QQmlEnginePrivate;
class QQmlImportsPrivate;
class QQmlExpression;
class QQmlContext;
class QQmlType;
class QUrl;
#if QT_CONFIG(qml_network)
class QNetworkAccessManager;
class QQmlNetworkAccessManagerFactory;
#endif
class QQmlIncubationController;
class Q_QML_EXPORT QQmlEngine : public QJSEngine
{
    Q_PROPERTY(QString offlineStoragePath READ offlineStoragePath WRITE setOfflineStoragePath)
    Q_OBJECT
public:
    explicit QQmlEngine(QObject *p = nullptr);
    ~QQmlEngine() override;

    QQmlContext *rootContext() const;

    void clearComponentCache();
    void trimComponentCache();

    QStringList importPathList() const;
    void setImportPathList(const QStringList &paths);
    void addImportPath(const QString& dir);

    QStringList pluginPathList() const;
    void setPluginPathList(const QStringList &paths);
    void addPluginPath(const QString& dir);

    bool addNamedBundle(const QString &name, const QString &fileName);

#if QT_CONFIG(library)
    bool importPlugin(const QString &filePath, const QString &uri, QList<QQmlError> *errors);
#endif

#if QT_CONFIG(qml_network)
    void setNetworkAccessManagerFactory(QQmlNetworkAccessManagerFactory *);
    QQmlNetworkAccessManagerFactory *networkAccessManagerFactory() const;

    QNetworkAccessManager *networkAccessManager() const;
#endif

    void setUrlInterceptor(QQmlAbstractUrlInterceptor* urlInterceptor);
    QQmlAbstractUrlInterceptor* urlInterceptor() const;

    void addImageProvider(const QString &id, QQmlImageProviderBase *);
    QQmlImageProviderBase *imageProvider(const QString &id) const;
    void removeImageProvider(const QString &id);

    void setIncubationController(QQmlIncubationController *);
    QQmlIncubationController *incubationController() const;

    void setOfflineStoragePath(const QString& dir);
    QString offlineStoragePath() const;
    QString offlineStorageDatabaseFilePath(const QString &databaseName) const;

    QUrl baseUrl() const;
    void setBaseUrl(const QUrl &);

    bool outputWarningsToStandardError() const;
    void setOutputWarningsToStandardError(bool);

    template<typename T>
    T singletonInstance(int qmlTypeId);

public Q_SLOTS:
    void retranslate();

public:
    static QQmlContext *contextForObject(const QObject *);
    static void setContextForObject(QObject *, QQmlContext *);

    enum ObjectOwnership { CppOwnership, JavaScriptOwnership };
    static void setObjectOwnership(QObject *, ObjectOwnership);
    static ObjectOwnership objectOwnership(QObject *);
protected:
    QQmlEngine(QQmlEnginePrivate &dd, QObject *p);
    bool event(QEvent *) override;

Q_SIGNALS:
    void quit();
    void exit(int retCode);
    void warnings(const QList<QQmlError> &warnings);

private:
    Q_DISABLE_COPY(QQmlEngine)
    Q_DECLARE_PRIVATE(QQmlEngine)
};

template<>
Q_QML_EXPORT QJSValue QQmlEngine::singletonInstance<QJSValue>(int qmlTypeId);

template<typename T>
T QQmlEngine::singletonInstance(int qmlTypeId) {
    return qobject_cast<T>(singletonInstance<QJSValue>(qmlTypeId).toQObject());
}

QT_END_NAMESPACE

#endif // QQMLENGINE_H
