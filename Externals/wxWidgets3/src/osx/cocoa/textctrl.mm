/////////////////////////////////////////////////////////////////////////////
// Name:        src/osx/cocoa/textctrl.mm
// Purpose:     wxTextCtrl
// Author:      Stefan Csomor
// Modified by: Ryan Norton (MLTE GetLineLength and GetLineText)
// Created:     1998-01-01
// RCS-ID:      $Id: textctrl.mm 67232 2011-03-18 15:10:15Z DS $
// Copyright:   (c) Stefan Csomor
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#include "wx/wxprec.h"

#if wxUSE_TEXTCTRL

#include "wx/textctrl.h"

#ifndef WX_PRECOMP
    #include "wx/intl.h"
    #include "wx/app.h"
    #include "wx/utils.h"
    #include "wx/dc.h"
    #include "wx/button.h"
    #include "wx/menu.h"
    #include "wx/settings.h"
    #include "wx/msgdlg.h"
    #include "wx/toplevel.h"
#endif

#ifdef __DARWIN__
    #include <sys/types.h>
    #include <sys/stat.h>
#else
    #include <stat.h>
#endif

#if wxUSE_STD_IOSTREAM
    #if wxUSE_IOSTREAMH
        #include <fstream.h>
    #else
        #include <fstream>
    #endif
#endif

#include "wx/filefn.h"
#include "wx/sysopt.h"
#include "wx/thread.h"

#include "wx/osx/private.h"
#include "wx/osx/cocoa/private/textimpl.h"

@interface NSView(EditableView)
- (BOOL)isEditable;
- (void)setEditable:(BOOL)flag;
- (BOOL)isSelectable;
- (void)setSelectable:(BOOL)flag;
@end

class wxMacEditHelper
{
public :
    wxMacEditHelper( NSView* textView )
    {
        m_textView = textView;
        m_formerEditable = YES;
        if ( textView )
        {
            m_formerEditable = [textView isEditable];
            m_formerSelectable = [textView isSelectable];
            [textView setEditable:YES];
        }
    }

    ~wxMacEditHelper()
    {
        if ( m_textView )
        {
            [m_textView setEditable:m_formerEditable];
            [m_textView setSelectable:m_formerSelectable];
        }
    }

protected :
    BOOL m_formerEditable ;
    BOOL m_formerSelectable;
    NSView* m_textView;
} ;

@implementation wxNSSecureTextField

+ (void)initialize
{
    static BOOL initialized = NO;
    if (!initialized)
    {
        initialized = YES;
        wxOSXCocoaClassAddWXMethods( self );
    }
}

- (void)controlTextDidChange:(NSNotification *)aNotification
{
    wxUnusedVar(aNotification);
    wxWidgetCocoaImpl* impl = (wxWidgetCocoaImpl* ) wxWidgetImpl::FindFromWXWidget( self );
    if ( impl )
        impl->controlTextDidChange();
}

- (void)controlTextDidEndEditing:(NSNotification *)aNotification
{
    wxUnusedVar(aNotification);
    wxWidgetCocoaImpl* impl = (wxWidgetCocoaImpl* ) wxWidgetImpl::FindFromWXWidget( self );
    if ( impl )
    {
        impl->DoNotifyFocusEvent( false, NULL );
    }
}

@end

@interface wxNSTextScrollView : NSScrollView
{
}
@end

@implementation wxNSTextScrollView

+ (void)initialize
{
    static BOOL initialized = NO;
    if (!initialized)
    {
        initialized = YES;
        wxOSXCocoaClassAddWXMethods( self );
    }
}

@end

@implementation wxNSTextFieldEditor

- (void) keyDown:(NSEvent*) event
{
    wxWidgetCocoaImpl* impl = (wxWidgetCocoaImpl* ) wxWidgetImpl::FindFromWXWidget( (WXWidget) [self delegate] );
    lastKeyDownEvent = event;
    if ( impl == NULL || !impl->DoHandleKeyEvent(event) )
        [super keyDown:event];
    lastKeyDownEvent = nil;
}

- (void) keyUp:(NSEvent*) event
{
    wxWidgetCocoaImpl* impl = (wxWidgetCocoaImpl* ) wxWidgetImpl::FindFromWXWidget( (WXWidget) [self delegate] );
    if ( impl == NULL || !impl->DoHandleKeyEvent(event) )
        [super keyUp:event];
}

- (void) flagsChanged:(NSEvent*) event
{
    wxWidgetCocoaImpl* impl = (wxWidgetCocoaImpl* ) wxWidgetImpl::FindFromWXWidget( (WXWidget) [self delegate] );
    if ( impl == NULL || !impl->DoHandleKeyEvent(event) )
        [super flagsChanged:event];
}

- (BOOL) performKeyEquivalent:(NSEvent*) event
{
    BOOL retval = [super performKeyEquivalent:event];
    return retval;
}

- (void) insertText:(id) str
{
    wxWidgetCocoaImpl* impl = (wxWidgetCocoaImpl* ) wxWidgetImpl::FindFromWXWidget( (WXWidget) [self delegate] );
    if ( impl == NULL || lastKeyDownEvent==nil || !impl->DoHandleCharEvent(lastKeyDownEvent, str) )
    {
        [super insertText:str];
    }
}

@end

@implementation wxNSTextView

+ (void)initialize
{
    static BOOL initialized = NO;
    if (!initialized)
    {
        initialized = YES;
        wxOSXCocoaClassAddWXMethods( self );
    }
}

- (void)textDidChange:(NSNotification *)aNotification
{
    wxUnusedVar(aNotification);
    wxWidgetCocoaImpl* impl = (wxWidgetCocoaImpl* ) wxWidgetImpl::FindFromWXWidget( self );
    if ( impl )
        impl->controlTextDidChange();
}

- (void) setEnabled:(BOOL) flag
{
    // from Technical Q&A QA1461
    if (flag) {
        [self setTextColor: [NSColor controlTextColor]];

    } else {
        [self setTextColor: [NSColor disabledControlTextColor]];
    }

    [self setSelectable: flag];
    [self setEditable: flag];
}

- (BOOL) isEnabled
{
    return [self isEditable];
}

- (void)textDidEndEditing:(NSNotification *)aNotification
{
    wxUnusedVar(aNotification);
    wxWidgetCocoaImpl* impl = (wxWidgetCocoaImpl* ) wxWidgetImpl::FindFromWXWidget( self );
    if ( impl )
    {
        impl->DoNotifyFocusEvent( false, NULL );
    }
}

@end

@implementation wxNSTextField

+ (void)initialize
{
    static BOOL initialized = NO;
    if (!initialized)
    {
        initialized = YES;
        wxOSXCocoaClassAddWXMethods( self );
    }
}

- (id) initWithFrame:(NSRect) frame
{
    self = [super initWithFrame:frame];
    fieldEditor = nil;
    return self;
}

- (void) dealloc
{
    [fieldEditor release];
    [super dealloc];
}

- (void) setFieldEditor:(wxNSTextFieldEditor*) editor
{
    fieldEditor = editor;
}

- (wxNSTextFieldEditor*) fieldEditor
{
    return fieldEditor;
}

- (void) setEnabled:(BOOL) flag
{
    [super setEnabled: flag];

    if (![self drawsBackground]) {
        // Static text is drawn incorrectly when disabled.
        // For an explanation, see
        // http://www.cocoabuilder.com/archive/message/cocoa/2006/7/21/168028
        if (flag) {
            [self setTextColor: [NSColor controlTextColor]];
        } else {
            [self setTextColor: [NSColor secondarySelectedControlColor]];
        }
    }
}

- (void)controlTextDidChange:(NSNotification *)aNotification
{
    wxUnusedVar(aNotification);
    wxWidgetCocoaImpl* impl = (wxWidgetCocoaImpl* ) wxWidgetImpl::FindFromWXWidget( self );
    if ( impl )
        impl->controlTextDidChange();
}

- (BOOL)control:(NSControl*)control textView:(NSTextView*)textView doCommandBySelector:(SEL)commandSelector
{
    wxUnusedVar(textView);
    wxUnusedVar(control);
    
    BOOL handled = NO;

    // send back key events wx' common code knows how to handle
    
    wxWidgetCocoaImpl* impl = (wxWidgetCocoaImpl* ) wxWidgetImpl::FindFromWXWidget( self );
    if ( impl  )
    {
        wxWindow* wxpeer = (wxWindow*) impl->GetWXPeer();
        if ( wxpeer )
        {
            if (commandSelector == @selector(insertNewline:))
            {
                [textView insertNewlineIgnoringFieldEditor:self];
                handled = YES;
            }
            else if ( commandSelector == @selector(insertTab:))
            {
                [textView insertTabIgnoringFieldEditor:self];
                handled = YES;
            }
            else if ( commandSelector == @selector(insertBacktab:))
            {
                [textView insertTabIgnoringFieldEditor:self];
                handled = YES;
            }
        }
    }
    
    return handled;
}

- (void)controlTextDidEndEditing:(NSNotification *)aNotification
{
    wxUnusedVar(aNotification);
    wxWidgetCocoaImpl* impl = (wxWidgetCocoaImpl* ) wxWidgetImpl::FindFromWXWidget( self );
    if ( impl )
    {
        impl->DoNotifyFocusEvent( false, NULL );
    }
}
@end

// wxNSTextViewControl

wxNSTextViewControl::wxNSTextViewControl( wxTextCtrl *wxPeer, WXWidget w )
    : wxWidgetCocoaImpl(wxPeer, w),
      wxTextWidgetImpl(wxPeer)
{
    wxNSTextScrollView* sv = (wxNSTextScrollView*) w;
    m_scrollView = sv;

    [m_scrollView setHasVerticalScroller:YES];
    [m_scrollView setHasHorizontalScroller:NO];
    [m_scrollView setAutoresizingMask:NSViewWidthSizable | NSViewHeightSizable];
    NSSize contentSize = [m_scrollView contentSize];

    wxNSTextView* tv = [[wxNSTextView alloc] initWithFrame: NSMakeRect(0, 0,
            contentSize.width, contentSize.height)];
    m_textView = tv;
    [tv setVerticallyResizable:YES];
    [tv setHorizontallyResizable:NO];
    [tv setAutoresizingMask:NSViewWidthSizable];

    [m_scrollView setDocumentView: tv];

    [tv setDelegate: tv];

    InstallEventHandler(tv);
}

wxNSTextViewControl::~wxNSTextViewControl()
{
    if (m_textView)
        [m_textView setDelegate: nil];
}

bool wxNSTextViewControl::CanFocus() const
{
    // since this doesn't work (return false), we hardcode
    // if (m_textView)
    //    return [m_textView canBecomeKeyView];
    return true;
}

wxString wxNSTextViewControl::GetStringValue() const
{
    if (m_textView)
    {
        wxString result = wxCFStringRef::AsString([m_textView string], m_wxPeer->GetFont().GetEncoding());
        wxMacConvertNewlines13To10( &result ) ;
        return result;
    }
    return wxEmptyString;
}
void wxNSTextViewControl::SetStringValue( const wxString &str)
{
    wxString st = str;
    wxMacConvertNewlines10To13( &st );
    wxMacEditHelper helper(m_textView);

    if (m_textView)
        [m_textView setString: wxCFStringRef( st , m_wxPeer->GetFont().GetEncoding() ).AsNSString()];
}

void wxNSTextViewControl::Copy()
{
    if (m_textView)
        [m_textView copy:nil];

}

void wxNSTextViewControl::Cut()
{
    if (m_textView)
        [m_textView cut:nil];
}

void wxNSTextViewControl::Paste()
{
    if (m_textView)
        [m_textView paste:nil];
}

bool wxNSTextViewControl::CanPaste() const
{
    return true;
}

void wxNSTextViewControl::SetEditable(bool editable)
{
    if (m_textView)
        [m_textView setEditable: editable];
}

void wxNSTextViewControl::GetSelection( long* from, long* to) const
{
    if (m_textView)
    {
        NSRange range = [m_textView selectedRange];
        *from = range.location;
        *to = range.location + range.length;
    }
}

void wxNSTextViewControl::SetSelection( long from , long to )
{
    long textLength = [[m_textView string] length];
    if ((from == -1) && (to == -1))
    {
        from = 0 ;
        to = textLength ;
    }
    else
    {
        from = wxMin(textLength,wxMax(from,0)) ;
        if ( to == -1 )
            to = textLength;
        else
            to = wxMax(0,wxMin(textLength,to)) ;
    }

    NSRange selrange = NSMakeRange(from, to-from);
    [m_textView setSelectedRange:selrange];
    [m_textView scrollRangeToVisible:selrange];
}

void wxNSTextViewControl::WriteText(const wxString& str)
{
    wxString st = str;
    wxMacConvertNewlines10To13( &st );
    wxMacEditHelper helper(m_textView);
    NSEvent* formerEvent = m_lastKeyDownEvent;
    m_lastKeyDownEvent = nil;
    [m_textView insertText:wxCFStringRef( st , m_wxPeer->GetFont().GetEncoding() ).AsNSString()];
    m_lastKeyDownEvent = formerEvent;
}

void wxNSTextViewControl::SetFont( const wxFont & font , const wxColour& WXUNUSED(foreground) , long WXUNUSED(windowStyle), bool WXUNUSED(ignoreBlack) )
{
    if ([m_textView respondsToSelector:@selector(setFont:)])
        [m_textView setFont: font.OSXGetNSFont()];
}

bool wxNSTextViewControl::GetStyle(long position, wxTextAttr& style)
{
    if (m_textView && position >=0)
    {   
        NSFont* font = NULL;
        NSColor* bgcolor = NULL;
        NSColor* fgcolor = NULL;
        // NOTE: It appears that other platforms accept GetStyle with the position == length
        // but that NSTextStorage does not accept length as a valid position.
        // Therefore we return the default control style in that case.
        if (position < (long) [[m_textView string] length]) 
        {
            NSTextStorage* storage = [m_textView textStorage];
            font = [[storage attribute:NSFontAttributeName atIndex:position effectiveRange:NULL] autorelease];
            bgcolor = [[storage attribute:NSBackgroundColorAttributeName atIndex:position effectiveRange:NULL] autorelease];
            fgcolor = [[storage attribute:NSForegroundColorAttributeName atIndex:position effectiveRange:NULL] autorelease];
        }
        else
        {
            NSDictionary* attrs = [m_textView typingAttributes];
            font = [[attrs objectForKey:NSFontAttributeName] autorelease];
            bgcolor = [[attrs objectForKey:NSBackgroundColorAttributeName] autorelease];
            fgcolor = [[attrs objectForKey:NSForegroundColorAttributeName] autorelease];
        }
        
        if (font)
            style.SetFont(wxFont(font));
        
        if (bgcolor)
            style.SetBackgroundColour(wxColour(bgcolor));
            
        if (fgcolor)
            style.SetTextColour(wxColour(fgcolor));
        return true;
    }

    return false;
}

void wxNSTextViewControl::SetStyle(long start,
                                long end,
                                const wxTextAttr& style)
{
    if (m_textView) {
        NSRange range = NSMakeRange(start, end-start);
        if (start == -1 && end == -1)
            range = [m_textView selectedRange];

        NSTextStorage* storage = [m_textView textStorage];
        
        wxFont font = style.GetFont();
        if (style.HasFont() && font.IsOk())
            [storage addAttribute:NSFontAttributeName value:font.OSXGetNSFont() range:range];
        
        wxColour bgcolor = style.GetBackgroundColour();
        if (style.HasBackgroundColour() && bgcolor.IsOk())
            [storage addAttribute:NSBackgroundColorAttributeName value:bgcolor.OSXGetNSColor() range:range];
        
        wxColour fgcolor = style.GetTextColour();
        if (style.HasTextColour() && fgcolor.IsOk())
            [storage addAttribute:NSForegroundColorAttributeName value:fgcolor.OSXGetNSColor() range:range];
    }
}

void wxNSTextViewControl::CheckSpelling(bool check)
{
    if (m_textView)
        [m_textView setContinuousSpellCheckingEnabled: check];
}

wxSize wxNSTextViewControl::GetBestSize() const
{
    if (m_textView && [m_textView layoutManager])
    {
        NSRect rect = [[m_textView layoutManager] usedRectForTextContainer: [m_textView textContainer]];
        return wxSize((int)(rect.size.width + [m_textView textContainerInset].width),
                      (int)(rect.size.height + [m_textView textContainerInset].height));
    }
    return wxSize(0,0);
}

// wxNSTextFieldControl

wxNSTextFieldControl::wxNSTextFieldControl( wxTextCtrl *text, WXWidget w )
    : wxWidgetCocoaImpl(text, w),
      wxTextWidgetImpl(text)
{
    Init(w);
}

wxNSTextFieldControl::wxNSTextFieldControl(wxWindow *wxPeer,
                                           wxTextEntry *entry,
                                           WXWidget w)
    : wxWidgetCocoaImpl(wxPeer, w),
      wxTextWidgetImpl(entry)
{
    Init(w);
}

void wxNSTextFieldControl::Init(WXWidget w)
{
    NSTextField wxOSX_10_6_AND_LATER(<NSTextFieldDelegate>) *tf = (NSTextField*) w;
    m_textField = tf;
    [m_textField setDelegate: tf];
    m_selStart = m_selEnd = 0;
    m_hasEditor = [w isKindOfClass:[NSTextField class]];
}

wxNSTextFieldControl::~wxNSTextFieldControl()
{
    if (m_textField)
        [m_textField setDelegate: nil];
}

wxString wxNSTextFieldControl::GetStringValue() const
{
    return wxCFStringRef::AsString([m_textField stringValue], m_wxPeer->GetFont().GetEncoding());
}

void wxNSTextFieldControl::SetStringValue( const wxString &str)
{
    wxMacEditHelper helper(m_textField);
    [m_textField setStringValue: wxCFStringRef( str , m_wxPeer->GetFont().GetEncoding() ).AsNSString()];
}

void wxNSTextFieldControl::Copy()
{
    NSText* editor = [m_textField currentEditor];
    if ( editor )
    {
        [editor copy:nil];
    }
}

void wxNSTextFieldControl::Cut()
{
    NSText* editor = [m_textField currentEditor];
    if ( editor )
    {
        [editor cut:nil];
    }
}

void wxNSTextFieldControl::Paste()
{
    NSText* editor = [m_textField currentEditor];
    if ( editor )
    {
        [editor paste:nil];
    }
}

bool wxNSTextFieldControl::CanPaste() const
{
    return true;
}

void wxNSTextFieldControl::SetEditable(bool editable)
{
    [m_textField setEditable:editable];
}

void wxNSTextFieldControl::GetSelection( long* from, long* to) const
{
    NSText* editor = [m_textField currentEditor];
    if ( editor )
    {
        NSRange range = [editor selectedRange];
        *from = range.location;
        *to = range.location + range.length;
    }
    else
    {
        *from = m_selStart;
        *to = m_selEnd;
    }
}

void wxNSTextFieldControl::SetSelection( long from , long to )
{
    long textLength = [[m_textField stringValue] length];
    if ((from == -1) && (to == -1))
    {
        from = 0 ;
        to = textLength ;
    }
    else
    {
        from = wxMin(textLength,wxMax(from,0)) ;
        if ( to == -1 )
            to = textLength;
        else
            to = wxMax(0,wxMin(textLength,to)) ;
    }

    NSText* editor = [m_textField currentEditor];
    if ( editor )
    {
        [editor setSelectedRange:NSMakeRange(from, to-from)];
    }
    else
    {
        m_selStart = from;
        m_selEnd = to;
    }
}

void wxNSTextFieldControl::WriteText(const wxString& str)
{
    NSEvent* formerEvent = m_lastKeyDownEvent;
    m_lastKeyDownEvent = nil;
    NSText* editor = [m_textField currentEditor];
    if ( editor )
    {
        wxMacEditHelper helper(m_textField);
        [editor insertText:wxCFStringRef( str , m_wxPeer->GetFont().GetEncoding() ).AsNSString()];
    }
    else
    {
        wxString val = GetStringValue() ;
        long start , end ;
        GetSelection( &start , &end ) ;
        val.Remove( start , end - start ) ;
        val.insert( start , str ) ;
        SetStringValue( val ) ;
        SetSelection( start + str.length() , start + str.length() ) ;
    }
    m_lastKeyDownEvent = formerEvent;
}

void wxNSTextFieldControl::controlAction(WXWidget WXUNUSED(slf),
    void* WXUNUSED(_cmd), void *WXUNUSED(sender))
{
    wxWindow* wxpeer = (wxWindow*) GetWXPeer();
    if ( wxpeer && (wxpeer->GetWindowStyle() & wxTE_PROCESS_ENTER) )
    {
        wxCommandEvent event(wxEVT_COMMAND_TEXT_ENTER, wxpeer->GetId());
        event.SetEventObject( wxpeer );
        event.SetString( GetTextEntry()->GetValue() );
        wxpeer->HandleWindowEvent( event );
    }
}

bool wxNSTextFieldControl::SetHint(const wxString& hint)
{
    wxCFStringRef hintstring(hint);
    [[m_textField cell] setPlaceholderString:hintstring.AsNSString()];
    return true;
}

//
//
//

wxWidgetImplType* wxWidgetImpl::CreateTextControl( wxTextCtrl* wxpeer,
                                    wxWindowMac* WXUNUSED(parent),
                                    wxWindowID WXUNUSED(id),
                                    const wxString& WXUNUSED(str),
                                    const wxPoint& pos,
                                    const wxSize& size,
                                    long style,
                                    long WXUNUSED(extraStyle))
{
    NSRect r = wxOSXGetFrameForControl( wxpeer, pos , size ) ;
    wxWidgetCocoaImpl* c = NULL;

    if ( style & wxTE_MULTILINE || style & wxTE_RICH || style & wxTE_RICH2 )
    {
        wxNSTextScrollView* v = nil;
        v = [[wxNSTextScrollView alloc] initWithFrame:r];
        c = new wxNSTextViewControl( wxpeer, v );
    }
    else
    {
        NSTextField* v = nil;
        if ( style & wxTE_PASSWORD )
            v = [[wxNSSecureTextField alloc] initWithFrame:r];
        else
            v = [[wxNSTextField alloc] initWithFrame:r];

        if ( style & wxNO_BORDER )
        {
            // FIXME: How can we remove the native control's border?
            // setBordered is separate from the text ctrl's border.
        }
        
        NSTextFieldCell* cell = [v cell];
        [cell setScrollable:YES];
        // TODO: Remove if we definitely are sure, it's not needed
        // as setting scrolling to yes, should turn off any wrapping
        // [cell setLineBreakMode:NSLineBreakByClipping]; 

        [v setBezeled:NO];
        [v setBordered:NO];

        c = new wxNSTextFieldControl( wxpeer, wxpeer, v );
    }
    c->SetNeedsFocusRect( true );

    return c;
}


#endif // wxUSE_TEXTCTRL
