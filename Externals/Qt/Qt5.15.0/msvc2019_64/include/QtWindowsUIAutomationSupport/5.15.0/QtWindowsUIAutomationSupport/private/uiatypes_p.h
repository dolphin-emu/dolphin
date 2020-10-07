/****************************************************************************
**
** Copyright (C) 2017 The Qt Company Ltd.
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

#ifndef UIATYPES_H
#define UIATYPES_H

//
//  W A R N I N G
//  -------------
//
// This file is not part of the Qt API. It exists purely as an
// implementation detail. This header file may change from version to
// version without notice, or even be removed.
//
// We mean it.
//

typedef int PROPERTYID;
typedef int PATTERNID;
typedef int EVENTID;
typedef int TEXTATTRIBUTEID;
typedef int CONTROLTYPEID;
typedef int LANDMARKTYPEID;
typedef int METADATAID;

typedef void *UIA_HWND;

enum NavigateDirection {
    NavigateDirection_Parent           = 0,
    NavigateDirection_NextSibling      = 1,
    NavigateDirection_PreviousSibling  = 2,
    NavigateDirection_FirstChild       = 3,
    NavigateDirection_LastChild        = 4
};

enum ProviderOptions {
    ProviderOptions_ClientSideProvider      = 0x1,
    ProviderOptions_ServerSideProvider      = 0x2,
    ProviderOptions_NonClientAreaProvider   = 0x4,
    ProviderOptions_OverrideProvider        = 0x8,
    ProviderOptions_ProviderOwnsSetFocus    = 0x10,
    ProviderOptions_UseComThreading         = 0x20,
    ProviderOptions_RefuseNonClientSupport  = 0x40,
    ProviderOptions_HasNativeIAccessible    = 0x80,
    ProviderOptions_UseClientCoordinates    = 0x100
};

enum SupportedTextSelection {
    SupportedTextSelection_None      = 0,
    SupportedTextSelection_Single    = 1,
    SupportedTextSelection_Multiple  = 2
};

enum TextUnit {
    TextUnit_Character  = 0,
    TextUnit_Format     = 1,
    TextUnit_Word       = 2,
    TextUnit_Line       = 3,
    TextUnit_Paragraph  = 4,
    TextUnit_Page       = 5,
    TextUnit_Document   = 6
};

enum TextPatternRangeEndpoint {
    TextPatternRangeEndpoint_Start  = 0,
    TextPatternRangeEndpoint_End    = 1
};

enum CaretPosition {
    CaretPosition_Unknown           = 0,
    CaretPosition_EndOfLine         = 1,
    CaretPosition_BeginningOfLine   = 2
};

enum ToggleState {
    ToggleState_Off            = 0,
    ToggleState_On             = 1,
    ToggleState_Indeterminate  = 2
};

enum RowOrColumnMajor {
    RowOrColumnMajor_RowMajor       = 0,
    RowOrColumnMajor_ColumnMajor    = 1,
    RowOrColumnMajor_Indeterminate  = 2
};

enum TreeScope {
    TreeScope_None        = 0,
    TreeScope_Element     = 0x1,
    TreeScope_Children    = 0x2,
    TreeScope_Descendants = 0x4,
    TreeScope_Parent      = 0x8,
    TreeScope_Ancestors   = 0x10,
    TreeScope_Subtree     = TreeScope_Element | TreeScope_Children | TreeScope_Descendants
};

enum OrientationType {
    OrientationType_None        = 0,
    OrientationType_Horizontal  = 1,
    OrientationType_Vertical    = 2
};

enum PropertyConditionFlags {
    PropertyConditionFlags_None        = 0,
    PropertyConditionFlags_IgnoreCase  = 1
};

enum WindowVisualState {
    WindowVisualState_Normal    = 0,
    WindowVisualState_Maximized = 1,
    WindowVisualState_Minimized = 2
};

enum WindowInteractionState {
    WindowInteractionState_Running                 = 0,
    WindowInteractionState_Closing                 = 1,
    WindowInteractionState_ReadyForUserInteraction = 2,
    WindowInteractionState_BlockedByModalWindow    = 3,
    WindowInteractionState_NotResponding           = 4
};

enum ExpandCollapseState {
    ExpandCollapseState_Collapsed         = 0,
    ExpandCollapseState_Expanded          = 1,
    ExpandCollapseState_PartiallyExpanded = 2,
    ExpandCollapseState_LeafNode          = 3
};

enum NotificationKind {
    NotificationKind_ItemAdded       = 0,
    NotificationKind_ItemRemoved     = 1,
    NotificationKind_ActionCompleted = 2,
    NotificationKind_ActionAborted   = 3,
    NotificationKind_Other           = 4
};

enum NotificationProcessing {
    NotificationProcessing_ImportantAll          = 0,
    NotificationProcessing_ImportantMostRecent   = 1,
    NotificationProcessing_All                   = 2,
    NotificationProcessing_MostRecent            = 3,
    NotificationProcessing_CurrentThenMostRecent = 4
};

struct UiaRect {
    double left;
    double top;
    double width;
    double height;
};

struct UiaPoint {
    double x;
    double y;
};

#endif
