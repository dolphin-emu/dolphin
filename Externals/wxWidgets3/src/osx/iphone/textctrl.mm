/////////////////////////////////////////////////////////////////////////////
// Name:        src/osx/iphone/textctrl.mm
// Purpose:     wxTextCtrl
// Author:      Stefan Csomor
// Modified by: Ryan Norton (MLTE GetLineLength and GetLineText)
// Created:     1998-01-01
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
#include "wx/osx/iphone/private/textimpl.h"

// currently for some reasong the UITextField leads to a recursion when the keyboard should be shown, so let's leave the code
// in case this gets resolved...

#define wxOSX_IPHONE_USE_TEXTFIELD 1

class wxMacEditHelper
{
public :
    wxMacEditHelper( UITextView* textView )
    {
        m_textView = textView;
        m_formerState = YES;
        if ( textView )
        {
            m_formerState = [textView isEditable];
            [textView setEditable:YES];
        }
    }

    ~wxMacEditHelper()
    {
        if ( m_textView )
            [m_textView setEditable:m_formerState];
    }

protected :
    BOOL m_formerState ;
    UITextView* m_textView;
} ;

#if wxOSX_IPHONE_USE_TEXTFIELD

@interface  wxUITextFieldDelegate : NSObject<UITextFieldDelegate>
{
}

@end


@interface wxUITextField : UITextField
{
}

@end

@interface wxNSSecureTextField : UITextField<UITextFieldDelegate>
{
}

@end

@implementation wxNSSecureTextField

+ (void)initialize
{
    static BOOL initialized = NO;
    if (!initialized)
    {
        initialized = YES;
        wxOSXIPhoneClassAddWXMethods( self );
    }
}

- (void)controlTextDidChange:(NSNotification *)aNotification
{
    wxUnusedVar(aNotification);
    wxWidgetIPhoneImpl* impl = (wxWidgetIPhoneImpl* ) wxWidgetImpl::FindFromWXWidget( self );
    if ( impl )
        impl->controlTextDidChange();
}

- (void)controlTextDidEndEditing:(NSNotification *)aNotification
{
    wxUnusedVar(aNotification);
    wxWidgetIPhoneImpl* impl = (wxWidgetIPhoneImpl* ) wxWidgetImpl::FindFromWXWidget( self );
    if ( impl )
    {
        impl->DoNotifyFocusEvent( false, NULL );
    }
}

@end

#if 0 
@implementation wxUITextFieldEditor

- (void) keyDown:(NSEvent*) event
{
    wxWidgetIPhoneImpl* impl = (wxWidgetIPhoneImpl* ) wxWidgetImpl::FindFromWXWidget( (WXWidget) [self delegate] );
    lastKeyDownEvent = event;
    if ( impl == NULL || !impl->DoHandleKeyEvent(event) )
        [super keyDown:event];
    lastKeyDownEvent = nil;
}

- (void) keyUp:(NSEvent*) event
{
    wxWidgetIPhoneImpl* impl = (wxWidgetIPhoneImpl* ) wxWidgetImpl::FindFromWXWidget( (WXWidget) [self delegate] );
    if ( impl == NULL || !impl->DoHandleKeyEvent(event) )
        [super keyUp:event];
}

- (void) flagsChanged:(NSEvent*) event
{
    wxWidgetIPhoneImpl* impl = (wxWidgetIPhoneImpl* ) wxWidgetImpl::FindFromWXWidget( (WXWidget) [self delegate] );
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
    wxWidgetIPhoneImpl* impl = (wxWidgetIPhoneImpl* ) wxWidgetImpl::FindFromWXWidget( (WXWidget) [self delegate] );
    if ( impl == NULL || lastKeyDownEvent==nil || !impl->DoHandleCharEvent(lastKeyDownEvent, str) )
    {
        [super insertText:str];
    }
}

@end

#endif


@implementation wxUITextFieldDelegate

- (BOOL)textFieldShouldReturn:(UITextField *)textField
{
    // the user pressed the "Done" button, so dismiss the keyboard
    wxWidgetIPhoneImpl* impl = (wxWidgetIPhoneImpl* ) wxWidgetImpl::FindFromWXWidget( textField );
    if ( impl  )
    {
        wxWindow* wxpeer = (wxWindow*) impl->GetWXPeer();
        if ( wxpeer && wxpeer->GetWindowStyle() & wxTE_PROCESS_ENTER )
        {
            wxCommandEvent event(wxEVT_TEXT_ENTER, wxpeer->GetId());
            event.SetEventObject( wxpeer );
            event.SetString( static_cast<wxTextCtrl*>(wxpeer)->GetValue() );
            wxpeer->HandleWindowEvent( event );
        }
    }

    [textField resignFirstResponder];
    return YES;
}

/*
- (BOOL)textFieldShouldBeginEditing:(UITextField *)textField;        // return NO to disallow editing.
- (void)textFieldDidBeginEditing:(UITextField *)textField;           // became first responder
- (BOOL)textFieldShouldEndEditing:(UITextField *)textField;          // return YES to allow editing to stop and to resign first responder status. NO to disallow the editing session to end
- (void)textFieldDidEndEditing:(UITextField *)textField;             // may be called if forced even if shouldEndEditing returns NO (e.g. view removed from window) or endEditing:YES called

- (BOOL)textField:(UITextField *)textField shouldChangeCharactersInRange:(NSRange)range replacementString:(NSString *)string;   // return NO to not change text

- (BOOL)textFieldShouldClear:(UITextField *)textField;               // called when clear button pressed. return NO to ignore (no notifications)
*/

@end


@implementation wxUITextField

+ (void)initialize
{
    static BOOL initialized = NO;
    if (!initialized)
    {
        initialized = YES;
        wxOSXIPhoneClassAddWXMethods( self );
    }
}

#if 0
- (void)controlTextDidChange:(NSNotification *)aNotification
{
    wxUnusedVar(aNotification);
    wxWidgetIPhoneImpl* impl = (wxWidgetIPhoneImpl* ) wxWidgetImpl::FindFromWXWidget( self );
    if ( impl )
        impl->controlTextDidChange();
}
#endif

- (BOOL)textFieldShouldReturn:(UITextField *)textField
{
    wxUnusedVar(textField);
    
    
    return NO;
}

- (void)controlTextDidEndEditing:(NSNotification *)aNotification
{
    wxUnusedVar(aNotification);
    wxWidgetIPhoneImpl* impl = (wxWidgetIPhoneImpl* ) wxWidgetImpl::FindFromWXWidget( self );
    if ( impl )
    {
        impl->DoNotifyFocusEvent( false, NULL );
    }
}
@end

#endif

@interface wxUITextViewDelegate : NSObject<UITextViewDelegate>
{
}

- (void)textViewDidChange:(UITextView *)textView;
- (void)textViewDidBeginEditing:(UITextView *)textView;
- (void)textViewDidEndEditing:(UITextView *)textView;
- (BOOL)textView:(UITextView *)textView shouldChangeTextInRange:(NSRange)range replacementText:(NSString *)text;

@end

@implementation wxUITextViewDelegate

- (void)textViewDidChange:(UITextView *)textView
{
    wxWidgetIPhoneImpl* impl = (wxWidgetIPhoneImpl* ) wxWidgetImpl::FindFromWXWidget( textView );
    if ( impl )
        impl->controlTextDidChange();
}

- (void)textViewDidBeginEditing:(UITextView *)textView
{
    wxWidgetIPhoneImpl* impl = (wxWidgetIPhoneImpl* ) wxWidgetImpl::FindFromWXWidget( textView );
    if ( impl )
        impl->DoNotifyFocusEvent(true, NULL);
}

- (void)textViewDidEndEditing:(UITextView *)textView
{
    wxWidgetIPhoneImpl* impl = (wxWidgetIPhoneImpl* ) wxWidgetImpl::FindFromWXWidget( textView );
    if ( impl )
        impl->DoNotifyFocusEvent(false, NULL);
}

- (BOOL)textView:(UITextView *)textView shouldChangeTextInRange:(NSRange)range replacementText:(NSString *)text
{
    wxWidgetIPhoneImpl* impl = (wxWidgetIPhoneImpl* ) wxWidgetImpl::FindFromWXWidget( textView );
    if ( impl )
    {
        if ( !impl->GetWXPeer()->HasFlag(wxTE_MULTILINE) && [text isEqualToString:@"\n"])
        {
            [textView resignFirstResponder];
            return NO;
        }
    }
    return YES;
}


@end

//
// wxUITextViewControl
//

wxUITextViewControl::wxUITextViewControl( wxTextCtrl *wxPeer, UITextView* v) : 
    wxWidgetIPhoneImpl(wxPeer, v),
    wxTextWidgetImpl(wxPeer)
{
    m_textView = v;
    m_delegate= [[wxUITextViewDelegate alloc] init];
    
    [m_textView setDelegate:m_delegate];
}

wxUITextViewControl::~wxUITextViewControl()
{
    if (m_textView)
    {
        [m_textView setDelegate: nil];
    }
    [m_delegate release];
}

bool wxUITextViewControl::CanFocus() const
{
    return true;
}

wxString wxUITextViewControl::GetStringValue() const
{
    if (m_textView)
    {
        wxString result = wxCFStringRef::AsString([m_textView text], m_wxPeer->GetFont().GetEncoding());
        wxMacConvertNewlines13To10( &result ) ;
        return result;
    }
    return wxEmptyString;
}

void wxUITextViewControl::SetStringValue( const wxString &str)
{
    wxString st = str;
    wxMacConvertNewlines10To13( &st );
    wxMacEditHelper helper(m_textView);

    if (m_textView)
        [m_textView setText: wxCFStringRef( st , m_wxPeer->GetFont().GetEncoding() ).AsNSString()];
}

void wxUITextViewControl::Copy()
{
    if (m_textView)
        [m_textView copy:nil];

}

void wxUITextViewControl::Cut()
{
    if (m_textView)
        [m_textView cut:nil];
}

void wxUITextViewControl::Paste()
{
    if (m_textView)
        [m_textView paste:nil];
}

bool wxUITextViewControl::CanPaste() const
{
    return true;
}

void wxUITextViewControl::SetEditable(bool editable)
{
    if (m_textView)
        [m_textView setEditable: editable];
}

void wxUITextViewControl::GetSelection( long* from, long* to) const
{
    if (m_textView)
    {
        NSRange range = [m_textView selectedRange];
        *from = range.location;
        *to = range.location + range.length;
    }
}

void wxUITextViewControl::SetSelection( long from , long to )
{
    long textLength = [[m_textView text] length];
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

void wxUITextViewControl::WriteText(const wxString& str)
{
    wxString st = str;
    wxMacConvertNewlines10To13( &st );
    wxMacEditHelper helper(m_textView);
    
    wxCFStringRef insert( st , m_wxPeer->GetFont().GetEncoding() );
    NSMutableString* subst = [NSMutableString stringWithString:[m_textView text]];
    [subst replaceCharactersInRange:[m_textView selectedRange] withString:insert.AsNSString()];

    [m_textView setText:subst];
}

void wxUITextViewControl::SetFont( const wxFont & font , const wxColour& WXUNUSED(foreground) , long WXUNUSED(windowStyle), bool WXUNUSED(ignoreBlack) )
{
    if ([m_textView respondsToSelector:@selector(setFont:)])
        [m_textView setFont: font.OSXGetUIFont()];
}

bool wxUITextViewControl::GetStyle(long position, wxTextAttr& style)
{
    if (m_textView && position >=0)
    {   
        // UIFont* font = NULL;
        // NSColor* bgcolor = NULL;
        // NSColor* fgcolor = NULL;
        // NOTE: It appears that other platforms accept GetStyle with the position == length
        // but that UITextStorage does not accept length as a valid position.
        // Therefore we return the default control style in that case.
        /*
        if (position < [[m_textView string] length]) 
        {
            UITextStorage* storage = [m_textView textStorage];
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
        */
        /*
        if (font)
            style.SetFont(wxFont(font));
        
        if (bgcolor)
            style.SetBackgroundColour(wxColour(bgcolor));
            
        if (fgcolor)
            style.SetTextColour(wxColour(fgcolor));
        */
        return true;
    }

    return false;
}

void wxUITextViewControl::SetStyle(long start,
                                long end,
                                const wxTextAttr& style)
{
    if (m_textView) {
        NSRange range = NSMakeRange(start, end-start);
        if (start == -1 && end == -1)
            range = [m_textView selectedRange];
/*
        UITextStorage* storage = [m_textView textStorage];
        
        wxFont font = style.GetFont();
        if (style.HasFont() && font.IsOk())
            [storage addAttribute:NSFontAttributeName value:font.OSXGetNSFont() range:range];
        
        wxColour bgcolor = style.GetBackgroundColour();
        if (style.HasBackgroundColour() && bgcolor.IsOk())
            [storage addAttribute:NSBackgroundColorAttributeName value:bgcolor.OSXGetNSColor() range:range];
        
        wxColour fgcolor = style.GetTextColour();
        if (style.HasTextColour() && fgcolor.IsOk())
            [storage addAttribute:NSForegroundColorAttributeName value:fgcolor.OSXGetNSColor() range:range];
*/
    }
}

void wxUITextViewControl::CheckSpelling(bool check)
{
}

wxSize wxUITextViewControl::GetBestSize() const
{
    wxRect r;
    
    GetBestRect(&r);
    
    /*
    if (m_textView && [m_textView layoutManager])
    {
        NSRect rect = [[m_textView layoutManager] usedRectForTextContainer: [m_textView textContainer]];
        wxSize size = wxSize(rect.size.width, rect.size.height);
        size.x += [m_textView textContainerInset].width;
        size.y += [m_textView textContainerInset].height;
        return size;
    }
    return wxSize(0,0);
    */
    
    wxSize sz = r.GetSize();
    if ( sz.y < 31 )
        sz.y = 31;

    return sz;
}

#if wxOSX_IPHONE_USE_TEXTFIELD

//
// wxUITextFieldControl
//

wxUITextFieldControl::wxUITextFieldControl( wxTextCtrl *wxPeer, UITextField* w ) : 
    wxWidgetIPhoneImpl(wxPeer, w),
    wxTextWidgetImpl(wxPeer)
{
    m_textField = w;
    m_delegate = [[wxUITextFieldDelegate alloc] init];
    [m_textField setDelegate: m_delegate];
    m_selStart = m_selEnd = 0;
}

wxUITextFieldControl::~wxUITextFieldControl()
{
    if (m_textField)
        [m_textField setDelegate: nil];
    [m_delegate release];
}

wxString wxUITextFieldControl::GetStringValue() const
{
    return wxCFStringRef::AsString([m_textField text], m_wxPeer->GetFont().GetEncoding());
}

void wxUITextFieldControl::SetStringValue( const wxString &str)
{
//    wxMacEditHelper helper(m_textField);
    [m_textField setText: wxCFStringRef( str , m_wxPeer->GetFont().GetEncoding() ).AsNSString()];
}

wxSize wxUITextFieldControl::GetBestSize() const
{
    wxRect r;
    
    GetBestRect(&r);
    wxSize sz = r.GetSize();
    if ( sz.y < 31 )
        sz.y = 31;
    return sz;
}

void wxUITextFieldControl::Copy()
{
    [m_textField copy:nil];
}

void wxUITextFieldControl::Cut()
{
    [m_textField cut:nil];
}

void wxUITextFieldControl::Paste()
{
    [m_textField paste:nil];
}

bool wxUITextFieldControl::CanPaste() const
{
    return true;
}

void wxUITextFieldControl::SetEditable(bool editable)
{
    if (m_textField) {
        if ( !editable ) {
            [m_textField resignFirstResponder];
        }

        [m_textField setEnabled: editable];
    }
}

void wxUITextFieldControl::GetSelection( long* from, long* to) const
{
    *from = m_selStart;
    *to = m_selEnd;
}

void wxUITextFieldControl::SetSelection( long from , long to )
{
    long textLength = [[m_textField text] length];
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

    m_selStart = from;
    m_selEnd = to;
}

void wxUITextFieldControl::WriteText(const wxString& str)
{
#if 0
    NSEvent* formerEvent = m_lastKeyDownEvent;
        UIText* editor = [m_textField currentEditor];
    if ( editor )
    {
        wxMacEditHelper helper(m_textField);
        [editor insertText:wxCFStringRef( str , m_wxPeer->GetFont().GetEncoding() ).AsNSString()];
    }
    else
#endif
    {
        wxString val = GetStringValue() ;
        long start , end ;
        GetSelection( &start , &end ) ;
        val.Remove( start , end - start ) ;
        val.insert( start , str ) ;
        SetStringValue( val ) ;
        SetSelection( start + str.length() , start + str.length() ) ;
    }
#if 0
    m_lastKeyDownEvent = formerEvent;
#endif
}

void wxUITextFieldControl::controlAction(WXWidget WXUNUSED(slf),
    void* WXUNUSED(_cmd), void *WXUNUSED(sender))
{
    wxWindow* wxpeer = (wxWindow*) GetWXPeer();
    if ( wxpeer && (wxpeer->GetWindowStyle() & wxTE_PROCESS_ENTER) )
    {
        wxCommandEvent event(wxEVT_TEXT_ENTER, wxpeer->GetId());
        event.SetEventObject( wxpeer );
        event.SetString( static_cast<wxTextCtrl*>(wxpeer)->GetValue() );
        wxpeer->HandleWindowEvent( event );
    }
}

bool wxUITextFieldControl::SetHint(const wxString& hint)
{
    wxCFStringRef hintstring(hint);
    [m_textField setPlaceholder:hintstring.AsNSString()];
    return true;
}

#endif

//
//
//

wxWidgetImplType* wxWidgetImpl::CreateTextControl( wxTextCtrl* wxpeer,
                                    wxWindowMac* WXUNUSED(parent),
                                    wxWindowID WXUNUSED(id),
                                    const wxString& str,
                                    const wxPoint& pos,
                                    const wxSize& size,
                                    long style,
                                    long WXUNUSED(extraStyle))
{
    CGRect r = wxOSXGetFrameForControl( wxpeer, pos , size ) ;
    wxWidgetIPhoneImpl* c = NULL;
    wxTextWidgetImpl* t = NULL;
    id<UITextInputTraits> tv = nil;

#if wxOSX_IPHONE_USE_TEXTFIELD
    if ( style & wxTE_MULTILINE || style & wxTE_RICH || style & wxTE_RICH2 )
#endif
    {
        UITextView * v = nil;
        v = [[UITextView alloc] initWithFrame:r];
        tv = v;
        
        wxUITextViewControl* tc = new wxUITextViewControl( wxpeer, v );
        c = tc;
        t = tc;
    }
#if wxOSX_IPHONE_USE_TEXTFIELD
    else
    {
        wxUITextField* v = [[wxUITextField alloc] initWithFrame:r];
        tv = v;

		v.textColor = [UIColor blackColor];
		v.font = [UIFont systemFontOfSize:17.0];
		v.backgroundColor = [UIColor whiteColor];
		
		v.clearButtonMode = UITextFieldViewModeNever;
		
        [v setBorderStyle:UITextBorderStyleBezel];
        if ( style & wxNO_BORDER )
            v.borderStyle = UITextBorderStyleNone;
         
        wxUITextFieldControl* tc = new wxUITextFieldControl( wxpeer, v );
        c = tc;
        t = tc;
    }
#endif
    
    if ( style & wxTE_PASSWORD )
        [tv setSecureTextEntry:YES];
    
    if ( style & wxTE_CAPITALIZE )
        [tv setAutocapitalizationType:UITextAutocapitalizationTypeWords];
    else
        [tv setAutocapitalizationType:UITextAutocapitalizationTypeSentences];

    if ( !(style & wxTE_MULTILINE) )
    {
        [tv setAutocorrectionType:UITextAutocorrectionTypeNo];
		[tv setReturnKeyType:UIReturnKeyDone];
    }
    [tv setKeyboardType:UIKeyboardTypeDefault];
    
    t->SetStringValue(str);
    
    return c;
}


#endif // wxUSE_TEXTCTRL
