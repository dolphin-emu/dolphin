/////////////////////////////////////////////////////////////////////////////
// Name:        src/osx/cocoa/textctrl.mm
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
#include "wx/textcompleter.h"

#include "wx/osx/private.h"
#include "wx/osx/cocoa/private/textimpl.h"

@interface NSView(EditableView)
- (BOOL)isEditable;
- (void)setEditable:(BOOL)flag;
- (BOOL)isSelectable;
- (void)setSelectable:(BOOL)flag;
@end

// An object of this class is created before the text is modified
// programmatically and destroyed as soon as this is done. It does several
// things, like ensuring that the control is editable to allow setting its text
// at all and eating any unwanted focus loss events from textDidEndEditing:
// which don't really correspond to focus change.
class wxMacEditHelper
{
public :
    wxMacEditHelper( NSView* textView )
    {
        m_viewPreviouslyEdited = ms_viewCurrentlyEdited;
        ms_viewCurrentlyEdited =
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

        ms_viewCurrentlyEdited = m_viewPreviouslyEdited;
    }

    // Returns true if this view is the one currently being changed by the
    // program.
    static bool IsCurrentlyEditedView(NSView* v)
    {
        return v == ms_viewCurrentlyEdited;
    }

    // Returns true if this editor is the one currently being modified.
    static bool IsCurrentEditor(wxNSTextFieldEditor* e)
    {
        return e == [(NSTextField*)ms_viewCurrentlyEdited currentEditor];
    }

protected :
    BOOL m_formerEditable ;
    BOOL m_formerSelectable;
    NSView* m_textView;

    // The original value of ms_viewCurrentlyEdited when this object was
    // created.
    NSView* m_viewPreviouslyEdited;

    static NSView* ms_viewCurrentlyEdited;
} ;

NSView* wxMacEditHelper::ms_viewCurrentlyEdited = nil;

// an NSFormatter used to implement SetMaxLength() and ForceUpper() methods
@interface wxTextEntryFormatter : NSFormatter
{
    int maxLength;
    wxTextEntry* field;
    bool forceUpper;
}

@end

@implementation wxTextEntryFormatter

- (id)init 
{
    self = [super init];
    maxLength = 0;
    forceUpper = false;
    return self;
}

- (void) setMaxLength:(int) maxlen 
{
    maxLength = maxlen;
}

- (void) forceUpper
{
    forceUpper = true;
}

- (NSString *)stringForObjectValue:(id)anObject 
{
    if(![anObject isKindOfClass:[NSString class]])
        return nil;
    return [NSString stringWithString:anObject];
}

- (BOOL)getObjectValue:(id *)obj forString:(NSString *)string errorDescription:(NSString  **)error 
{
    *obj = [NSString stringWithString:string];
    return YES;
}

- (BOOL)isPartialStringValid:(NSString **)partialStringPtr proposedSelectedRange:(NSRangePointer)proposedSelRangePtr 
              originalString:(NSString *)origString originalSelectedRange:(NSRange)origSelRange errorDescription:(NSString **)error
{
    if ( maxLength > 0 )
    {
        if ( [*partialStringPtr length] > maxLength )
        {
            field->SendMaxLenEvent();
            return NO;
        }
    }

    if ( forceUpper )
    {
        NSString* upper = [*partialStringPtr uppercaseString];
        if ( ![*partialStringPtr isEqual:upper] )
        {
            *partialStringPtr = upper;
            return NO;
        }
    }

    return YES;
}

- (void) setTextEntry:(wxTextEntry*) tf
{
    field = tf;
}

@end

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
        impl->DoNotifyFocusLost();
}

- (BOOL)control:(NSControl*)control textView:(NSTextView*)textView doCommandBySelector:(SEL)commandSelector
{
    wxUnusedVar(textView);
    
    BOOL handled = NO;
    
    wxWidgetCocoaImpl* impl = (wxWidgetCocoaImpl* ) wxWidgetImpl::FindFromWXWidget( control );
    if ( impl  )
    {
        wxWindow* wxpeer = (wxWindow*) impl->GetWXPeer();
        if ( wxpeer )
        {
            if (commandSelector == @selector(insertNewline:))
            {
                if ( wxpeer->GetWindowStyle() & wxTE_PROCESS_ENTER )
                {
                    wxCommandEvent event(wxEVT_TEXT_ENTER, wxpeer->GetId());
                    event.SetEventObject( wxpeer );
                    wxTextWidgetImpl* impl = (wxNSTextFieldControl * ) wxWidgetImpl::FindFromWXWidget( self );
                    wxTextEntry * const entry = impl->GetTextEntry();
                    event.SetString( entry->GetValue() );
                    handled = wxpeer->HandleWindowEvent( event );
                }
                else
                {
                    wxTopLevelWindow *tlw = wxDynamicCast(wxGetTopLevelParent(wxpeer), wxTopLevelWindow);
                    if ( tlw && tlw->GetDefaultItem() )
                    {
                        wxButton *def = wxDynamicCast(tlw->GetDefaultItem(), wxButton);
                        if ( def && def->IsEnabled() )
                        {
                            wxCommandEvent event(wxEVT_BUTTON, def->GetId() );
                            event.SetEventObject(def);
                            def->Command(event);
                            handled = YES;
                        }
                     }
                }
            }
        }
    }
    
    return handled;
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
    // We should never generate char events for the text being inserted
    // programmatically.
    if ( !wxMacEditHelper::IsCurrentEditor(self) )
    {
        NSString *text = [str isKindOfClass:[NSAttributedString class]] ? [str string] : str;
        wxWidgetCocoaImpl* impl = (wxWidgetCocoaImpl* ) wxWidgetImpl::FindFromWXWidget( (WXWidget) [self delegate] );
        if ( impl && lastKeyDownEvent && impl->DoHandleCharEvent(lastKeyDownEvent, text) )
            return;
    }

    [super insertText:str];
}

- (BOOL) resignFirstResponder
{
    return [super resignFirstResponder];
}

- (BOOL) becomeFirstResponder
{
    // we need the stored text field, as at this point the delegate is not yet set
    wxWidgetCocoaImpl* impl = (wxWidgetCocoaImpl* ) wxWidgetImpl::FindFromWXWidget( (WXWidget) textField );

    BOOL r = [super becomeFirstResponder];
    if ( impl != NULL && r )
        impl->DoNotifyFocusSet();

    return r;
}

- (void) setTextField:(NSTextField*) field
{
    textField = field;
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

    if ( wxMacEditHelper::IsCurrentlyEditedView(self) )
    {
        // This notification is generated as the result of calling our own
        // wxTextCtrl method (e.g. WriteText()) and doesn't correspond to any
        // real focus loss event so skip generating it.
        return;
    }

    wxWidgetCocoaImpl* impl = (wxWidgetCocoaImpl* ) wxWidgetImpl::FindFromWXWidget( self );
    if ( impl )
        impl->DoNotifyFocusLost();
}

-(BOOL)textView:(NSTextView *)aTextView clickedOnLink:(id)link atIndex:(NSUInteger)charIndex
{
    wxWidgetCocoaImpl* impl = (wxWidgetCocoaImpl* ) wxWidgetImpl::FindFromWXWidget( aTextView );
    if ( impl  )
    {
        wxWindow* wxpeer = (wxWindow*) impl->GetWXPeer();
        if ( wxpeer )
        {
            wxMouseEvent evtMouse( wxEVT_LEFT_DOWN );
            wxTextUrlEvent event( wxpeer->GetId(), evtMouse, (long int)charIndex, (long int)charIndex );
            event.SetEventObject( wxpeer );
            wxpeer->HandleWindowEvent( event );
        }
    }
    return NO;
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
    if ( editor != fieldEditor )
    {
        [editor retain];
        [fieldEditor release];
        fieldEditor = editor;
    }
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

- (NSArray *)control:(NSControl *)control textView:(NSTextView *)textView completions:(NSArray *)words
 forPartialWordRange:(NSRange)charRange indexOfSelectedItem:(NSInteger*)index
{
    NSMutableArray* matches = NULL;

    wxTextWidgetImpl* impl = (wxNSTextFieldControl * ) wxWidgetImpl::FindFromWXWidget( self );
    wxTextEntry * const entry = impl->GetTextEntry();
    wxTextCompleter * const completer = entry->OSXGetCompleter();
    if ( completer )
    {
        const wxString prefix = entry->GetValue();
        if ( completer->Start(prefix) )
        {
            const wxString
                wordStart = wxCFStringRef::AsString(
                              [[textView string] substringWithRange:charRange]
                            );

            matches = [NSMutableArray array];
            for ( ;; )
            {
                const wxString s = completer->GetNext();
                if ( s.empty() )
                    break;

                // Normally the completer should return only the strings
                // starting with the prefix, but there could be exceptions
                // and, for compatibility with MSW which simply ignores all
                // entries that don't match the current text control contents,
                // we ignore them as well. Besides, our own wxTextCompleterFixed
                // doesn't respect this rule and, moreover, we need to extract
                // just the rest of the string anyhow.
                wxString completion;
                if ( s.StartsWith(prefix, &completion) )
                {
                    // We discarded the entire prefix above but actually we
                    // should include the part of it that consists of the
                    // beginning of the current word, otherwise it would be
                    // lost when completion is accepted as OS X supposes that
                    // our matches do start with the "partial word range"
                    // passed to us.
                    const wxCFStringRef fullWord(wordStart + completion);
                    [matches addObject: fullWord.AsNSString()];
                }
            }
        }
    }

    return matches;
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
        wxNSTextFieldControl* timpl = dynamic_cast<wxNSTextFieldControl*>(impl);
        if ( timpl )
            timpl->UpdateInternalSelectionFromEditor(fieldEditor);
        impl->DoNotifyFocusLost();
    }
}
@end

// wxNSTextViewControl

wxNSTextViewControl::wxNSTextViewControl( wxTextCtrl *wxPeer, WXWidget w, long style )
    : wxWidgetCocoaImpl(wxPeer, w),
      wxTextWidgetImpl(wxPeer)
{
    wxNSTextScrollView* sv = (wxNSTextScrollView*) w;
    m_scrollView = sv;

    const bool hasHScroll = (style & wxHSCROLL) != 0;

    [m_scrollView setHasVerticalScroller:YES];
    [m_scrollView setHasHorizontalScroller:hasHScroll];
    NSSize contentSize = [m_scrollView contentSize];
    NSRect viewFrame = NSMakeRect(
            0, 0,
            hasHScroll ? FLT_MAX : contentSize.width, contentSize.height
        );

    wxNSTextView* const tv = [[wxNSTextView alloc] initWithFrame: viewFrame];
    m_textView = tv;
    [tv setVerticallyResizable:YES];
    [tv setHorizontallyResizable:hasHScroll];
    [tv setAutoresizingMask:NSViewWidthSizable];
    [tv setAutomaticDashSubstitutionEnabled:false];
    [tv setAutomaticQuoteSubstitutionEnabled:false];

    if ( hasHScroll )
    {
        [[tv textContainer] setContainerSize:NSMakeSize(FLT_MAX, FLT_MAX)];
        [[tv textContainer] setWidthTracksTextView:NO];
    }

    if ( style & wxTE_RIGHT)
    {
        [tv setAlignment:NSRightTextAlignment];
    }
    else if ( style & wxTE_CENTRE)
    {
        [tv setAlignment:NSCenterTextAlignment];
    }

    if ( !wxPeer->HasFlag(wxTE_RICH | wxTE_RICH2) )
    {
        [tv setRichText:NO];
    }

    if ( wxPeer->HasFlag(wxTE_AUTO_URL) )
    {
        [tv setAutomaticLinkDetectionEnabled:YES];
    }

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

void wxNSTextViewControl::insertText(NSString* text, WXWidget slf, void *_cmd)
{
    if ( m_lastKeyDownEvent ==NULL || !DoHandleCharEvent(m_lastKeyDownEvent, text) )
    {
        wxOSX_TextEventHandlerPtr superimpl = (wxOSX_TextEventHandlerPtr) [[slf superclass] instanceMethodForSelector:(SEL)_cmd];
        superimpl(slf, (SEL)_cmd, text);
    }
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
    {
        [m_textView setString: wxCFStringRef( st , m_wxPeer->GetFont().GetEncoding() ).AsNSString()];
        if ( m_wxPeer->HasFlag(wxTE_AUTO_URL) )
        {
            // Make sure that any URLs in the new text are highlighted.
            [m_textView checkTextInDocument:nil];
        }
    }
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
    if ( !m_textView )
        return;

    if ( start == -1 && end == -1 )
    {
        NSMutableDictionary* const
            attrs = [NSMutableDictionary dictionaryWithCapacity:3];
        if ( style.HasFont() )
            [attrs setValue:style.GetFont().OSXGetNSFont() forKey:NSFontAttributeName];
        if ( style.HasBackgroundColour() )
            [attrs setValue:style.GetBackgroundColour().OSXGetNSColor() forKey:NSBackgroundColorAttributeName];
        if ( style.HasTextColour() )
            [attrs setValue:style.GetTextColour().OSXGetNSColor() forKey:NSForegroundColorAttributeName];

        [m_textView setTypingAttributes:attrs];
    }
    else // Set the attributes just for this range.
    {
        NSRange range = NSMakeRange(start, end-start);

        NSTextStorage* storage = [m_textView textStorage];
        if ( style.HasFont() )
            [storage addAttribute:NSFontAttributeName value:style.GetFont().OSXGetNSFont() range:range];

        if ( style.HasBackgroundColour() )
            [storage addAttribute:NSBackgroundColorAttributeName value:style.GetBackgroundColour().OSXGetNSColor() range:range];

        if ( style.HasTextColour() )
            [storage addAttribute:NSForegroundColorAttributeName value:style.GetTextColour().OSXGetNSColor() range:range];
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
    NSTextField <NSTextFieldDelegate> *tf = (NSTextField <NSTextFieldDelegate>*) w;
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

wxTextEntryFormatter* wxNSTextFieldControl::GetFormatter()
{
    // We only ever call setFormatter with wxTextEntryFormatter, so the cast is
    // safe.
    wxTextEntryFormatter*
        formatter = (wxTextEntryFormatter*)[m_textField formatter];
    if ( !formatter )
    {
        formatter = [[[wxTextEntryFormatter alloc] init] autorelease];
        [formatter setTextEntry:GetTextEntry()];
        [m_textField setFormatter:formatter];
    }

    return formatter;
}

void wxNSTextFieldControl::SetMaxLength(unsigned long len)
{
    [GetFormatter() setMaxLength:len];
}

void wxNSTextFieldControl::ForceUpper()
{
    [GetFormatter() forceUpper];
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

    // the editor might still be in existence, but we might be already passed our 'focus lost' storage
    // of the selection, so make sure we copy this
    m_selStart = from;
    m_selEnd = to;
}

void wxNSTextFieldControl::WriteText(const wxString& str)
{
    NSEvent* formerEvent = m_lastKeyDownEvent;
    m_lastKeyDownEvent = nil;
    NSText* editor = [m_textField currentEditor];
    if ( editor )
    {
        wxMacEditHelper helper(m_textField);
        BOOL hasUndo = [editor respondsToSelector:@selector(setAllowsUndo:)];
        if ( hasUndo )
            [(NSTextView*)editor setAllowsUndo:NO];
        [editor insertText:wxCFStringRef( str , m_wxPeer->GetFont().GetEncoding() ).AsNSString()];
        if ( hasUndo )
            [(NSTextView*)editor setAllowsUndo:YES];
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
        wxCommandEvent event(wxEVT_TEXT_ENTER, wxpeer->GetId());
        event.SetEventObject( wxpeer );
        event.SetString( GetTextEntry()->GetValue() );
        wxpeer->HandleWindowEvent( event );
    }
}

void wxNSTextFieldControl::SetInternalSelection( long from , long to )
{
    m_selStart = from;
    m_selEnd = to;
}

void wxNSTextFieldControl::UpdateInternalSelectionFromEditor( wxNSTextFieldEditor* fieldEditor )
{
    if ( fieldEditor )
    {
        NSRange range = [fieldEditor selectedRange];
        SetInternalSelection(range.location, range.location + range.length);
    }
}

// as becoming first responder on a window - triggers a resign on the same control, we have to avoid
// the resign notification writing back native selection values before we can set our own

static WXWidget s_widgetBecomingFirstResponder = nil;

bool wxNSTextFieldControl::becomeFirstResponder(WXWidget slf, void *_cmd)
{
    s_widgetBecomingFirstResponder = slf;
    bool retval = wxWidgetCocoaImpl::becomeFirstResponder(slf, _cmd);
    s_widgetBecomingFirstResponder = nil;
    return retval;
}

bool wxNSTextFieldControl::resignFirstResponder(WXWidget slf, void *_cmd)
{
    if ( slf != s_widgetBecomingFirstResponder )
    {
        NSText* editor = [m_textField currentEditor];
        if ( editor )
        {
            NSRange range = [editor selectedRange];
            m_selStart = range.location;
            m_selEnd = range.location + range.length;
        }
    }
    return wxWidgetCocoaImpl::resignFirstResponder(slf, _cmd);
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

    if ( style & wxTE_MULTILINE )
    {
        wxNSTextScrollView* v = nil;
        v = [[wxNSTextScrollView alloc] initWithFrame:r];
        c = new wxNSTextViewControl( wxpeer, v, style );
        c->SetNeedsFocusRect( true );
    }
    else
    {
        NSTextField* v = nil;
        if ( style & wxTE_PASSWORD )
            v = [[wxNSSecureTextField alloc] initWithFrame:r];
        else
            v = [[wxNSTextField alloc] initWithFrame:r];

        if ( style & wxTE_RIGHT)
        {
            [v setAlignment:NSRightTextAlignment];
        }
        else if ( style & wxTE_CENTRE)
        {
            [v setAlignment:NSCenterTextAlignment];
        }
                
        NSTextFieldCell* cell = [v cell];
        [cell setScrollable:YES];
        // TODO: Remove if we definitely are sure, it's not needed
        // as setting scrolling to yes, should turn off any wrapping
        // [cell setLineBreakMode:NSLineBreakByClipping]; 

        c = new wxNSTextFieldControl( wxpeer, wxpeer, v );
        
        if ( (style & wxNO_BORDER) || (style & wxSIMPLE_BORDER) )
        {
            // under 10.7 the textcontrol can draw its own focus
            // even if no border is shown, on previous systems
            // we have to emulate this
            [v setBezeled:NO];
            [v setBordered:NO];
        }
        else
        {
            // use native border
            c->SetNeedsFrame(false);
        }
    }

    return c;
}


#endif // wxUSE_TEXTCTRL
