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

#ifndef QQMLGLOBAL_H
#define QQMLGLOBAL_H

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

#include <private/qtqmlglobal_p.h>
#include <QtCore/QObject>
#include <private/qqmlmetaobject_p.h>
#include <private/qmetaobject_p.h>

QT_BEGIN_NAMESPACE


#define DEFINE_BOOL_CONFIG_OPTION(name, var) \
    static bool name() \
    { \
        static enum { Yes, No, Unknown } status = Unknown; \
        if (status == Unknown) { \
            status = No; \
            if (Q_UNLIKELY(!qEnvironmentVariableIsEmpty(#var))) { \
                const QByteArray v = qgetenv(#var); \
                if (v != "0" && v != "false") \
                    status = Yes; \
            } \
        } \
        return status == Yes; \
    }

/*!
    Connect \a Signal of \a Sender to \a Method of \a Receiver.  \a Signal must be
    of type \a SenderType and \a Receiver of type \a ReceiverType.

    Unlike QObject::connect(), this method caches the lookup of the signal and method
    indexes.  It also does not require lazy QMetaObjects to be built so should be
    preferred in all QML code that might interact with QML built objects.

    \code
        QQuickTextControl *control;
        QQuickTextEdit *textEdit;
        qmlobject_connect(control, QQuickTextControl, SIGNAL(updateRequest(QRectF)),
                          textEdit, QQuickTextEdit, SLOT(updateDocument()));
    \endcode
*/
#define qmlobject_connect(Sender, SenderType, Signal, Receiver, ReceiverType, Method) \
do { \
    SenderType *sender = (Sender); \
    ReceiverType *receiver = (Receiver); \
    const char *signal = (Signal); \
    const char *method = (Method); \
    static int signalIdx = -1; \
    static int methodIdx = -1; \
    if (signalIdx < 0) { \
        Q_ASSERT((int(*signal) - '0') == QSIGNAL_CODE); \
        signalIdx = SenderType::staticMetaObject.indexOfSignal(signal+1); \
    } \
    if (methodIdx < 0) { \
        int code = (int(*method) - '0'); \
        Q_ASSERT(code == QSLOT_CODE || code == QSIGNAL_CODE); \
        if (code == QSLOT_CODE) \
            methodIdx = ReceiverType::staticMetaObject.indexOfSlot(method+1); \
        else \
            methodIdx = ReceiverType::staticMetaObject.indexOfSignal(method+1); \
    } \
    Q_ASSERT(signalIdx != -1 && methodIdx != -1); \
    QMetaObject::connect(sender, signalIdx, receiver, methodIdx, Qt::DirectConnection); \
} while (0)

/*!
    Disconnect \a Signal of \a Sender from \a Method of \a Receiver.  \a Signal must be
    of type \a SenderType and \a Receiver of type \a ReceiverType.

    Unlike QObject::disconnect(), this method caches the lookup of the signal and method
    indexes.  It also does not require lazy QMetaObjects to be built so should be
    preferred in all QML code that might interact with QML built objects.

    \code
        QQuickTextControl *control;
        QQuickTextEdit *textEdit;
        qmlobject_disconnect(control, QQuickTextControl, SIGNAL(updateRequest(QRectF)),
                             textEdit, QQuickTextEdit, SLOT(updateDocument()));
    \endcode
*/
#define qmlobject_disconnect(Sender, SenderType, Signal, Receiver, ReceiverType, Method) \
do { \
    SenderType *sender = (Sender); \
    ReceiverType *receiver = (Receiver); \
    const char *signal = (Signal); \
    const char *method = (Method); \
    static int signalIdx = -1; \
    static int methodIdx = -1; \
    if (signalIdx < 0) { \
        Q_ASSERT((int(*signal) - '0') == QSIGNAL_CODE); \
        signalIdx = SenderType::staticMetaObject.indexOfSignal(signal+1); \
    } \
    if (methodIdx < 0) { \
        int code = (int(*method) - '0'); \
        Q_ASSERT(code == QSLOT_CODE || code == QSIGNAL_CODE); \
        if (code == QSLOT_CODE) \
            methodIdx = ReceiverType::staticMetaObject.indexOfSlot(method+1); \
        else \
            methodIdx = ReceiverType::staticMetaObject.indexOfSignal(method+1); \
    } \
    Q_ASSERT(signalIdx != -1 && methodIdx != -1); \
    QMetaObject::disconnect(sender, signalIdx, receiver, methodIdx); \
} while (0)

/*!
    This method is identical to qobject_cast<T>() except that it does not require lazy
    QMetaObjects to be built, so should be preferred in all QML code that might interact
    with QML built objects.

    \code
        QObject *object;
        if (QQuickTextEdit *textEdit = qmlobject_cast<QQuickTextEdit *>(object)) {
            // ...Do something...
        }
    \endcode
*/
template<class T>
T qmlobject_cast(QObject *object)
{
    if (object && QQmlMetaObject::canConvert(object, &reinterpret_cast<T>(object)->staticMetaObject))
        return static_cast<T>(object);
    else
        return 0;
}

#define IS_SIGNAL_CONNECTED(Sender, SenderType, Name, Arguments) \
do { \
    QObject *sender = (Sender); \
    void (SenderType::*signal)Arguments = &SenderType::Name; \
    static QMetaMethod method = QMetaMethod::fromSignal(signal); \
    static int signalIdx = QMetaObjectPrivate::signalIndex(method); \
    return QObjectPrivate::get(sender)->isSignalConnected(signalIdx); \
} while (0)

/*!
    Returns true if the case of \a fileName is equivalent to the file case of
    \a fileName on disk, and false otherwise.

    This is used to ensure that the behavior of QML on a case-insensitive file
    system is the same as on a case-sensitive file system.  This function
    performs a "best effort" attempt to determine the real case of the file.
    It may have false positives (say the case is correct when it isn't), but it
    should never have a false negative (say the case is incorrect when it is
    correct).

    Length specifies specifies the number of characters to be checked from
    behind. That is, if a file name results from a relative path specification
    like "foo/bar.qml" and is made absolute, the original length (11) should
    be passed indicating that only the last part of the relative path should
    be checked.

*/
bool QQml_isFileCaseCorrect(const QString &fileName, int length = -1);

/*!
    Makes the \a object a child of \a parent.  Note that when using this method,
    neither \a parent nor the object's previous parent (if it had one) will
    receive ChildRemoved or ChildAdded events.
*/
inline void QQml_setParent_noEvent(QObject *object, QObject *parent)
{
    QObjectPrivate *d_ptr = QObjectPrivate::get(object);
    bool sce = d_ptr->sendChildEvents;
    d_ptr->sendChildEvents = false;
    object->setParent(parent);
    d_ptr->sendChildEvents = sce;
}

class Q_QML_PRIVATE_EXPORT QQmlValueTypeProvider
{
public:
    QQmlValueTypeProvider();
    virtual ~QQmlValueTypeProvider();

    const QMetaObject *metaObjectForMetaType(int);

    bool initValueType(int, QVariant&);

    QVariant createValueType(int, int, const void *[]);
    bool createValueFromString(int, const QString &, void *, size_t);
    bool createStringFromValue(int, const void *, QString *);

    QVariant createVariantFromString(const QString &);
    QVariant createVariantFromString(int, const QString &, bool *);
    QVariant createVariantFromJsObject(int, const QV4::Value &, QV4::ExecutionEngine *, bool *);

    bool equalValueType(int, const void *, const QVariant&);
    bool storeValueType(int, const void *, void *, size_t);
    bool readValueType(const QVariant&, void *, int);
    bool writeValueType(int, const void *, QVariant&);

private:
    virtual const QMetaObject *getMetaObjectForMetaType(int);
    virtual bool init(int, QVariant&);

    virtual bool create(int, int, const void *[], QVariant *);
    virtual bool createFromString(int, const QString &, void *, size_t);
    virtual bool createStringFrom(int, const void *, QString *);

    virtual bool variantFromString(const QString &, QVariant *);
    virtual bool variantFromString(int, const QString &, QVariant *);
    virtual bool variantFromJsObject(int, const QV4::Value &, QV4::ExecutionEngine *, QVariant *);

    virtual bool equal(int, const void *, const QVariant&);
    virtual bool store(int, const void *, void *, size_t);
    virtual bool read(const QVariant&, void *, int);
    virtual bool write(int, const void *, QVariant&);

    friend Q_QML_PRIVATE_EXPORT void QQml_addValueTypeProvider(QQmlValueTypeProvider *);
    friend Q_QML_PRIVATE_EXPORT void QQml_removeValueTypeProvider(QQmlValueTypeProvider *);

    QQmlValueTypeProvider *next;
};

Q_QML_PRIVATE_EXPORT void QQml_addValueTypeProvider(QQmlValueTypeProvider *);
Q_AUTOTEST_EXPORT QQmlValueTypeProvider *QQml_valueTypeProvider();


class Q_QML_PRIVATE_EXPORT QQmlColorProvider
{
public:
    virtual ~QQmlColorProvider();
    virtual QVariant colorFromString(const QString &, bool *);
    virtual unsigned rgbaFromString(const QString &, bool *);

    virtual QVariant fromRgbF(double, double, double, double);
    virtual QVariant fromHslF(double, double, double, double);
    virtual QVariant fromHsvF(double, double, double, double);
    virtual QVariant lighter(const QVariant &, qreal);
    virtual QVariant darker(const QVariant &, qreal);
    virtual QVariant tint(const QVariant &, const QVariant &);
};

Q_QML_PRIVATE_EXPORT QQmlColorProvider *QQml_setColorProvider(QQmlColorProvider *);
Q_QML_PRIVATE_EXPORT QQmlColorProvider *QQml_colorProvider();


class Q_QML_PRIVATE_EXPORT QQmlGuiProvider
{
public:
    virtual ~QQmlGuiProvider();
    virtual QObject *application(QObject *parent);
    virtual QObject *inputMethod();
    virtual QObject *styleHints();
    virtual QStringList fontFamilies();
    virtual bool openUrlExternally(QUrl &);
    virtual QString pluginName() const;
};

Q_QML_PRIVATE_EXPORT QQmlGuiProvider *QQml_setGuiProvider(QQmlGuiProvider *);
Q_AUTOTEST_EXPORT QQmlGuiProvider *QQml_guiProvider();

class QQmlApplicationPrivate;

class Q_QML_PRIVATE_EXPORT QQmlApplication : public QObject
{
    //Application level logic, subclassed by Qt Quick if available via QQmlGuiProvider
    Q_OBJECT
    Q_PROPERTY(QStringList arguments READ args CONSTANT)
    Q_PROPERTY(QString name READ name WRITE setName NOTIFY nameChanged)
    Q_PROPERTY(QString version READ version WRITE setVersion NOTIFY versionChanged)
    Q_PROPERTY(QString organization READ organization WRITE setOrganization NOTIFY organizationChanged)
    Q_PROPERTY(QString domain READ domain WRITE setDomain NOTIFY domainChanged)
public:
    QQmlApplication(QObject* parent=nullptr);

    QStringList args();

    QString name() const;
    QString version() const;
    QString organization() const;
    QString domain() const;

public Q_SLOTS:
    void setName(const QString &arg);
    void setVersion(const QString &arg);
    void setOrganization(const QString &arg);
    void setDomain(const QString &arg);

Q_SIGNALS:
    void aboutToQuit();

    void nameChanged();
    void versionChanged();
    void organizationChanged();
    void domainChanged();

protected:
    QQmlApplication(QQmlApplicationPrivate &dd, QObject* parent=nullptr);

private:
    Q_DISABLE_COPY(QQmlApplication)
    Q_DECLARE_PRIVATE(QQmlApplication)
};

class QQmlApplicationPrivate : public QObjectPrivate
{
    Q_DECLARE_PUBLIC(QQmlApplication)
public:
    QQmlApplicationPrivate() {
        argsInit = false;
    }

    bool argsInit;
    QStringList args;
};

struct QQmlSourceLocation
{
    QQmlSourceLocation() {}
    QQmlSourceLocation(const QString &sourceFile, quint16 line, quint16 column)
        : sourceFile(sourceFile), line(line), column(column) {}
    QString sourceFile;
    quint16 line = 0;
    quint16 column = 0;
};

QT_END_NAMESPACE

#endif // QQMLGLOBAL_H
