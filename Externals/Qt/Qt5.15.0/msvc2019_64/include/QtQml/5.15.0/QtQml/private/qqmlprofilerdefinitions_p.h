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

#ifndef QQMLPROFILERDEFINITIONS_P_H
#define QQMLPROFILERDEFINITIONS_P_H

#include <private/qtqmlglobal_p.h>
#include <private/qv4profiling_p.h>

QT_REQUIRE_CONFIG(qml_debug);

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

QT_BEGIN_NAMESPACE

struct QQmlProfilerDefinitions {
    enum Message {
        Event,
        RangeStart,
        RangeData,
        RangeLocation,
        RangeEnd,
        Complete, // end of transmission
        PixmapCacheEvent,
        SceneGraphFrame,
        MemoryAllocation,
        DebugMessage,

        MaximumMessage
    };

    enum EventType {
        FramePaint,
        Mouse,
        Key,
        AnimationFrame,
        EndTrace,
        StartTrace,

        MaximumEventType
    };

    enum RangeType {
        Painting,
        Compiling,
        Creating,
        Binding,            //running a binding
        HandlingSignal,     //running a signal handler
        Javascript,

        MaximumRangeType
    };

    enum PixmapEventType {
        PixmapSizeKnown,
        PixmapReferenceCountChanged,
        PixmapCacheCountChanged,
        PixmapLoadingStarted,
        PixmapLoadingFinished,
        PixmapLoadingError,

        MaximumPixmapEventType
    };

    enum SceneGraphFrameType {
        SceneGraphRendererFrame,        // Render Thread
        SceneGraphAdaptationLayerFrame, // Render Thread
        SceneGraphContextFrame,         // Render Thread
        SceneGraphRenderLoopFrame,      // Render Thread
        SceneGraphTexturePrepare,       // Render Thread
        SceneGraphTextureDeletion,      // Render Thread
        SceneGraphPolishAndSync,        // GUI Thread
        SceneGraphWindowsRenderShow,    // Unused
        SceneGraphWindowsAnimations,    // GUI Thread
        SceneGraphPolishFrame,          // GUI Thread

        MaximumSceneGraphFrameType,
        NumRenderThreadFrameTypes = SceneGraphPolishAndSync,
        NumGUIThreadFrameTypes = MaximumSceneGraphFrameType - NumRenderThreadFrameTypes
    };

    enum ProfileFeature {
        ProfileJavaScript,
        ProfileMemory,
        ProfilePixmapCache,
        ProfileSceneGraph,
        ProfileAnimations,
        ProfilePainting,
        ProfileCompiling,
        ProfileCreating,
        ProfileBinding,
        ProfileHandlingSignal,
        ProfileInputEvents,
        ProfileDebugMessages,

        MaximumProfileFeature
    };

    enum InputEventType {
        InputKeyPress,
        InputKeyRelease,
        InputKeyUnknown,

        InputMousePress,
        InputMouseRelease,
        InputMouseMove,
        InputMouseDoubleClick,
        InputMouseWheel,
        InputMouseUnknown,

        MaximumInputEventType
    };
};

QT_END_NAMESPACE

#endif
