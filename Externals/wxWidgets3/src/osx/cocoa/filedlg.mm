/////////////////////////////////////////////////////////////////////////////
// Name:        src/cocoa/filedlg.mm
// Purpose:     wxFileDialog for wxCocoa
// Author:      Ryan Norton
// Modified by:
// Created:     2004-10-02
// Copyright:   (c) Ryan Norton
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

// ============================================================================
// declarations
// ============================================================================

// ----------------------------------------------------------------------------
// headers
// ----------------------------------------------------------------------------

// For compilers that support precompilation, includes "wx.h".
#include "wx/wxprec.h"

#if wxUSE_FILEDLG

#include "wx/filedlg.h"

#ifndef WX_PRECOMP
    #include "wx/msgdlg.h"
    #include "wx/app.h"
    #include "wx/sizer.h"
    #include "wx/stattext.h"
    #include "wx/choice.h"
#endif

#include "wx/filename.h"
#include "wx/tokenzr.h"
#include "wx/evtloop.h"

#include "wx/osx/private.h"
#include "wx/sysopt.h"
#include "wx/modalhook.h"

#include <mach-o/dyld.h>

// ============================================================================
// implementation
// ============================================================================

// Open Items:
// - parameter support for descending into packages as directories (setTreatsFilePackagesAsDirectories)
// - as setAllowedFileTypes is only functional for NSOpenPanel on 10.6+, on earlier systems, the file
// type choice will not be shown, but all possible file items will be shown, if a popup must be working
// then the delegate method - (BOOL)panel:(id)sender shouldShowFilename:(NSString *)filename will have to
// be implemented

@interface wxOpenPanelDelegate : NSObject <NSOpenSavePanelDelegate>
{
    wxFileDialog* _dialog;
}

- (wxFileDialog*) fileDialog;
- (void) setFileDialog:(wxFileDialog*) dialog;

- (BOOL)panel:(id)sender shouldShowFilename:(NSString *)filename;

@end

@implementation wxOpenPanelDelegate

- (id) init
{
    self = [super init];
    _dialog = NULL;
    return self;
}

- (wxFileDialog*) fileDialog
{
    return _dialog;
}

- (void) setFileDialog:(wxFileDialog*) dialog
{
    _dialog = dialog;
}

- (BOOL)panel:(id)sender shouldShowFilename:(NSString *)filename
{
    BOOL showObject = YES;

    wxGCC_WARNING_SUPPRESS(deprecated-declarations)

    NSString* resolvedLink = [[NSFileManager defaultManager] pathContentOfSymbolicLinkAtPath:filename];
    if ( resolvedLink != nil )
        filename = resolvedLink;

    NSDictionary* fileAttribs = [[NSFileManager defaultManager]
                                 fileAttributesAtPath:filename traverseLink:YES];

    wxGCC_WARNING_RESTORE(deprecated-declarations)

    if (fileAttribs)
    {
        // check for packages
        if ([NSFileTypeDirectory isEqualTo:[fileAttribs objectForKey:NSFileType]])
        {
            if ([[NSWorkspace sharedWorkspace] isFilePackageAtPath:filename] == NO)
                showObject = YES;    // it's a folder, OK to show
            else
            {
                // it's a packaged directory, apply check
                wxCFStringRef filecf([filename retain]);
                showObject = _dialog->CheckFile(filecf.AsString());  
            }
        }
        else
        {
            // the code above only solves links, not aliases, do this here:
            
            NSString* resolvedAlias = nil;
            
            CFURLRef url = CFURLCreateWithFileSystemPath (kCFAllocatorDefault, 
                                                          (CFStringRef)filename, 
                                                          kCFURLPOSIXPathStyle,
                                                          NO); 
            if (url != NULL) 
            {
                FSRef fsRef; 
                if (CFURLGetFSRef(url, &fsRef)) 
                {
                    Boolean targetIsFolder, wasAliased;
                    OSErr err = FSResolveAliasFile (&fsRef, true, &targetIsFolder, &wasAliased);
                    
                    if ((err == noErr) && wasAliased) 
                    {
                        CFURLRef resolvedUrl = CFURLCreateFromFSRef(kCFAllocatorDefault,  &fsRef);
                        if (resolvedUrl != NULL) 
                        {
                            resolvedAlias = (NSString*) CFURLCopyFileSystemPath(resolvedUrl,
                                                                               kCFURLPOSIXPathStyle); 
                            CFRelease(resolvedUrl);
                        }
                    } 
                }
                CFRelease(url);
            }

            if (resolvedAlias != nil) 
            {
                // recursive call
                [resolvedAlias autorelease];
                showObject = [self panel:sender shouldShowFilename:resolvedAlias];
            }
            else
            {
                wxCFStringRef filecf([filename retain]);
                showObject = _dialog->CheckFile(filecf.AsString());  
            }
        }
    }

    return showObject;    
}

@end

wxIMPLEMENT_CLASS(wxFileDialog, wxFileDialogBase);

void wxFileDialog::Init()
{
    m_filterIndex = -1;
    m_delegate = nil;
    m_sheetDelegate = nil;
    m_filterPanel = NULL;
    m_filterChoice = NULL;
}

void wxFileDialog::Create(
    wxWindow *parent, const wxString& message,
    const wxString& defaultDir, const wxString& defaultFileName, const wxString& wildCard,
    long style, const wxPoint& pos, const wxSize& sz, const wxString& name)
{
    wxFileDialogBase::Create(parent, message, defaultDir, defaultFileName, wildCard, style, pos, sz, name);

    m_sheetDelegate = [[ModalDialogDelegate alloc] init];
    [(ModalDialogDelegate*)m_sheetDelegate setImplementation: this];
}

wxFileDialog::~wxFileDialog()
{
    [m_sheetDelegate release];
}

bool wxFileDialog::SupportsExtraControl() const
{
    return true;
}

NSArray* GetTypesFromExtension( const wxString extensiongroup, wxArrayString& extensions )
{
    NSMutableArray* types = nil;
    extensions.Clear();

    wxStringTokenizer tokenizer( extensiongroup, wxT(";") ) ;
    while ( tokenizer.HasMoreTokens() )
    {
        wxString extension = tokenizer.GetNextToken() ;
        // Remove leading '*'
        if ( extension.length() && (extension.GetChar(0) == '*') )
            extension = extension.Mid( 1 );

        // Remove leading '.'
        if ( extension.length() && (extension.GetChar(0) == '.') )
            extension = extension.Mid( 1 );

        // Remove leading '*', this is for handling *.*
        if ( extension.length() && (extension.GetChar(0) == '*') )
            extension = extension.Mid( 1 );

        if ( extension.IsEmpty() )
        {
            extensions.Clear();
            [types release];
            types = nil;
            return nil;
        }

        if ( types == nil )
            types = [[NSMutableArray alloc] init];

        extensions.Add(extension.Lower());
        wxCFStringRef cfext(extension);
        [types addObject: (NSString*)cfext.AsNSString()  ];
#if 0
        // add support for classic fileType / creator here
        wxUint32 fileType, creator;
        // extension -> mactypes
#endif
    }
    [types autorelease];
    return types;
}

NSArray* GetTypesFromFilter( const wxString& filter, wxArrayString& names, wxArrayString& extensiongroups )
{
    NSMutableArray* types = nil;
    bool allowAll = false;

    names.Clear();
    extensiongroups.Clear();

    if ( !filter.empty() )
    {
        wxStringTokenizer tokenizer( filter, wxT("|") );
        int numtokens = (int)tokenizer.CountTokens();
        if(numtokens == 1)
        {
            // we allow for compatibility reason to have a single filter expression (like *.*) without
            // an explanatory text, in that case the first part is name and extension at the same time
            wxString extension = tokenizer.GetNextToken();
            names.Add( extension );
            extensiongroups.Add( extension );
        }
        else
        {
            int numextensions = numtokens / 2;
            for(int i = 0; i < numextensions; i++)
            {
                wxString name = tokenizer.GetNextToken();
                wxString extension = tokenizer.GetNextToken();
                names.Add( name );
                extensiongroups.Add( extension );
            }
        }

        const size_t extCount = extensiongroups.GetCount();
        wxArrayString extensions;
        for ( size_t i = 0 ; i < extCount; i++ )
        {
            NSArray* exttypes = GetTypesFromExtension(extensiongroups[i], extensions);
            if ( exttypes != nil )
            {
                if ( allowAll == false )
                {
                    if ( types == nil )
                        types = [[NSMutableArray alloc] init];

                    [types addObjectsFromArray:exttypes];
                }
            }
            else
            {
                allowAll = true;
                [types release];
                types = nil;
            }
        }
    }
    [types autorelease];
    return types;
}

void wxFileDialog::ShowWindowModal()
{
    wxCFStringRef cf( m_message );
    wxCFStringRef dir( m_dir );
    wxCFStringRef file( m_fileName );

    wxNonOwnedWindow* parentWindow = NULL;
    
    m_modality = wxDIALOG_MODALITY_WINDOW_MODAL;

    if (GetParent())
        parentWindow = dynamic_cast<wxNonOwnedWindow*>(wxGetTopLevelParent(GetParent()));

    wxASSERT_MSG(parentWindow, "Window modal display requires parent.");

    wxGCC_WARNING_SUPPRESS(deprecated-declarations)

    NSArray* types = GetTypesFromFilter( m_wildCard, m_filterNames, m_filterExtensions ) ;
    if ( HasFlag(wxFD_SAVE) )
    {
        NSSavePanel* sPanel = [NSSavePanel savePanel];

        SetupExtraControls(sPanel);

        // makes things more convenient:
        [sPanel setCanCreateDirectories:YES];
        [sPanel setMessage:cf.AsNSString()];
        // if we should be able to descend into pacakges we must somehow
        // be able to pass this in
        [sPanel setTreatsFilePackagesAsDirectories:NO];
        [sPanel setCanSelectHiddenExtension:YES];
        [sPanel setAllowedFileTypes:types];
        [sPanel setAllowsOtherFileTypes:NO];
        
        NSWindow* nativeParent = parentWindow->GetWXWindow();
        [sPanel beginSheetForDirectory:dir.AsNSString() file:file.AsNSString()
            modalForWindow: nativeParent modalDelegate: m_sheetDelegate
            didEndSelector: @selector(sheetDidEnd:returnCode:contextInfo:)
            contextInfo: nil];
    }
    else 
    {
        NSOpenPanel* oPanel = [NSOpenPanel openPanel];
        
        SetupExtraControls(oPanel);

        [oPanel setTreatsFilePackagesAsDirectories:NO];
        [oPanel setCanChooseDirectories:NO];
        [oPanel setResolvesAliases:YES];
        [oPanel setCanChooseFiles:YES];
        [oPanel setMessage:cf.AsNSString()];
        [oPanel setAllowsMultipleSelection: (HasFlag(wxFD_MULTIPLE) ? YES : NO )];
        
        NSWindow* nativeParent = parentWindow->GetWXWindow();
        [oPanel beginSheetForDirectory:dir.AsNSString() file:file.AsNSString()
            types: types modalForWindow: nativeParent
            modalDelegate: m_sheetDelegate
            didEndSelector: @selector(sheetDidEnd:returnCode:contextInfo:)
            contextInfo: nil];
    }

    wxGCC_WARNING_RESTORE(deprecated-declarations)
}

// Create a panel with the file type drop down list
// If extra controls need to be added (see wxFileDialog::SetExtraControlCreator), add
// them to the panel as well
// Returns the newly created wxPanel

wxWindow* wxFileDialog::CreateFilterPanel(wxWindow *extracontrol)
{
    wxPanel *extrapanel = new wxPanel(this, wxID_ANY, wxDefaultPosition, wxDefaultSize);
    wxBoxSizer *verticalSizer = new wxBoxSizer(wxVERTICAL);
    extrapanel->SetSizer(verticalSizer);
    
    // the file type control
    {
        wxBoxSizer *horizontalSizer = new wxBoxSizer(wxHORIZONTAL);
        verticalSizer->Add(horizontalSizer, 0, wxEXPAND, 0);
        wxStaticText *stattext = new wxStaticText( extrapanel, wxID_ANY, _("File type:") );
        horizontalSizer->Add(stattext, 0, wxALIGN_CENTER_VERTICAL|wxALL, 5);
        m_filterChoice = new wxChoice(extrapanel, wxID_ANY);
        horizontalSizer->Add(m_filterChoice, 1, wxALIGN_CENTER_VERTICAL|wxALL, 5);
        m_filterChoice->Append(m_filterNames);
        if( m_filterNames.GetCount() > 0)
        {
            if ( m_firstFileTypeFilter >= 0 )
                m_filterChoice->SetSelection(m_firstFileTypeFilter);
        }
        m_filterChoice->Connect(wxEVT_CHOICE, wxCommandEventHandler(wxFileDialog::OnFilterSelected), NULL, this);
    }
        
    if(extracontrol)
    {
        wxBoxSizer *horizontalSizer = new wxBoxSizer(wxHORIZONTAL);
        verticalSizer->Add(horizontalSizer, 0, wxEXPAND, 0);

        extracontrol->Reparent(extrapanel);
        horizontalSizer->Add(extracontrol);
    }

    verticalSizer->Layout();
    verticalSizer->SetSizeHints(extrapanel);
    return extrapanel;
}

void wxFileDialog::DoOnFilterSelected(int index)
{
    NSArray* types = GetTypesFromExtension(m_filterExtensions[index],m_currentExtensions);
    NSSavePanel* panel = (NSSavePanel*) GetWXWindow();
    if ( m_delegate )
        [panel validateVisibleColumns];
    else
        [panel setAllowedFileTypes:types];
}

// An item has been selected in the file filter wxChoice:
void wxFileDialog::OnFilterSelected( wxCommandEvent &WXUNUSED(event) )
{
    DoOnFilterSelected( m_filterChoice->GetSelection() );
}

bool wxFileDialog::CheckFile( const wxString& filename )
{
    if ( m_currentExtensions.GetCount() == 0 )
        return true;
    
    wxString ext = filename.AfterLast('.').Lower();
    
    for ( size_t i = 0; i < m_currentExtensions.GetCount(); ++i )
    {
        if ( ext == m_currentExtensions[i] )
            return true;
    }
    return false;
}

void wxFileDialog::SetupExtraControls(WXWindow nativeWindow)
{
    NSSavePanel* panel = (NSSavePanel*) nativeWindow;
    // for sandboxed app we cannot access the outer structures
    // this leads to problems with extra controls, so as a temporary
    // workaround for crashes we don't support those yet
    if ( [panel contentView] == nil || getenv("APP_SANDBOX_CONTAINER_ID") != NULL )
        return;
    
    wxNonOwnedWindow::Create( GetParent(), nativeWindow );
    wxWindow* extracontrol = NULL;
    if ( HasExtraControlCreator() )
    {
        CreateExtraControl();
        extracontrol = GetExtraControl();
    }

    NSView* accView = nil;

    if ( m_useFileTypeFilter )
    {
        m_filterPanel = CreateFilterPanel(extracontrol);
        accView = m_filterPanel->GetHandle();
    }
    else
    {
        m_filterPanel = NULL;
        m_filterChoice = NULL;
        if ( extracontrol != nil )
            accView = extracontrol->GetHandle();
    }

    if ( accView != nil )
    {
        [accView removeFromSuperview];
        [panel setAccessoryView:accView];
    }
    else
    {
        [panel setAccessoryView:nil];
    }
}

int wxFileDialog::ShowModal()
{
    WX_HOOK_MODAL_DIALOG();

    wxCFEventLoopPauseIdleEvents pause;

    wxMacAutoreleasePool autoreleasepool;
    
    wxCFStringRef cf( m_message );

    wxCFStringRef dir( m_dir );
    wxCFStringRef file( m_fileName );

    m_path = wxEmptyString;
    m_fileNames.Clear();
    m_paths.Clear();

    wxNonOwnedWindow* parentWindow = NULL;
    int returnCode = -1;

    if (GetParent())
    {
        parentWindow = dynamic_cast<wxNonOwnedWindow*>(wxGetTopLevelParent(GetParent()));
    }


    NSArray* types = GetTypesFromFilter( m_wildCard, m_filterNames, m_filterExtensions ) ;

    m_useFileTypeFilter = m_filterExtensions.GetCount() > 1;

    if( HasFlag(wxFD_OPEN) )
    {
        if ( !(wxSystemOptions::HasOption( wxOSX_FILEDIALOG_ALWAYS_SHOW_TYPES ) && (wxSystemOptions::GetOptionInt( wxOSX_FILEDIALOG_ALWAYS_SHOW_TYPES ) == 1)) )
            m_useFileTypeFilter = false;            
    }

    m_firstFileTypeFilter = -1;

    if ( m_useFileTypeFilter
        && m_filterIndex >= 0 && m_filterIndex < m_filterExtensions.GetCount() )
    {
        m_firstFileTypeFilter = m_filterIndex;
    }
    else if ( m_useFileTypeFilter )
    {
        types = nil;
        bool useDefault = true;
        for ( size_t i = 0; i < m_filterExtensions.GetCount(); ++i )
        {
            types = GetTypesFromExtension(m_filterExtensions[i], m_currentExtensions);
            if ( m_currentExtensions.GetCount() == 0 )
            {
                useDefault = false;
                m_firstFileTypeFilter = i;
                break;
            }
            
            for ( size_t j = 0; j < m_currentExtensions.GetCount(); ++j )
            {
                if ( m_fileName.EndsWith(m_currentExtensions[j]) )
                {
                    m_firstFileTypeFilter = i;
                    useDefault = false;
                    break;
                }
            }
            if ( !useDefault )
                break;
        }
        if ( useDefault )
        {
            types = GetTypesFromExtension(m_filterExtensions[0], m_currentExtensions);
            m_firstFileTypeFilter = 0;
        }
    }

    wxGCC_WARNING_SUPPRESS(deprecated-declarations)

    if ( HasFlag(wxFD_SAVE) )
    {
        NSSavePanel* sPanel = [NSSavePanel savePanel];

        SetupExtraControls(sPanel);

        // makes things more convenient:
        [sPanel setCanCreateDirectories:YES];
        [sPanel setMessage:cf.AsNSString()];
        // if we should be able to descend into pacakges we must somehow
        // be able to pass this in
        [sPanel setTreatsFilePackagesAsDirectories:NO];
        [sPanel setCanSelectHiddenExtension:YES];
        [sPanel setAllowedFileTypes:types];
        [sPanel setAllowsOtherFileTypes:NO];

        if ( HasFlag(wxFD_OVERWRITE_PROMPT) )
        {
        }

        /*
        Let the file dialog know what file type should be used initially.
        If this is not done then when setting the filter index
        programmatically to 1 the file will still have the extension
        of the first file type instead of the second one. E.g. when file
        types are foo and bar, a filename "myletter" with SetDialogIndex(1)
        would result in saving as myletter.foo, while we want myletter.bar.
        */
        if(m_firstFileTypeFilter > 0)
        {
            DoOnFilterSelected(m_firstFileTypeFilter);
        }

        returnCode = [sPanel runModalForDirectory: m_dir.IsEmpty() ? nil : dir.AsNSString() file:file.AsNSString() ];
        ModalFinishedCallback(sPanel, returnCode);
    }
    else
    {
        NSOpenPanel* oPanel = [NSOpenPanel openPanel];
        
        SetupExtraControls(oPanel);
                
        [oPanel setTreatsFilePackagesAsDirectories:NO];
        [oPanel setCanChooseDirectories:NO];
        [oPanel setResolvesAliases:YES];
        [oPanel setCanChooseFiles:YES];
        [oPanel setMessage:cf.AsNSString()];
        [oPanel setAllowsMultipleSelection: (HasFlag(wxFD_MULTIPLE) ? YES : NO )];

        // Note that the test here is intentionally different from the one
        // above, in the wxFD_SAVE case: we need to call DoOnFilterSelected()
        // even for m_firstFileTypeFilter == 0, i.e. when using the default
        // filter.
        if ( m_firstFileTypeFilter >= 0 )
        {
            DoOnFilterSelected(m_firstFileTypeFilter);
        }
        else
        {
            [oPanel setAllowedFileTypes: (m_delegate == nil ? types : nil)];
        }
        if ( !m_dir.IsEmpty() )
            [oPanel setDirectoryURL:[NSURL fileURLWithPath:dir.AsNSString()
                                               isDirectory:YES]];
        returnCode = [oPanel runModal];
            
        ModalFinishedCallback(oPanel, returnCode);
    }

    wxGCC_WARNING_RESTORE(deprecated-declarations)

    return GetReturnCode();
}

void wxFileDialog::ModalFinishedCallback(void* panel, int returnCode)
{
    wxGCC_WARNING_SUPPRESS(deprecated-declarations)

    int result = wxID_CANCEL;
    if (HasFlag(wxFD_SAVE))
    {
        if (returnCode == NSOKButton )
        {
            NSSavePanel* sPanel = (NSSavePanel*)panel;
            result = wxID_OK;

            m_path = wxCFStringRef::AsStringWithNormalizationFormC([sPanel filename]);
            m_fileName = wxFileNameFromPath(m_path);
            m_dir = wxPathOnly( m_path );
            if (m_filterChoice)
            {
                m_filterIndex = m_filterChoice->GetSelection();
            }
        }
    }
    else
    {
        NSOpenPanel* oPanel = (NSOpenPanel*)panel;
        if (returnCode == NSOKButton )
        {
            panel = oPanel;
            result = wxID_OK;

            if (m_filterChoice)
            {
                m_filterIndex = m_filterChoice->GetSelection();
            }

            NSArray* filenames = [oPanel filenames];
            for ( size_t i = 0 ; i < [filenames count] ; ++ i )
            {
                wxString fnstr = wxCFStringRef::AsStringWithNormalizationFormC([filenames objectAtIndex:i]);
                m_paths.Add( fnstr );
                m_fileNames.Add( wxFileNameFromPath(fnstr) );
                if ( i == 0 )
                {
                    m_path = fnstr;
                    m_fileName = wxFileNameFromPath(fnstr);
                    m_dir = wxPathOnly( fnstr );
                }
            }
        }
        if ( m_delegate )
        {
            [oPanel setDelegate:nil];
            [m_delegate release];
            m_delegate = nil;
        }
    }
    SetReturnCode(result);
    
    if (GetModality() == wxDIALOG_MODALITY_WINDOW_MODAL)
        SendWindowModalDialogEvent ( wxEVT_WINDOW_MODAL_DIALOG_CLOSED  );
    
    // workaround for sandboxed app, see above
    if ( m_isNativeWindowWrapper )
        UnsubclassWin();
    [(NSSavePanel*) panel setAccessoryView:nil];

    wxGCC_WARNING_RESTORE(deprecated-declarations)
}

#endif // wxUSE_FILEDLG
