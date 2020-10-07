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

#ifndef UIASERVERINTERFACES_H
#define UIASERVERINTERFACES_H

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

#include <unknwn.h>

#ifndef __IRawElementProviderSimple_INTERFACE_DEFINED__
#define __IRawElementProviderSimple_INTERFACE_DEFINED__
DEFINE_GUID(IID_IRawElementProviderSimple, 0xd6dd68d1, 0x86fd, 0x4332, 0x86,0x66, 0x9a,0xbe,0xde,0xa2,0xd2,0x4c);
MIDL_INTERFACE("d6dd68d1-86fd-4332-8666-9abedea2d24c")
IRawElementProviderSimple : public IUnknown
{
public:
    virtual HRESULT STDMETHODCALLTYPE get_ProviderOptions(__RPC__out enum ProviderOptions *pRetVal) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetPatternProvider(PATTERNID patternId, __RPC__deref_out_opt IUnknown **pRetVal) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetPropertyValue(PROPERTYID propertyId, __RPC__out VARIANT *pRetVal) = 0;
    virtual HRESULT STDMETHODCALLTYPE get_HostRawElementProvider(__RPC__deref_out_opt IRawElementProviderSimple **pRetVal) = 0;
};
#ifdef __CRT_UUID_DECL
__CRT_UUID_DECL(IRawElementProviderSimple, 0xd6dd68d1, 0x86fd, 0x4332, 0x86,0x66, 0x9a,0xbe,0xde,0xa2,0xd2,0x4c)
#endif
#endif


#ifndef __IRawElementProviderFragmentRoot_FWD_DEFINED__
#define __IRawElementProviderFragmentRoot_FWD_DEFINED__
typedef interface IRawElementProviderFragmentRoot IRawElementProviderFragmentRoot;
#endif


#ifndef __IRawElementProviderFragment_FWD_DEFINED__
#define __IRawElementProviderFragment_FWD_DEFINED__
typedef interface IRawElementProviderFragment IRawElementProviderFragment;
#endif


#ifndef __IRawElementProviderFragment_INTERFACE_DEFINED__
#define __IRawElementProviderFragment_INTERFACE_DEFINED__
DEFINE_GUID(IID_IRawElementProviderFragment, 0xf7063da8, 0x8359, 0x439c, 0x92,0x97, 0xbb,0xc5,0x29,0x9a,0x7d,0x87);
MIDL_INTERFACE("f7063da8-8359-439c-9297-bbc5299a7d87")
IRawElementProviderFragment : public IUnknown
{
public:
    virtual HRESULT STDMETHODCALLTYPE Navigate(enum NavigateDirection direction, __RPC__deref_out_opt IRawElementProviderFragment **pRetVal) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetRuntimeId(__RPC__deref_out_opt SAFEARRAY **pRetVal) = 0;
    virtual HRESULT STDMETHODCALLTYPE get_BoundingRectangle(__RPC__out struct UiaRect *pRetVal) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetEmbeddedFragmentRoots(__RPC__deref_out_opt SAFEARRAY **pRetVal) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetFocus() = 0;
    virtual HRESULT STDMETHODCALLTYPE get_FragmentRoot(__RPC__deref_out_opt IRawElementProviderFragmentRoot **pRetVal) = 0;
};
#ifdef __CRT_UUID_DECL
__CRT_UUID_DECL(IRawElementProviderFragment, 0xf7063da8, 0x8359, 0x439c, 0x92,0x97, 0xbb,0xc5,0x29,0x9a,0x7d,0x87)
#endif
#endif


#ifndef __IRawElementProviderFragmentRoot_INTERFACE_DEFINED__
#define __IRawElementProviderFragmentRoot_INTERFACE_DEFINED__
DEFINE_GUID(IID_IRawElementProviderFragmentRoot, 0x620ce2a5, 0xab8f, 0x40a9, 0x86,0xcb, 0xde,0x3c,0x75,0x59,0x9b,0x58);
MIDL_INTERFACE("620ce2a5-ab8f-40a9-86cb-de3c75599b58")
IRawElementProviderFragmentRoot : public IUnknown
{
public:
    virtual HRESULT STDMETHODCALLTYPE ElementProviderFromPoint(double x, double y, __RPC__deref_out_opt IRawElementProviderFragment **pRetVal) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetFocus(__RPC__deref_out_opt IRawElementProviderFragment **pRetVal) = 0;
};
#ifdef __CRT_UUID_DECL
__CRT_UUID_DECL(IRawElementProviderFragmentRoot, 0x620ce2a5, 0xab8f, 0x40a9, 0x86,0xcb, 0xde,0x3c,0x75,0x59,0x9b,0x58)
#endif
#endif


#ifndef __IValueProvider_INTERFACE_DEFINED__
#define __IValueProvider_INTERFACE_DEFINED__
DEFINE_GUID(IID_IValueProvider, 0xc7935180, 0x6fb3, 0x4201, 0xb1,0x74, 0x7d,0xf7,0x3a,0xdb,0xf6,0x4a);
MIDL_INTERFACE("c7935180-6fb3-4201-b174-7df73adbf64a")
IValueProvider : public IUnknown
{
public:
    virtual HRESULT STDMETHODCALLTYPE SetValue(__RPC__in LPCWSTR val) = 0;
    virtual HRESULT STDMETHODCALLTYPE get_Value(__RPC__deref_out_opt BSTR *pRetVal) = 0;
    virtual HRESULT STDMETHODCALLTYPE get_IsReadOnly(__RPC__out BOOL *pRetVal) = 0;
};
#ifdef __CRT_UUID_DECL
__CRT_UUID_DECL(IValueProvider, 0xc7935180, 0x6fb3, 0x4201, 0xb1,0x74, 0x7d,0xf7,0x3a,0xdb,0xf6,0x4a)
#endif
#endif


#ifndef __IRangeValueProvider_INTERFACE_DEFINED__
#define __IRangeValueProvider_INTERFACE_DEFINED__
DEFINE_GUID(IID_IRangeValueProvider, 0x36dc7aef, 0x33e6, 0x4691, 0xaf,0xe1, 0x2b,0xe7,0x27,0x4b,0x3d,0x33);
MIDL_INTERFACE("36dc7aef-33e6-4691-afe1-2be7274b3d33")
IRangeValueProvider : public IUnknown
{
public:
    virtual HRESULT STDMETHODCALLTYPE SetValue(double val) = 0;
    virtual HRESULT STDMETHODCALLTYPE get_Value(__RPC__out double *pRetVal) = 0;
    virtual HRESULT STDMETHODCALLTYPE get_IsReadOnly(__RPC__out BOOL *pRetVal) = 0;
    virtual HRESULT STDMETHODCALLTYPE get_Maximum(__RPC__out double *pRetVal) = 0;
    virtual HRESULT STDMETHODCALLTYPE get_Minimum(__RPC__out double *pRetVal) = 0;
    virtual HRESULT STDMETHODCALLTYPE get_LargeChange(__RPC__out double *pRetVal) = 0;
    virtual HRESULT STDMETHODCALLTYPE get_SmallChange(__RPC__out double *pRetVal) = 0;
};
#ifdef __CRT_UUID_DECL
__CRT_UUID_DECL(IRangeValueProvider, 0x36dc7aef, 0x33e6, 0x4691, 0xaf,0xe1, 0x2b,0xe7,0x27,0x4b,0x3d,0x33)
#endif
#endif


#ifndef __ITextRangeProvider_INTERFACE_DEFINED__
#define __ITextRangeProvider_INTERFACE_DEFINED__
DEFINE_GUID(IID_ITextRangeProvider, 0x5347ad7b, 0xc355, 0x46f8, 0xaf,0xf5, 0x90,0x90,0x33,0x58,0x2f,0x63);
MIDL_INTERFACE("5347ad7b-c355-46f8-aff5-909033582f63")
ITextRangeProvider : public IUnknown
{
public:
    virtual HRESULT STDMETHODCALLTYPE Clone(__RPC__deref_out_opt ITextRangeProvider **pRetVal) = 0;
    virtual HRESULT STDMETHODCALLTYPE Compare(__RPC__in_opt ITextRangeProvider *range, __RPC__out BOOL *pRetVal) = 0;
    virtual HRESULT STDMETHODCALLTYPE CompareEndpoints(enum TextPatternRangeEndpoint endpoint, __RPC__in_opt ITextRangeProvider *targetRange, enum TextPatternRangeEndpoint targetEndpoint, __RPC__out int *pRetVal) = 0;
    virtual HRESULT STDMETHODCALLTYPE ExpandToEnclosingUnit(enum TextUnit unit) = 0;
    virtual HRESULT STDMETHODCALLTYPE FindAttribute(TEXTATTRIBUTEID attributeId, VARIANT val, BOOL backward, __RPC__deref_out_opt ITextRangeProvider **pRetVal) = 0;
    virtual HRESULT STDMETHODCALLTYPE FindText(__RPC__in BSTR text, BOOL backward, BOOL ignoreCase, __RPC__deref_out_opt ITextRangeProvider **pRetVal) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetAttributeValue(TEXTATTRIBUTEID attributeId, __RPC__out VARIANT *pRetVal) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetBoundingRectangles(__RPC__deref_out_opt SAFEARRAY **pRetVal) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetEnclosingElement(__RPC__deref_out_opt IRawElementProviderSimple **pRetVal) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetText(int maxLength, __RPC__deref_out_opt BSTR *pRetVal) = 0;
    virtual HRESULT STDMETHODCALLTYPE Move(enum TextUnit unit, int count, __RPC__out int *pRetVal) = 0;
    virtual HRESULT STDMETHODCALLTYPE MoveEndpointByUnit(enum TextPatternRangeEndpoint endpoint, enum TextUnit unit, int count, __RPC__out int *pRetVal) = 0;
    virtual HRESULT STDMETHODCALLTYPE MoveEndpointByRange(enum TextPatternRangeEndpoint endpoint, __RPC__in_opt ITextRangeProvider *targetRange, enum TextPatternRangeEndpoint targetEndpoint) = 0;
    virtual HRESULT STDMETHODCALLTYPE Select() = 0;
    virtual HRESULT STDMETHODCALLTYPE AddToSelection() = 0;
    virtual HRESULT STDMETHODCALLTYPE RemoveFromSelection() = 0;
    virtual HRESULT STDMETHODCALLTYPE ScrollIntoView(BOOL alignToTop) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetChildren(__RPC__deref_out_opt SAFEARRAY **pRetVal) = 0;
};
#ifdef __CRT_UUID_DECL
__CRT_UUID_DECL(ITextRangeProvider, 0x5347ad7b, 0xc355, 0x46f8, 0xaf,0xf5, 0x90,0x90,0x33,0x58,0x2f,0x63)
#endif
#endif


#ifndef __ITextProvider_INTERFACE_DEFINED__
#define __ITextProvider_INTERFACE_DEFINED__
DEFINE_GUID(IID_ITextProvider, 0x3589c92c, 0x63f3, 0x4367, 0x99,0xbb, 0xad,0xa6,0x53,0xb7,0x7c,0xf2);
MIDL_INTERFACE("3589c92c-63f3-4367-99bb-ada653b77cf2")
ITextProvider : public IUnknown
{
public:
    virtual HRESULT STDMETHODCALLTYPE GetSelection(__RPC__deref_out_opt SAFEARRAY **pRetVal) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetVisibleRanges(__RPC__deref_out_opt SAFEARRAY **pRetVal) = 0;
    virtual HRESULT STDMETHODCALLTYPE RangeFromChild(__RPC__in_opt IRawElementProviderSimple *childElement, __RPC__deref_out_opt ITextRangeProvider **pRetVal) = 0;
    virtual HRESULT STDMETHODCALLTYPE RangeFromPoint(struct UiaPoint point, __RPC__deref_out_opt ITextRangeProvider **pRetVal) = 0;
    virtual HRESULT STDMETHODCALLTYPE get_DocumentRange(__RPC__deref_out_opt ITextRangeProvider **pRetVal) = 0;
    virtual HRESULT STDMETHODCALLTYPE get_SupportedTextSelection(__RPC__out enum SupportedTextSelection *pRetVal) = 0;
};
#ifdef __CRT_UUID_DECL
__CRT_UUID_DECL(ITextProvider, 0x3589c92c, 0x63f3, 0x4367, 0x99,0xbb, 0xad,0xa6,0x53,0xb7,0x7c,0xf2)
#endif
#endif


#ifndef __ITextProvider2_INTERFACE_DEFINED__
#define __ITextProvider2_INTERFACE_DEFINED__
DEFINE_GUID(IID_ITextProvider2, 0x0dc5e6ed, 0x3e16, 0x4bf1, 0x8f,0x9a, 0xa9,0x79,0x87,0x8b,0xc1,0x95);
MIDL_INTERFACE("0dc5e6ed-3e16-4bf1-8f9a-a979878bc195")
ITextProvider2 : public ITextProvider
{
public:
    virtual HRESULT STDMETHODCALLTYPE RangeFromAnnotation(__RPC__in_opt IRawElementProviderSimple *annotationElement, __RPC__deref_out_opt ITextRangeProvider **pRetVal) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetCaretRange(__RPC__out BOOL *isActive, __RPC__deref_out_opt ITextRangeProvider **pRetVal) = 0;
};
#ifdef __CRT_UUID_DECL
__CRT_UUID_DECL(ITextProvider2, 0x0dc5e6ed, 0x3e16, 0x4bf1, 0x8f,0x9a, 0xa9,0x79,0x87,0x8b,0xc1,0x95)
#endif
#endif


#ifndef __IToggleProvider_INTERFACE_DEFINED__
#define __IToggleProvider_INTERFACE_DEFINED__
DEFINE_GUID(IID_IToggleProvider, 0x56d00bd0, 0xc4f4, 0x433c, 0xa8,0x36, 0x1a,0x52,0xa5,0x7e,0x08,0x92);
MIDL_INTERFACE("56d00bd0-c4f4-433c-a836-1a52a57e0892")
IToggleProvider : public IUnknown
{
public:
    virtual HRESULT STDMETHODCALLTYPE Toggle() = 0;
    virtual HRESULT STDMETHODCALLTYPE get_ToggleState(__RPC__out enum ToggleState *pRetVal) = 0;
};
#ifdef __CRT_UUID_DECL
__CRT_UUID_DECL(IToggleProvider, 0x56d00bd0, 0xc4f4, 0x433c, 0xa8,0x36, 0x1a,0x52,0xa5,0x7e,0x08,0x92)
#endif
#endif


#ifndef __IInvokeProvider_INTERFACE_DEFINED__
#define __IInvokeProvider_INTERFACE_DEFINED__
DEFINE_GUID(IID_IInvokeProvider, 0x54fcb24b, 0xe18e, 0x47a2, 0xb4,0xd3, 0xec,0xcb,0xe7,0x75,0x99,0xa2);
MIDL_INTERFACE("54fcb24b-e18e-47a2-b4d3-eccbe77599a2")
IInvokeProvider : public IUnknown
{
public:
    virtual HRESULT STDMETHODCALLTYPE Invoke() = 0;
};
#ifdef __CRT_UUID_DECL
__CRT_UUID_DECL(IInvokeProvider, 0x54fcb24b, 0xe18e, 0x47a2, 0xb4,0xd3, 0xec,0xcb,0xe7,0x75,0x99,0xa2)
#endif
#endif


#ifndef __ISelectionProvider_INTERFACE_DEFINED__
#define __ISelectionProvider_INTERFACE_DEFINED__
DEFINE_GUID(IID_ISelectionProvider, 0xfb8b03af, 0x3bdf, 0x48d4, 0xbd,0x36, 0x1a,0x65,0x79,0x3b,0xe1,0x68);
MIDL_INTERFACE("fb8b03af-3bdf-48d4-bd36-1a65793be168")
ISelectionProvider : public IUnknown
{
public:
    virtual HRESULT STDMETHODCALLTYPE GetSelection(__RPC__deref_out_opt SAFEARRAY **pRetVal) = 0;
    virtual HRESULT STDMETHODCALLTYPE get_CanSelectMultiple(__RPC__out BOOL *pRetVal) = 0;
    virtual HRESULT STDMETHODCALLTYPE get_IsSelectionRequired(__RPC__out BOOL *pRetVal) = 0;
};
#ifdef __CRT_UUID_DECL
__CRT_UUID_DECL(ISelectionProvider, 0xfb8b03af, 0x3bdf, 0x48d4, 0xbd,0x36, 0x1a,0x65,0x79,0x3b,0xe1,0x68)
#endif
#endif


#ifndef __ISelectionItemProvider_INTERFACE_DEFINED__
#define __ISelectionItemProvider_INTERFACE_DEFINED__
DEFINE_GUID(IID_ISelectionItemProvider, 0x2acad808, 0xb2d4, 0x452d, 0xa4,0x07, 0x91,0xff,0x1a,0xd1,0x67,0xb2);
MIDL_INTERFACE("2acad808-b2d4-452d-a407-91ff1ad167b2")
ISelectionItemProvider : public IUnknown
{
public:
    virtual HRESULT STDMETHODCALLTYPE Select() = 0;
    virtual HRESULT STDMETHODCALLTYPE AddToSelection() = 0;
    virtual HRESULT STDMETHODCALLTYPE RemoveFromSelection() = 0;
    virtual HRESULT STDMETHODCALLTYPE get_IsSelected(__RPC__out BOOL *pRetVal) = 0;
    virtual HRESULT STDMETHODCALLTYPE get_SelectionContainer(__RPC__deref_out_opt IRawElementProviderSimple **pRetVal) = 0;
};
#ifdef __CRT_UUID_DECL
__CRT_UUID_DECL(ISelectionItemProvider, 0x2acad808, 0xb2d4, 0x452d, 0xa4,0x07, 0x91,0xff,0x1a,0xd1,0x67,0xb2)
#endif
#endif


#ifndef __ITableProvider_INTERFACE_DEFINED__
#define __ITableProvider_INTERFACE_DEFINED__
DEFINE_GUID(IID_ITableProvider, 0x9c860395, 0x97b3, 0x490a, 0xb5,0x2a, 0x85,0x8c,0xc2,0x2a,0xf1,0x66);
MIDL_INTERFACE("9c860395-97b3-490a-b52a-858cc22af166")
ITableProvider : public IUnknown
{
public:
    virtual HRESULT STDMETHODCALLTYPE GetRowHeaders(__RPC__deref_out_opt SAFEARRAY **pRetVal) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetColumnHeaders(__RPC__deref_out_opt SAFEARRAY **pRetVal) = 0;
    virtual HRESULT STDMETHODCALLTYPE get_RowOrColumnMajor(__RPC__out enum RowOrColumnMajor *pRetVal) = 0;
};
#ifdef __CRT_UUID_DECL
__CRT_UUID_DECL(ITableProvider, 0x9c860395, 0x97b3, 0x490a, 0xb5,0x2a, 0x85,0x8c,0xc2,0x2a,0xf1,0x66)
#endif
#endif


#ifndef __ITableItemProvider_INTERFACE_DEFINED__
#define __ITableItemProvider_INTERFACE_DEFINED__
DEFINE_GUID(IID_ITableItemProvider, 0xb9734fa6, 0x771f, 0x4d78, 0x9c,0x90, 0x25,0x17,0x99,0x93,0x49,0xcd);
MIDL_INTERFACE("b9734fa6-771f-4d78-9c90-2517999349cd")
ITableItemProvider : public IUnknown
{
public:
    virtual HRESULT STDMETHODCALLTYPE GetRowHeaderItems(__RPC__deref_out_opt SAFEARRAY **pRetVal) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetColumnHeaderItems(__RPC__deref_out_opt SAFEARRAY **pRetVal) = 0;
};
#ifdef __CRT_UUID_DECL
__CRT_UUID_DECL(ITableItemProvider, 0xb9734fa6, 0x771f, 0x4d78, 0x9c,0x90, 0x25,0x17,0x99,0x93,0x49,0xcd)
#endif
#endif


#ifndef __IGridProvider_INTERFACE_DEFINED__
#define __IGridProvider_INTERFACE_DEFINED__
DEFINE_GUID(IID_IGridProvider, 0xb17d6187, 0x0907, 0x464b, 0xa1,0x68, 0x0e,0xf1,0x7a,0x15,0x72,0xb1);
MIDL_INTERFACE("b17d6187-0907-464b-a168-0ef17a1572b1")
IGridProvider : public IUnknown
{
public:
    virtual HRESULT STDMETHODCALLTYPE GetItem(int row, int column, __RPC__deref_out_opt IRawElementProviderSimple **pRetVal) = 0;
    virtual HRESULT STDMETHODCALLTYPE get_RowCount(__RPC__out int *pRetVal) = 0;
    virtual HRESULT STDMETHODCALLTYPE get_ColumnCount(__RPC__out int *pRetVal) = 0;
};
#ifdef __CRT_UUID_DECL
__CRT_UUID_DECL(IGridProvider, 0xb17d6187, 0x0907, 0x464b, 0xa1,0x68, 0x0e,0xf1,0x7a,0x15,0x72,0xb1)
#endif
#endif


#ifndef __IGridItemProvider_INTERFACE_DEFINED__
#define __IGridItemProvider_INTERFACE_DEFINED__
DEFINE_GUID(IID_IGridItemProvider, 0xd02541f1, 0xfb81, 0x4d64, 0xae,0x32, 0xf5,0x20,0xf8,0xa6,0xdb,0xd1);
MIDL_INTERFACE("d02541f1-fb81-4d64-ae32-f520f8a6dbd1")
IGridItemProvider : public IUnknown
{
public:
    virtual HRESULT STDMETHODCALLTYPE get_Row(__RPC__out int *pRetVal) = 0;
    virtual HRESULT STDMETHODCALLTYPE get_Column(__RPC__out int *pRetVal) = 0;
    virtual HRESULT STDMETHODCALLTYPE get_RowSpan(__RPC__out int *pRetVal) = 0;
    virtual HRESULT STDMETHODCALLTYPE get_ColumnSpan(__RPC__out int *pRetVal) = 0;
    virtual HRESULT STDMETHODCALLTYPE get_ContainingGrid(__RPC__deref_out_opt IRawElementProviderSimple **pRetVal) = 0;
};
#ifdef __CRT_UUID_DECL
__CRT_UUID_DECL(IGridItemProvider, 0xd02541f1, 0xfb81, 0x4d64, 0xae,0x32, 0xf5,0x20,0xf8,0xa6,0xdb,0xd1)
#endif
#endif


#ifndef __IWindowProvider_INTERFACE_DEFINED__
#define __IWindowProvider_INTERFACE_DEFINED__
DEFINE_GUID(IID_IWindowProvider, 0x987df77b, 0xdb06, 0x4d77, 0x8f,0x8a, 0x86,0xa9,0xc3,0xbb,0x90,0xb9);
MIDL_INTERFACE("987df77b-db06-4d77-8f8a-86a9c3bb90b9")
IWindowProvider : public IUnknown
{
public:
    virtual HRESULT STDMETHODCALLTYPE SetVisualState(enum WindowVisualState state) = 0;
    virtual HRESULT STDMETHODCALLTYPE Close( void) = 0;
    virtual HRESULT STDMETHODCALLTYPE WaitForInputIdle(int milliseconds, __RPC__out BOOL *pRetVal) = 0;
    virtual HRESULT STDMETHODCALLTYPE get_CanMaximize(__RPC__out BOOL *pRetVal) = 0;
    virtual HRESULT STDMETHODCALLTYPE get_CanMinimize(__RPC__out BOOL *pRetVal) = 0;
    virtual HRESULT STDMETHODCALLTYPE get_IsModal(__RPC__out BOOL *pRetVal) = 0;
    virtual HRESULT STDMETHODCALLTYPE get_WindowVisualState(__RPC__out enum WindowVisualState *pRetVal) = 0;
    virtual HRESULT STDMETHODCALLTYPE get_WindowInteractionState(__RPC__out enum WindowInteractionState *pRetVal) = 0;
    virtual HRESULT STDMETHODCALLTYPE get_IsTopmost(__RPC__out BOOL *pRetVal) = 0;
};
#ifdef __CRT_UUID_DECL
__CRT_UUID_DECL(IWindowProvider, 0x987df77b, 0xdb06, 0x4d77, 0x8f,0x8a, 0x86,0xa9,0xc3,0xbb,0x90,0xb9)
#endif
#endif


#ifndef __IExpandCollapseProvider_INTERFACE_DEFINED__
#define __IExpandCollapseProvider_INTERFACE_DEFINED__
DEFINE_GUID(IID_IExpandCollapseProvider, 0xd847d3a5, 0xcab0, 0x4a98, 0x8c,0x32, 0xec,0xb4,0x5c,0x59,0xad,0x24);
MIDL_INTERFACE("d847d3a5-cab0-4a98-8c32-ecb45c59ad24")
IExpandCollapseProvider : public IUnknown
{
public:
    virtual HRESULT STDMETHODCALLTYPE Expand() = 0;
    virtual HRESULT STDMETHODCALLTYPE Collapse() = 0;
    virtual HRESULT STDMETHODCALLTYPE get_ExpandCollapseState(__RPC__out enum ExpandCollapseState *pRetVal) = 0;
};
#ifdef __CRT_UUID_DECL
__CRT_UUID_DECL(IExpandCollapseProvider, 0xd847d3a5, 0xcab0, 0x4a98, 0x8c,0x32, 0xec,0xb4,0x5c,0x59,0xad,0x24)
#endif
#endif

#endif
