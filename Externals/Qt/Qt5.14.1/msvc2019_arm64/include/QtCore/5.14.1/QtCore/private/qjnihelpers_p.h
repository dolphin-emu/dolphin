/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtCore module of the Qt Toolkit.
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

#ifndef QJNIHELPERS_H
#define QJNIHELPERS_H

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

#include <jni.h>
#include <functional>
#include <QtCore/private/qglobal_p.h>
#include <QHash>
#include <QMetaType>

QT_BEGIN_NAMESPACE

class QRunnable;
class QStringList;

namespace QtAndroidPrivate
{
    class Q_CORE_EXPORT ActivityResultListener
    {
    public:
        virtual ~ActivityResultListener();
        virtual bool handleActivityResult(jint requestCode, jint resultCode, jobject data) = 0;
    };

    class Q_CORE_EXPORT NewIntentListener
    {
    public:
        virtual ~NewIntentListener();
        virtual bool handleNewIntent(JNIEnv *env, jobject intent) = 0;
    };

    class Q_CORE_EXPORT ResumePauseListener
    {
    public:
        virtual ~ResumePauseListener();
        virtual void handlePause();
        virtual void handleResume();
    };

    class Q_CORE_EXPORT GenericMotionEventListener
    {
    public:
        virtual ~GenericMotionEventListener();
        virtual bool handleGenericMotionEvent(jobject event) = 0;
    };

    class Q_CORE_EXPORT KeyEventListener
    {
    public:
        virtual ~KeyEventListener();
        virtual bool handleKeyEvent(jobject event) = 0;
    };

    class Q_CORE_EXPORT OnBindListener
    {
    public:
        virtual ~OnBindListener() {}
        virtual jobject onBind(jobject intent) = 0;
    };

    enum class PermissionsResult {
        Granted,
        Denied
    };
    typedef QHash<QString,  QtAndroidPrivate::PermissionsResult> PermissionsHash;
    typedef std::function<void()> Runnable;
    typedef std::function<void(const PermissionsHash &)> PermissionsResultFunc;

    Q_CORE_EXPORT jobject activity();
    Q_CORE_EXPORT jobject service();
    Q_CORE_EXPORT jobject context();
    Q_CORE_EXPORT JavaVM *javaVM();
    Q_CORE_EXPORT jint initJNI(JavaVM *vm, JNIEnv *env);
    jobject classLoader();
    Q_CORE_EXPORT jint androidSdkVersion();
    Q_CORE_EXPORT void runOnAndroidThread(const Runnable &runnable, JNIEnv *env);
    Q_CORE_EXPORT void runOnAndroidThreadSync(const Runnable &runnable, JNIEnv *env, int timeoutMs = INT_MAX);
    Q_CORE_EXPORT void runOnUiThread(QRunnable *runnable, JNIEnv *env);
    Q_CORE_EXPORT void requestPermissions(JNIEnv *env, const QStringList &permissions, const PermissionsResultFunc &callbackFunc, bool directCall = false);
    Q_CORE_EXPORT PermissionsHash requestPermissionsSync(JNIEnv *env, const QStringList &permissions, int timeoutMs = INT_MAX);
    Q_CORE_EXPORT PermissionsResult checkPermission(const QString &permission);
    Q_CORE_EXPORT bool shouldShowRequestPermissionRationale(const QString &permission);

    Q_CORE_EXPORT void handleActivityResult(jint requestCode, jint resultCode, jobject data);
    Q_CORE_EXPORT void registerActivityResultListener(ActivityResultListener *listener);
    Q_CORE_EXPORT void unregisterActivityResultListener(ActivityResultListener *listener);

    Q_CORE_EXPORT void handleNewIntent(JNIEnv *env, jobject intent);
    Q_CORE_EXPORT void registerNewIntentListener(NewIntentListener *listener);
    Q_CORE_EXPORT void unregisterNewIntentListener(NewIntentListener *listener);

    Q_CORE_EXPORT void handlePause();
    Q_CORE_EXPORT void handleResume();
    Q_CORE_EXPORT void registerResumePauseListener(ResumePauseListener *listener);
    Q_CORE_EXPORT void unregisterResumePauseListener(ResumePauseListener *listener);

    Q_CORE_EXPORT void registerGenericMotionEventListener(GenericMotionEventListener *listener);
    Q_CORE_EXPORT void unregisterGenericMotionEventListener(GenericMotionEventListener *listener);

    Q_CORE_EXPORT void registerKeyEventListener(KeyEventListener *listener);
    Q_CORE_EXPORT void unregisterKeyEventListener(KeyEventListener *listener);

    Q_CORE_EXPORT void hideSplashScreen(JNIEnv *env, int duration = 0);


    Q_CORE_EXPORT void waitForServiceSetup();
    Q_CORE_EXPORT int acuqireServiceSetup(int flags);
    Q_CORE_EXPORT void setOnBindListener(OnBindListener *listener);
    Q_CORE_EXPORT jobject callOnBindListener(jobject intent);

}

QT_END_NAMESPACE

Q_DECLARE_METATYPE(QtAndroidPrivate::PermissionsHash)

#endif // QJNIHELPERS_H
