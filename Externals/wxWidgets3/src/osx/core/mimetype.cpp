/////////////////////////////////////////////////////////////////////////////
// Name:        src/osx/core/mimetype.cpp
// Purpose:     Mac OS X implementation for wx MIME-related classes
// Author:      Neil Perkins
// Modified by:
// Created:     2010-05-15
// Copyright:   (C) 2010 Neil Perkins
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////


#include "wx/wxprec.h"

#ifdef __BORLANDC__
#pragma hdrstop
#endif

#ifndef WX_PRECOMP
#include "wx/defs.h"
#endif

#if wxUSE_MIMETYPE

#include "wx/osx/mimetype.h"
#include "wx/osx/private.h"

/////////////////////////////////////////////////////////////////////////////
// Helper functions
/////////////////////////////////////////////////////////////////////////////


// Read a string or array of strings from a CFDictionary for a given key
// Return an empty list on error
wxArrayString ReadStringListFromCFDict( CFDictionaryRef dictionary, CFStringRef key )
{
    // Create an empty list
    wxArrayString results;

    // Look up the requested key
    CFTypeRef valueData = CFDictionaryGetValue( dictionary, key );

    if( valueData )
    {
        // Value is an array
        if( CFGetTypeID( valueData ) == CFArrayGetTypeID() )
        {
            CFArrayRef valueList = reinterpret_cast< CFArrayRef >( valueData );

            CFTypeRef itemData;
            wxCFStringRef item;

            // Look at each item in the array
            for( CFIndex i = 0, n = CFArrayGetCount( valueList ); i < n; i++ )
            {
                itemData = CFArrayGetValueAtIndex( valueList, i );

                // Make sure the item is a string
                if( CFGetTypeID( itemData ) == CFStringGetTypeID() )
                {
                    // wxCFStringRef will automatically CFRelease, so an extra CFRetain is needed
                    item = reinterpret_cast< CFStringRef >( itemData );
                    wxCFRetain( item.get() );

                    // Add the string to the list
                    results.Add( item.AsString() );
                }
            }
        }

        // Value is a single string - return a list of one item
        else if( CFGetTypeID( valueData ) == CFStringGetTypeID() )
        {
            // wxCFStringRef will automatically CFRelease, so an extra CFRetain is needed
            wxCFStringRef value = reinterpret_cast< CFStringRef >( valueData );
            wxCFRetain( value.get() );

            // Add the string to the list
            results.Add( value.AsString() );
        }
    }

    // Return the list. If the dictionary did not contain key,
    // or contained the wrong data type, the list will be empty
    return results;
}


// Given a single CFDictionary representing document type data, check whether
// it matches a particular file extension. Return true for a match, false otherwise
bool CheckDocTypeMatchesExt( CFDictionaryRef docType, CFStringRef requiredExt )
{
    const static wxCFStringRef extKey( "CFBundleTypeExtensions" );

    CFTypeRef extData = CFDictionaryGetValue( docType, extKey );

    if( !extData )
        return false;

    if( CFGetTypeID( extData ) == CFArrayGetTypeID() )
    {
        CFArrayRef extList = reinterpret_cast< CFArrayRef >( extData );
        CFTypeRef extItem;

        for( CFIndex i = 0, n = CFArrayGetCount( extList ); i < n; i++ )
        {
            extItem = CFArrayGetValueAtIndex( extList, i );

            if( CFGetTypeID( extItem ) == CFStringGetTypeID() )
            {
                CFStringRef ext = reinterpret_cast< CFStringRef >( extItem );

                if( CFStringCompare( ext, requiredExt, kCFCompareCaseInsensitive ) == kCFCompareEqualTo )
                    return true;
            }
        }
    }

    if( CFGetTypeID( extData ) == CFStringGetTypeID() )
    {
        CFStringRef ext = reinterpret_cast< CFStringRef >( extData );

        if( CFStringCompare( ext, requiredExt, kCFCompareCaseInsensitive ) == kCFCompareEqualTo )
            return true;
    }

    return false;
}


// Given a data structure representing document type data, or a list of such
// structures, find the one which matches a particular file extension
// The result will be a CFDictionary containining document type data
// if a match is found, or null otherwise
CFDictionaryRef GetDocTypeForExt( CFTypeRef docTypeData, CFStringRef requiredExt )
{
    CFDictionaryRef docType;
    CFArrayRef docTypes;
    CFTypeRef item;

    if( !docTypeData )
        return NULL;

    if( CFGetTypeID( docTypeData ) == CFArrayGetTypeID() )
    {
        docTypes = reinterpret_cast< CFArrayRef >( docTypeData );

        for( CFIndex i = 0, n = CFArrayGetCount( docTypes ); i < n; i++ )
        {
            item = CFArrayGetValueAtIndex( docTypes, i );

            if( CFGetTypeID( item ) == CFDictionaryGetTypeID() )
            {
                docType = reinterpret_cast< CFDictionaryRef >( item );

                if( CheckDocTypeMatchesExt( docType, requiredExt ) )
                    return docType;
            }
        }
    }

    if( CFGetTypeID( docTypeData ) == CFDictionaryGetTypeID() )
    {
        CFDictionaryRef docType = reinterpret_cast< CFDictionaryRef >( docTypeData );

        if( CheckDocTypeMatchesExt( docType, requiredExt ) )
            return docType;
    }

    return NULL;
}


// Given an application bundle reference and the name of an icon file
// which is a resource in that bundle, look up the full (posix style)
// path to that icon. Returns the path, or an empty wxString on failure
wxString GetPathForIconFile( CFBundleRef bundle, CFStringRef iconFile )
{
    // If either parameter is NULL there is no hope of success
    if( !bundle || !iconFile )
        return wxEmptyString;

    // Create a range object representing the whole string
    CFRange wholeString;
    wholeString.location = 0;
    wholeString.length = CFStringGetLength( iconFile );

    // Index of the period in the file name for iconFile
    UniCharCount periodIndex;

    // In order to locate the period delimiting the extension,
    // iconFile must be represented as UniChar[]
    {
        // Allocate a buffer and copy in the iconFile string
        UniChar* buffer = new UniChar[ wholeString.length ];
        CFStringGetCharacters( iconFile, wholeString, buffer );

        // Locate the period character
        OSStatus status = LSGetExtensionInfo( wholeString.length, buffer, &periodIndex );

        // Deallocate the buffer
        delete [] buffer;

        // If the period could not be located it will not be possible to get the URL
        if( status != noErr || periodIndex == kLSInvalidExtensionIndex )
            return wxEmptyString;
    }

    // Range representing the name part of iconFile
    CFRange iconNameRange;
    iconNameRange.location = 0;
    iconNameRange.length = periodIndex - 1;

    // Range representing the extension part of iconFile
    CFRange iconExtRange;
    iconExtRange.location = periodIndex;
    iconExtRange.length = wholeString.length - periodIndex;

    // Get the name and extension strings
    wxCFStringRef iconName = CFStringCreateWithSubstring( kCFAllocatorDefault, iconFile, iconNameRange );
    wxCFStringRef iconExt = CFStringCreateWithSubstring( kCFAllocatorDefault, iconFile, iconExtRange );

    // Now it is possible to query the URL for the icon as a resource
    wxCFRef< CFURLRef > iconUrl = wxCFRef< CFURLRef >( CFBundleCopyResourceURL( bundle, iconName, iconExt, NULL ) );

    if( !iconUrl.get() )
        return wxEmptyString;

    // All being well, return the icon path
    return wxCFStringRef( CFURLCopyFileSystemPath( iconUrl, kCFURLPOSIXPathStyle ) ).AsString();
}


wxMimeTypesManagerImpl::wxMimeTypesManagerImpl()
{
}

wxMimeTypesManagerImpl::~wxMimeTypesManagerImpl()
{
}


/////////////////////////////////////////////////////////////////////////////
// Init / shutdown functions
//
// The Launch Services / UTI API provides no helpful way of getting a list
// of all registered types. Instead the API is focused around looking up
// information for a particular file type once you already have some
// identifying piece of information. In order to get a list of registered
// types it would first be necessary to get a list of all bundles exporting
// type information (all application bundles may be sufficient) then look at
// the Info.plist file for those bundles and store the type information. As
// this would require trawling the hard disk when a wxWidgets program starts
// up it was decided instead to load the information lazily.
//
// If this behaviour really messes up your app, please feel free to implement
// the trawling approach (perhaps with a configure switch?). A good place to
// start would be CFBundleCreateBundlesFromDirectory( NULL, "/Applications", "app" )
/////////////////////////////////////////////////////////////////////////////


void wxMimeTypesManagerImpl::Initialize(int WXUNUSED(mailcapStyles), const wxString& WXUNUSED(extraDir))
{
    // NO-OP
}

void wxMimeTypesManagerImpl::ClearData()
{
    // NO-OP
}


/////////////////////////////////////////////////////////////////////////////
// Lookup functions
//
// Apple uses a number of different systems for file type information.
// As of Spring 2010, these include:
//
// OS Types / OS Creators
// File Extensions
// Mime Types
// Uniform Type Identifiers (UTI)
//
// This implementation of the type manager for Mac supports all except OS
// Type / OS Creator codes, which have been deprecated for some time with
// less and less support in recent versions of OS X.
//
// The UTI system is the internal system used by OS X, as such it offers a
// one-to-one mapping with file types understood by Mac OS X and is the
// easiest way to convert between type systems. However, UTI meta-data is
// not stored with data files (as of OS X 10.6), instead the OS looks at
// the file extension and uses this to determine the UTI. Simillarly, most
// applications do not yet advertise the file types they can handle by UTI.
//
// The result is that no one typing system is suitable for all tasks. Further,
// as there is not a one-to-one mapping between type systems for the
// description of any given type, it follows that ambiguity cannot be precluded,
// whichever system is taken to be the "master".
//
// In the implementation below I have used UTI as the master key for looking
// up file types. Extensions and mime types are mapped to UTIs and the data
// for each UTI contains a list of all associated extensions and mime types.
// This has the advantage that unknown types will still be assigned a unique
// ID, while using any other system as the master could result in conflicts
// if there were no mime type assigned to an extension or vice versa. However
// there is still plenty of room for ambiguity if two or more applications
// are fighting over ownership of a particular type or group of types.
//
// If this proves to be serious issue it may be helpful to add some slightly
// more cleve logic to the code so that the key used to look up a file type is
// always first in the list in the resulting wxFileType object. I.e, if you
// look up .mpeg3 the list you get back could be .mpeg3, mp3, .mpg3, while
// looking up .mp3 would give .mp3, .mpg3, .mpeg3. The simplest way to do
// this would probably to keep two separate sets of data, one for lookup
// by extetnsion and one for lookup by mime type.
//
// One other point which may require consideration is handling of unrecognised
// types. Using UTI these will be assigned a unique ID of dyn.xxx. This will
// result in a wxFileType object being returned, although querying properties
// on that object will fail. If it would be more helpful to return NULL in this
// case a suitable check can be added.
/////////////////////////////////////////////////////////////////////////////

// Look up a file type by extension
// The extensions if mapped to a UTI
// If the requested extension is not know the OS is querried and the results saved
wxFileType *wxMimeTypesManagerImpl::GetFileTypeFromExtension(const wxString& ext)
{
    wxString uti;

    const TagMap::const_iterator extItr = m_extMap.find( ext );

    if( extItr == m_extMap.end() )
    {
        wxCFStringRef utiRef = UTTypeCreatePreferredIdentifierForTag( kUTTagClassFilenameExtension, wxCFStringRef( ext ), NULL );
        m_extMap[ ext ] = uti = utiRef.AsString();
    }
    else
        uti = extItr->second;

    return GetFileTypeFromUti( uti );
}

// Look up a file type by mime type
// The mime type is mapped to a UTI
// If the requested extension is not know the OS is querried and the results saved
wxFileType *wxMimeTypesManagerImpl::GetFileTypeFromMimeType(const wxString& mimeType)
{
    wxString uti;

    const TagMap::const_iterator mimeItr = m_mimeMap.find( mimeType );

    if( mimeItr == m_mimeMap.end() )
    {
        wxCFStringRef utiRef = UTTypeCreatePreferredIdentifierForTag( kUTTagClassFilenameExtension, wxCFStringRef( mimeType ), NULL );
        m_mimeMap[ mimeType ] = uti = utiRef.AsString();
    }
    else
        uti = mimeItr->second;

    return GetFileTypeFromUti( uti );
}

// Look up a file type by UTI
// If the requested extension is not know the OS is querried and the results saved
wxFileType *wxMimeTypesManagerImpl::GetFileTypeFromUti(const wxString& uti)
{
    UtiMap::const_iterator utiItr = m_utiMap.find( uti );

    if( utiItr == m_utiMap.end() )
    {
        LoadTypeDataForUti( uti );
        LoadDisplayDataForUti( uti );
    }

    wxFileType* const ft = new wxFileType;
    ft->m_impl->m_uti = uti;
    ft->m_impl->m_manager = this;

    return ft;
}


/////////////////////////////////////////////////////////////////////////////
// Load functions
//
// These functions query the OS for information on a particular file type
/////////////////////////////////////////////////////////////////////////////


// Look up all extensions and mime types associated with a UTI
void wxMimeTypesManagerImpl::LoadTypeDataForUti(const wxString& uti)
{
    // Keys in to the UTI declaration plist
    const static wxCFStringRef tagsKey( "UTTypeTagSpecification" );
    const static wxCFStringRef extKey( "public.filename-extension" );
    const static wxCFStringRef mimeKey( "public.mime-type" );

    // Get the UTI as a CFString
    wxCFStringRef utiRef( uti );

    // Get a copy of the UTI declaration
    wxCFRef< CFDictionaryRef > utiDecl;
    utiDecl = wxCFRef< CFDictionaryRef >( UTTypeCopyDeclaration( utiRef ) );

    if( !utiDecl )
        return;

    // Get the tags spec (the section of a UTI declaration containing mappings to other type systems)
    CFTypeRef tagsData = CFDictionaryGetValue( utiDecl, tagsKey );

    if( CFGetTypeID( tagsData ) != CFDictionaryGetTypeID() )
        return;

    CFDictionaryRef tags = reinterpret_cast< CFDictionaryRef >( tagsData );

    // Read tags for extensions and mime types
    m_utiMap[ uti ].extensions = ReadStringListFromCFDict( tags, extKey );
    m_utiMap[ uti ].mimeTypes = ReadStringListFromCFDict( tags, mimeKey );
}


// Look up the (locale) display name and icon file associated with a UTI
void wxMimeTypesManagerImpl::LoadDisplayDataForUti(const wxString& uti)
{
    // Keys in to Info.plist
    const static wxCFStringRef docTypesKey( "CFBundleDocumentTypes" );
    const static wxCFStringRef descKey( "CFBundleTypeName" );
    const static wxCFStringRef iconKey( "CFBundleTypeIconFile" );

    // The call for finding the preferred application for a UTI is LSCopyDefaultRoleHandlerForContentType
    // This returns an empty string on OS X 10.5
    // Instead it is necessary to get the primary extension and use LSGetApplicationForInfo
    wxCFStringRef ext = UTTypeCopyPreferredTagWithClass( wxCFStringRef( uti ), kUTTagClassFilenameExtension );

    // Look up the preferred application
    CFURLRef appUrl;
    OSStatus status = LSGetApplicationForInfo( kLSUnknownType, kLSUnknownCreator, ext, kLSRolesAll, NULL, &appUrl );

    if( status != noErr )
        return;

    // Create a bundle object for that application
    wxCFRef< CFBundleRef > bundle;
    bundle = wxCFRef< CFBundleRef >( CFBundleCreate( kCFAllocatorDefault, appUrl ) );

    if( !bundle )
        return;

    // Also get the open command while we have the bundle
    wxCFStringRef cfsAppPath(CFURLCopyFileSystemPath(appUrl, kCFURLPOSIXPathStyle));
    m_utiMap[ uti ].application = cfsAppPath.AsString();

    // Get all the document type data in this bundle
    CFTypeRef docTypeData;
    docTypeData = CFBundleGetValueForInfoDictionaryKey( bundle, docTypesKey );

    if( !docTypeData )
        return;

    // Find the document type entry that matches ext
    CFDictionaryRef docType;
    docType = GetDocTypeForExt( docTypeData, ext );

    if( !docType )
        return;

    // Get the display name for docType
    wxCFStringRef description = reinterpret_cast< CFStringRef >( CFDictionaryGetValue( docType, descKey ) );
    wxCFRetain( description.get() );
    m_utiMap[ uti ].description = description.AsString();

    // Get the icon path for docType
    CFStringRef iconFile = reinterpret_cast< CFStringRef > ( CFDictionaryGetValue( docType, iconKey ) );
    m_utiMap[ uti ].iconLoc.SetFileName( GetPathForIconFile( bundle, iconFile ) );
}



/////////////////////////////////////////////////////////////////////////////
// The remaining functionality from the public interface of
// wxMimeTypesManagerImpl is not implemented.
//
// Please see the note further up this file on Initialise/Clear to explain why
// EnumAllFileTypes is not available.
//
// Some thought will be needed before implementing Associate / Unassociate
// for OS X to ensure proper integration with the many file type and
// association mechanisms already used by the OS. Leaving these methods as
// NO-OP on OS X and asking client apps to put suitable entries in their
// Info.plist files when building their OS X bundle may well be the
// correct solution.
/////////////////////////////////////////////////////////////////////////////


size_t wxMimeTypesManagerImpl::EnumAllFileTypes(wxArrayString& WXUNUSED(mimetypes))
{
    return 0;
}

wxFileType *wxMimeTypesManagerImpl::Associate(const wxFileTypeInfo& WXUNUSED(ftInfo))
{
    return 0;
}

bool wxMimeTypesManagerImpl::Unassociate(wxFileType *WXUNUSED(ft))
{
    return false;
}


/////////////////////////////////////////////////////////////////////////////
// Getter methods
//
// These methods are private and should only ever be called by wxFileTypeImpl
// after the required information has been loaded. It should not be possible
// to get a wxFileTypeImpl for a UTI without information for that UTI being
// querried, however it is possible that some information may not have been
// found.
/////////////////////////////////////////////////////////////////////////////



bool wxMimeTypesManagerImpl::GetExtensions(const wxString& uti, wxArrayString& extensions)
{
    const UtiMap::const_iterator itr = m_utiMap.find( uti );

    if( itr == m_utiMap.end() || itr->second.extensions.GetCount() < 1 )
    {
        extensions.Clear();
        return false;
    }

    extensions = itr->second.extensions;
    return true;
}

bool wxMimeTypesManagerImpl::GetMimeType(const wxString& uti, wxString *mimeType)
{
    const UtiMap::const_iterator itr = m_utiMap.find( uti );

    if( itr == m_utiMap.end() || itr->second.mimeTypes.GetCount() < 1 )
    {
        *mimeType = wxEmptyString;
        return false;
    }

    *mimeType = itr->second.mimeTypes[ 0 ];
    return true;
}

bool wxMimeTypesManagerImpl::GetMimeTypes(const wxString& uti, wxArrayString& mimeTypes)
{
    const UtiMap::const_iterator itr = m_utiMap.find( uti );

    if( itr == m_utiMap.end() || itr->second.mimeTypes.GetCount() < 1 )
    {
        mimeTypes.Clear();
        return false;
    }

    mimeTypes = itr->second.mimeTypes;
    return true;
}

bool wxMimeTypesManagerImpl::GetIcon(const wxString& uti, wxIconLocation *iconLoc)
{
    const UtiMap::const_iterator itr = m_utiMap.find( uti );

    if( itr == m_utiMap.end() || !itr->second.iconLoc.IsOk() )
    {
        *iconLoc = wxIconLocation();
        return false;
    }

    *iconLoc = itr->second.iconLoc;
    return true;
}

bool wxMimeTypesManagerImpl::GetDescription(const wxString& uti, wxString *desc)
{
    const UtiMap::const_iterator itr = m_utiMap.find( uti );

    if( itr == m_utiMap.end() || itr->second.description.empty() )
    {
        *desc = wxEmptyString;
        return false;
    }

    *desc = itr->second.description;
    return true;
}

bool wxMimeTypesManagerImpl::GetApplication(const wxString& uti, wxString *command)
{
    const UtiMap::const_iterator itr = m_utiMap.find( uti );

    if( itr == m_utiMap.end() )
    {
        command->clear();
        return false;
    }

    *command = itr->second.application;
    return true;
}

/////////////////////////////////////////////////////////////////////////////
// The remaining functionality has not yet been implemented for OS X
/////////////////////////////////////////////////////////////////////////////

wxFileTypeImpl::wxFileTypeImpl()
{
}

wxFileTypeImpl::~wxFileTypeImpl()
{
}

// Query wxMimeTypesManagerImple to get real information for a file type
bool wxFileTypeImpl::GetExtensions(wxArrayString& extensions) const
{
    return m_manager->GetExtensions( m_uti, extensions );
}

bool wxFileTypeImpl::GetMimeType(wxString *mimeType) const
{
    return m_manager->GetMimeType( m_uti, mimeType );
}

bool wxFileTypeImpl::GetMimeTypes(wxArrayString& mimeTypes) const
{
    return m_manager->GetMimeTypes( m_uti, mimeTypes );
}

bool wxFileTypeImpl::GetIcon(wxIconLocation *iconLoc) const
{
    return m_manager->GetIcon( m_uti, iconLoc );
}

bool wxFileTypeImpl::GetDescription(wxString *desc) const
{
    return m_manager->GetDescription( m_uti, desc );
}

namespace
{

// Helper function for GetOpenCommand(): returns the string surrounded by
// (singly) quotes if it contains spaces.
wxString QuoteIfNecessary(const wxString& path)
{
    wxString result(path);

    if ( path.find(' ') != wxString::npos )
    {
        result.insert(0, "'");
        result.append("'");
    }

    return result;
}

} // anonymous namespace

bool wxFileTypeImpl::GetOpenCommand(wxString *openCmd, const wxFileType::MessageParameters& params) const
{
    wxString application;
    if ( !m_manager->GetApplication(m_uti, &application) )
        return false;

    *openCmd << QuoteIfNecessary(application)
             << ' ' << QuoteIfNecessary(params.GetFileName());

    return true;
}

bool wxFileTypeImpl::GetPrintCommand(wxString *WXUNUSED(printCmd), const wxFileType::MessageParameters& WXUNUSED(params)) const
{
    return false;
}

size_t wxFileTypeImpl::GetAllCommands(wxArrayString *WXUNUSED(verbs), wxArrayString *WXUNUSED(commands), const wxFileType::MessageParameters& WXUNUSED(params)) const
{
    return false;
}

bool wxFileTypeImpl::SetCommand(const wxString& WXUNUSED(cmd), const wxString& WXUNUSED(verb), bool WXUNUSED(overwriteprompt))
{
    return false;
}

bool wxFileTypeImpl::SetDefaultIcon(const wxString& WXUNUSED(strIcon), int WXUNUSED(index))
{
    return false;
}

bool wxFileTypeImpl::Unassociate(wxFileType *WXUNUSED(ft))
{
    return false;
}

wxString
wxFileTypeImpl::GetExpandedCommand(const wxString& WXUNUSED(verb),
                                   const wxFileType::MessageParameters& WXUNUSED(params)) const
{
    return wxString();
}

#endif // wxUSE_MIMETYPE


