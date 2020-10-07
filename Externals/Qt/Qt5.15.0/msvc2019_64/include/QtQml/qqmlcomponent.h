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

#ifndef QQMLCOMPONENT_H
#define QQMLCOMPONENT_H

#include <QtQml/qqml.h>
#include <QtQml/qqmlerror.h>

#include <QtCore/qobject.h>
#include <QtCore/qstring.h>
#include <QtQml/qjsvalue.h>

QT_BEGIN_NAMESPACE


class QByteArray;
class QQmlEngine;
class QQmlComponent;
class QQmlIncubator;
class QQmlV4Function;
class QQmlComponentPrivate;
class QQmlComponentAttached;

namespace QV4 {
class ExecutableCompilationUnit;
}

class Q_QML_EXPORT QQmlComponent : public QObject
{
    Q_OBJECT
    Q_DECLARE_PRIVATE(QQmlComponent)

    Q_PROPERTY(qreal progress READ progress NOTIFY progressChanged)
    Q_PROPERTY(Status status READ status NOTIFY statusChanged)
    Q_PROPERTY(QUrl url READ url CONSTANT)
    QML_NAMED_ELEMENT(Component)
    QML_ATTACHED(QQmlComponentAttached)
    Q_CLASSINFO("QML.Builtin", "QML")

public:
    enum CompilationMode { PreferSynchronous, Asynchronous };
    Q_ENUM(CompilationMode)

    QQmlComponent(QObject *parent = nullptr);
    QQmlComponent(QQmlEngine *, QObject *parent = nullptr);
    QQmlComponent(QQmlEngine *, const QString &fileName, QObject *parent = nullptr);
    QQmlComponent(QQmlEngine *, const QString &fileName, CompilationMode mode, QObject *parent = nullptr);
    QQmlComponent(QQmlEngine *, const QUrl &url, QObject *parent = nullptr);
    QQmlComponent(QQmlEngine *, const QUrl &url, CompilationMode mode, QObject *parent = nullptr);
    ~QQmlComponent() override;

    enum Status { Null, Ready, Loading, Error };
    Q_ENUM(Status)
    Status status() const;

    bool isNull() const;
    bool isReady() const;
    bool isError() const;
    bool isLoading() const;

    QList<QQmlError> errors() const;
    Q_INVOKABLE QString errorString() const;

    qreal progress() const;

    QUrl url() const;

    virtual QObject *create(QQmlContext *context = nullptr);
    QObject *createWithInitialProperties(const QVariantMap& initialProperties, QQmlContext *context = nullptr);
    void setInitialProperties(QObject *component, const QVariantMap &properties);
    virtual QObject *beginCreate(QQmlContext *);
    virtual void completeCreate();

    void create(QQmlIncubator &, QQmlContext *context = nullptr,
                QQmlContext *forContext = nullptr);

    QQmlContext *creationContext() const;
    QQmlEngine *engine() const;

    static QQmlComponentAttached *qmlAttachedProperties(QObject *);

public Q_SLOTS:
    void loadUrl(const QUrl &url);
    void loadUrl(const QUrl &url, CompilationMode mode);
    void setData(const QByteArray &, const QUrl &baseUrl);

Q_SIGNALS:
    void statusChanged(QQmlComponent::Status);
    void progressChanged(qreal);

protected:
    QQmlComponent(QQmlComponentPrivate &dd, QObject* parent);
    Q_INVOKABLE void createObject(QQmlV4Function *);
    Q_INVOKABLE void incubateObject(QQmlV4Function *);

private:
    QQmlComponent(QQmlEngine *, QV4::ExecutableCompilationUnit *compilationUnit, int,
                  QObject *parent);

    Q_DISABLE_COPY(QQmlComponent)
    friend class QQmlTypeData;
    friend class QQmlObjectCreator;
};


// Don't do this at home.
namespace QQmlPrivate {

// Generally you cannot use QQmlComponentAttached as attached properties object in derived classes.
// It is private.
template<class T>
struct OverridableAttachedType<T, QQmlComponentAttached>
{
    using Type = void;
};

// QQmlComponent itself is allowed to use QQmlComponentAttached, though.
template<>
struct OverridableAttachedType<QQmlComponent, QQmlComponentAttached>
{
    using Type = QQmlComponentAttached;
};

} // namespace QQmlPrivate


QT_END_NAMESPACE
QML_DECLARE_TYPE(QQmlComponent)

#endif // QQMLCOMPONENT_H
