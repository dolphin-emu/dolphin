/////////////////////////////////////////////////////////////////////////////
// Name:        src/osx/carbon/mimetmac.cpp
// Purpose:     Mac Carbon implementation for wx MIME-related classes
// Author:      Ryan Norton
// Modified by:
// Created:     04/16/2005
// Copyright:   (c) 2005 Ryan Norton (<wxprojects@comcast.net>)
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

//
//  TODO: Search Info[-macos](classic).plist dictionary in addition
//  to Internet Config database.
//
//  Maybe try a brainstorm a way to change the wxMimeTypesManager API
//  to get info from a file instead/addition to current get all stuff
//  API so that we can use Launch Services to get MIME type info.
//
//  Implement GetIcon from one of the FinderInfo functions - or
//  use Launch Services and search that app's plist for the icon.
//
//  Put some special juice in for the print command.
//

// for compilers that support precompilation, includes "wx.h".
#include "wx/wxprec.h"

#ifdef __BORLANDC__
    #pragma hdrstop
#endif

#if wxUSE_MIMETYPE

#include "wx/osx/mimetype.h"

#ifndef WX_PRECOMP
    #include "wx/dynarray.h"
    #include "wx/string.h"
    #include "wx/intl.h"
    #include "wx/log.h"

    #if wxUSE_GUI
        #include "wx/icon.h"
    #endif
#endif

#include "wx/file.h"
#include "wx/confbase.h"

#include "wx/osx/private.h"

// other standard headers
#include <ctype.h>

#ifndef __DARWIN__
    #include <InternetConfig.h>
    #include <CoreServices.h>
#endif

#ifndef __LP64__
//   START CODE SAMPLE FROM TECHNOTE 1002 (http://developer.apple.com/technotes/tn/tn1002.html)

// IsRemoteVolume can be used to find out if the
// volume referred to by vRefNum is a remote volume
// located somewhere on a network. the volume's attribute
// flags (copied from the GetVolParmsInfoBuffer structure)
// are returned in the longword pointed to by vMAttrib.
OSErr IsRemoteVolume(short vRefNum, Boolean *isRemote, long *vMAttrib)
{
    HParamBlockRec volPB;
    GetVolParmsInfoBuffer volinfo;
    OSErr err;

    volPB.ioParam.ioVRefNum = vRefNum;
    volPB.ioParam.ioNamePtr = NULL;
    volPB.ioParam.ioBuffer = (Ptr)&volinfo;
    volPB.ioParam.ioReqCount = sizeof(volinfo);
    err = PBHGetVolParmsSync( &volPB );
    if (err == noErr)
    {
        *isRemote = (volinfo.vMServerAdr != 0);
        *vMAttrib = volinfo.vMAttrib;
    }

    return err;
}

// BuildVolumeList fills the array pointed to by vols with
// a list of the currently mounted volumes.  If includeRemote
// is true, then remote server volumes will be included in
// the list.  When remote server volumes are included in the
// list, they will be added to the end of the list.  On entry,
// *count should contain the size of the array pointed to by
// vols.  On exit, *count will be set to the number of id numbers
// placed in the array. If vMAttribMask is non-zero, then
// only volumes with matching attributes are added to the
// list of volumes. bits in the vMAttribMask should use the
// same encoding as bits in the vMAttrib field of
// the GetVolParmsInfoBuffer structure.
OSErr BuildVolumeList(Boolean includeRemote, short *vols,
        long *count, long vMAttribMask)
{
    HParamBlockRec volPB;
    Boolean isRemote;
    OSErr err = noErr;
    long nlocal, nremote;
    long vMAttrib;

    // set up and check parameters
    volPB.volumeParam.ioNamePtr = NULL;
    nlocal = nremote = 0;
    if (*count == 0)
        return noErr;

    // iterate through volumes
    for (volPB.volumeParam.ioVolIndex = 1;
        PBHGetVInfoSync(&volPB) == noErr;
        volPB.volumeParam.ioVolIndex++)
    {
        // skip remote volumes, if necessary
        err = IsRemoteVolume(volPB.volumeParam.ioVRefNum, &isRemote, &vMAttrib);
        if (err != noErr)
            goto bail;

        if ((includeRemote || !isRemote) && ((vMAttrib & vMAttribMask) == vMAttribMask))
        {
            // add local volumes at the front; remote volumes at the end
            if (isRemote)
                vols[nlocal + nremote++] = volPB.volumeParam.ioVRefNum;
            else
            {
                if (nremote > 0)
                    BlockMoveData(
                        vols + nlocal,
                        vols + nlocal + 1,
                        nremote * sizeof(short) );
                vols[nlocal++] = volPB.volumeParam.ioVRefNum;
            }

            // list full?
            if ((nlocal + nremote) >= *count)
                break;
        }
    }

bail:
    *count = (nlocal + nremote);

    return err;
}


// FindApplication iterates through mounted volumes
// searching for an application with the given creator
// type.  If includeRemote is true, then remote volumes
// will be searched (after local ones) for an application
// with the creator type.
//
// Hacked to output to appName
//
#define kMaxVols 20

OSErr FindApplication(OSType appCreator, Boolean includeRemote, Str255 appName, FSSpec* appSpec)
{
    short rRefNums[kMaxVols];
    long i, volCount;
    DTPBRec desktopPB;
    OSErr err;

    // get a list of volumes - with desktop files
    volCount = kMaxVols;
    err = BuildVolumeList(includeRemote, rRefNums, &volCount, (1 << bHasDesktopMgr) );
    if (err != noErr)
        return err;

    // iterate through the list
    for (i=0; i<volCount; i++)
    {
        // has a desktop file?
        desktopPB.ioCompletion = NULL;
        desktopPB.ioVRefNum = rRefNums[i];
        desktopPB.ioNamePtr = NULL;
        desktopPB.ioIndex = 0;
        err = PBDTGetPath( &desktopPB );
        if (err != noErr)
            continue;

        // has the correct app??
        desktopPB.ioFileCreator = appCreator;
        desktopPB.ioNamePtr = appName;
        err = PBDTGetAPPLSync( &desktopPB );
        if (err != noErr)
            continue;

        // make a file spec referring to it
        err = FSMakeFSSpec( rRefNums[i], desktopPB.ioAPPLParID, appName, appSpec );
        if (err != noErr)
            continue;

        // found it!
        return noErr;
    }

    return fnfErr;
}

// END CODE SAMPLE FROM TECHNOTE 1002 (http://developer.apple.com/technotes/tn/tn1002.html)

// yeah, duplicated code
pascal OSErr FSpGetFullPath( const FSSpec *spec,
    short *fullPathLength,
    Handle *fullPath )
{
    OSErr result, realResult;
    FSSpec tempSpec;
    CInfoPBRec pb;

    *fullPathLength = 0;
    *fullPath = NULL;

    // default to noErr
    realResult = result = noErr;

  // work around Nav Services "bug" (it returns invalid FSSpecs with empty names)
#if 0
    if ( spec->name[0] == 0 )
    {
        result = FSMakeFSSpecCompat(spec->vRefNum, spec->parID, spec->name, &tempSpec);
    }
    else
    {
#endif

    // Make a copy of the input FSSpec that can be modified
    BlockMoveData( spec, &tempSpec, sizeof(FSSpec) );

    if ( result == noErr )
    {
        if ( tempSpec.parID == fsRtParID )
        {
            // object is a volume
            // Add a colon to make it a full pathname
            ++tempSpec.name[0];
            tempSpec.name[tempSpec.name[0]] = ':';

            // We're done
            result = PtrToHand(&tempSpec.name[1], fullPath, tempSpec.name[0]);
        }
        else
        {
            // object isn't a volume

            // Is the object a file or a directory?
            pb.dirInfo.ioNamePtr = tempSpec.name;
            pb.dirInfo.ioVRefNum = tempSpec.vRefNum;
            pb.dirInfo.ioDrDirID = tempSpec.parID;
            pb.dirInfo.ioFDirIndex = 0;
            result = PBGetCatInfoSync( &pb );

            // Allow file/directory name at end of path to not exist.
            realResult = result;
            if ((result == noErr) || (result == fnfErr))
            {
                // if the object is a directory, append a colon so full pathname ends with colon
                if ((result == noErr) && (pb.hFileInfo.ioFlAttrib & kioFlAttribDirMask) != 0)
                {
                    ++tempSpec.name[0];
                    tempSpec.name[tempSpec.name[0]] = ':';
                }

                // Put the object name in first
                result = PtrToHand( &tempSpec.name[1], fullPath, tempSpec.name[0] );
                if ( result == noErr )
                {
                    // Get the ancestor directory names
                    pb.dirInfo.ioNamePtr = tempSpec.name;
                    pb.dirInfo.ioVRefNum = tempSpec.vRefNum;
                    pb.dirInfo.ioDrParID = tempSpec.parID;

                    // loop until we have an error or find the root directory
                    do
                    {
                        pb.dirInfo.ioFDirIndex = -1;
                        pb.dirInfo.ioDrDirID = pb.dirInfo.ioDrParID;
                        result = PBGetCatInfoSync(&pb);
                        if ( result == noErr )
                        {
                            // Append colon to directory name
                            ++tempSpec.name[0];
                            tempSpec.name[tempSpec.name[0]] = ':';

                            // Add directory name to beginning of fullPath
                            (void)Munger(*fullPath, 0, NULL, 0, &tempSpec.name[1], tempSpec.name[0]);
                            result = MemError();
                        }
                    }
                    while ( (result == noErr) && (pb.dirInfo.ioDrDirID != fsRtDirID) );
                }
            }
        }
    }

    if ( result == noErr )
    {
        // Return the length
        *fullPathLength = GetHandleSize( *fullPath );
        result = realResult;  // return realResult in case it was fnfErr
    }
    else
    {
        // Dispose of the handle and return NULL and zero length
        if ( *fullPath != NULL )
        {
            DisposeHandle( *fullPath );
           *fullPath = NULL;
        }
        *fullPathLength = 0;
    }

    return result;
}
#endif // LP64
//
// On the mac there are two ways to open a file - one is through apple events and the
// finder, another is through mime types.
//
// So, really there are two ways to implement wxFileType...
//
// Mime types are only available on OS 8.1+ through the InternetConfig API
//
// Much like the old-style file manager, it has 3 levels of flexibility for its methods -
// Low - which means you have to iterate yourself through the mime database
// Medium - which lets you sort of cache the database if you want to use lowlevel functions
// High - which requires access to the database every time
//
// We want to be efficient (i.e. professional :) ) about it, so we use a combo of low
// and mid-level functions
//
// TODO: Should we call ICBegin/ICEnd?  Then where?
//

// debug helper
inline void wxLogMimeDebug(const wxChar* WXUNUSED_UNLESS_DEBUG(szMsg), OSStatus WXUNUSED_UNLESS_DEBUG(status))
{
    wxLogDebug(wxString::Format(wxT("%s  LINE:%i  OSERROR:%i"), szMsg, __LINE__, (int)status));
}

// in case we're compiling in non-GUI mode
class WXDLLIMPEXP_FWD_CORE wxIcon;

bool wxFileTypeImpl::SetCommand(const wxString& WXUNUSED(cmd), const wxString& WXUNUSED(verb), bool WXUNUSED(overwriteprompt))
{
    wxASSERT_MSG( m_manager != NULL , wxT("Bad wxFileType") );

    return false;
}

bool wxFileTypeImpl::SetDefaultIcon(const wxString& WXUNUSED(strIcon), int WXUNUSED(index))
{
    wxASSERT_MSG( m_manager != NULL , wxT("Bad wxFileType") );

    return false;
}

bool wxFileTypeImpl::GetOpenCommand(wxString *openCmd,
                               const wxFileType::MessageParameters& params) const
{
    wxString cmd = GetCommand(wxT("open"));

    *openCmd = wxFileType::ExpandCommand(cmd, params);

    return !openCmd->empty();
}

bool
wxFileTypeImpl::GetPrintCommand(
    wxString *printCmd,
    const wxFileType::MessageParameters& params) const
{
    wxString cmd = GetCommand(wxT("print"));

    *printCmd = wxFileType::ExpandCommand(cmd, params);

    return !printCmd->empty();
}

//
// Internet Config vs. Launch Services
//
// From OS 8 on there was internet config...
// However, OSX and its finder does not use info
// from Internet Config at all - the Internet Config
// database ONLY CONTAINS APPS THAT ARE CLASSIC APPS
// OR REGISTERED THROUGH INTERNET CONFIG
//
// Therefore on OSX in order for the open command to be useful
// we need to go straight to launch services
//

//on darwin, use launch services

#include <ApplicationServices/ApplicationServices.h>

wxString wxFileTypeImpl::GetCommand(const wxString& verb) const
{
    wxASSERT_MSG( m_manager != NULL , wxT("Bad wxFileType") );

    if (verb == wxT("open"))
    {
        ICMapEntry entry;
        ICGetMapEntry( (ICInstance) m_manager->m_hIC,
                       (Handle) m_manager->m_hDatabase,
                       m_lIndex, &entry);

        wxString sCurrentExtension = wxMacMakeStringFromPascal(entry.extension);
        sCurrentExtension = sCurrentExtension.Right(sCurrentExtension.length()-1 );

        //type, creator, ext, roles, outapp (FSRef), outappurl
        CFURLRef cfurlAppPath;
        OSStatus status = LSGetApplicationForInfo( kLSUnknownType,
            kLSUnknownCreator,
            wxCFStringRef(sCurrentExtension, wxLocale::GetSystemEncoding()),
            kLSRolesAll,
            NULL,
            &cfurlAppPath );

        if (status == noErr)
        {
            CFStringRef cfsUnixPath = CFURLCopyFileSystemPath(cfurlAppPath, kCFURLPOSIXPathStyle);
            CFRelease(cfurlAppPath);

            // PHEW!  Success!
            // Since a filename might have spaces in it, so surround it with quotes
            if (cfsUnixPath)
            {
                wxString resultStr;

                resultStr =
                    wxString(wxT("'"))
                    + wxCFStringRef(cfsUnixPath).AsString(wxLocale::GetSystemEncoding())
                    + wxString(wxT("'"));

               return resultStr;
            }
        }
        else
        {
            wxLogDebug(wxString::Format(wxT("%i - %s - %i"),
            __LINE__,
            wxT("LSGetApplicationForInfo failed."),
            (int)status));
        }
    }

    return wxEmptyString;
}

bool wxFileTypeImpl::GetDescription(wxString *desc) const
{
    wxASSERT_MSG( m_manager != NULL , wxT("Bad wxFileType") );

    ICMapEntry entry;
    ICGetMapEntry( (ICInstance) m_manager->m_hIC,
        (Handle) m_manager->m_hDatabase, m_lIndex, &entry );

    *desc = wxMacMakeStringFromPascal( entry.entryName );

    return true;
}

bool wxFileTypeImpl::GetExtensions(wxArrayString& extensions)
{
    wxASSERT_MSG( m_manager != NULL , wxT("Bad wxFileType") );

    ICMapEntry entry;
    ICGetMapEntry( (ICInstance) m_manager->m_hIC,
        (Handle) m_manager->m_hDatabase, m_lIndex, &entry );

    //entry has period in it
    wxString sCurrentExtension = wxMacMakeStringFromPascal( entry.extension );
    extensions.Add( sCurrentExtension.Right( sCurrentExtension.length() - 1 ) );

    return true;
}

bool wxFileTypeImpl::GetMimeType(wxString *mimeType) const
{
    wxASSERT_MSG( m_manager != NULL , wxT("Bad wxFileType") );

    ICMapEntry entry;
    ICGetMapEntry( (ICInstance) m_manager->m_hIC,
        (Handle) m_manager->m_hDatabase, m_lIndex, &entry );

    *mimeType = wxMacMakeStringFromPascal(entry.MIMEType);

    return true;
}

bool wxFileTypeImpl::GetMimeTypes(wxArrayString& mimeTypes) const
{
    wxString s;

    if (GetMimeType(&s))
    {
        mimeTypes.Clear();
        mimeTypes.Add(s);

        return true;
    }

    return false;
}

bool wxFileTypeImpl::GetIcon(wxIconLocation *WXUNUSED(icon)) const
{
    wxASSERT_MSG( m_manager != NULL , wxT("Bad wxFileType") );

    // no such file type or no value or incorrect icon entry
    return false;
}

size_t wxFileTypeImpl::GetAllCommands(wxArrayString * verbs,
    wxArrayString * commands,
    const wxFileType::MessageParameters& params) const
{
    wxASSERT_MSG( m_manager != NULL , wxT("Bad wxFileType") );

    wxString sCommand;
    size_t ulCount = 0;

    if (GetOpenCommand(&sCommand, params))
    {
        verbs->Add(wxString(wxT("open")));
        commands->Add(sCommand);
        ++ulCount;
    }

    return ulCount;
}

void wxMimeTypesManagerImpl::Initialize(int WXUNUSED(mailcapStyles), const wxString& WXUNUSED(extraDir))
{
    wxASSERT_MSG(m_hIC == NULL, wxT("Already initialized wxMimeTypesManager!"));

    // some apps (non-wx) use the 'plst' resource instead
#if 0
    CFBundleRef cfbMain = CFBundleGetMainBundle();
    wxCFDictionary cfdInfo( CFBundleGetInfoDictionary(cfbMain), wxCF_RETAIN );
    wxString sLog;
    cfdInfo.PrintOut(sLog);
    wxLogDebug(sLog);
#endif

    // start Internet Config - log if there's an error
    // the second param is the signature of the application, also known
    // as resource ID 0.  However, as per some recent discussions, we may not
    // have a signature for this app, so a generic 'APPL' which is the executable
    // type will work for now.
    OSStatus status = ICStart( (ICInstance*)&m_hIC, 'APPL' );

    if (status != noErr)
    {
        wxLogDebug(wxT("Could not initialize wxMimeTypesManager!"));
        wxFAIL;
        m_hIC = NULL;

        return;
    }

    ICAttr attr;
    m_hDatabase = (void**) NewHandle(0);
    status = ICFindPrefHandle( (ICInstance) m_hIC, kICMapping, &attr, (Handle) m_hDatabase );

    //the database file can be corrupt (on OSX its
    //~/Library/Preferences/com.apple.internetconfig.plist)
    //- bail if it is
    if (status != noErr)
    {
        ClearData();
        wxLogDebug(wxT("Corrupt MIME database!"));
        return;
    }

    //obtain the number of entries in the map
    status = ICCountMapEntries( (ICInstance) m_hIC, (Handle) m_hDatabase, &m_lCount );
    wxASSERT( status == noErr );

#if 0
    //debug stuff
    ICMapEntry entry;
    long pos;

    for (long i = 1; i <= m_lCount; ++i)
    {
        OSStatus status = ICGetIndMapEntry( (ICInstance) m_hIC, (Handle) m_hDatabase, i, &pos, &entry );

        if (status == noErr)
        {
            wxString sCreator = wxMacMakeStringFromPascal(entry.creatorAppName);
            wxString sCurrentExtension = wxMacMakeStringFromPascal(entry.extension);
            wxString sMIMEType = wxMacMakeStringFromPascal(entry.MIMEType);

            wxFileTypeImpl impl;
            impl.Init(this, pos);

            if (sMIMEType == wxT("text/html") && sCurrentExtension == wxT(".html"))
            {
                wxString cmd;

                impl.GetOpenCommand( &cmd, wxFileType::MessageParameters (wxT("http://www.google.com")));
                wxPrintf(wxT("APP: [%s]\n"), cmd.c_str());
            }
        }
    }
#endif
}

void wxMimeTypesManagerImpl::ClearData()
{
    if (m_hIC != NULL)
    {
        DisposeHandle( (Handle)m_hDatabase );

        // this can return an error, but we don't really care that much about it
        ICStop( (ICInstance)m_hIC );
        m_hIC = NULL;
    }
}

//
//  Q) Iterating through the map - why does it use if (err == noErr) instead of just asserting?
//  A) Some intermediate indexes are bad while subsequent ones may be good.  Its weird, I know.
//

// extension -> file type
wxFileType* wxMimeTypesManagerImpl::GetFileTypeFromExtension(const wxString& e)
{
    wxASSERT_MSG( m_hIC != NULL, wxT("wxMimeTypesManager not Initialized!") );

    //low level functions - iterate through the database
    ICMapEntry entry;
    long pos;

    for (long i = 1; i <= m_lCount; ++i)
    {
        OSStatus status = ICGetIndMapEntry( (ICInstance) m_hIC, (Handle) m_hDatabase, i, &pos, &entry );

        if (status == noErr)
        {
            wxString sCurrentExtension = wxMacMakeStringFromPascal(entry.extension);
            if ( sCurrentExtension.Right(sCurrentExtension.length() - 1) == e ) // entry has period in it
            {
                wxFileType* pFileType = new wxFileType();
                pFileType->m_impl->Init((wxMimeTypesManagerImpl*)this, pos);

                return pFileType;
            }
        }
    }

    return NULL;
}

// MIME type -> extension -> file type
wxFileType* wxMimeTypesManagerImpl::GetFileTypeFromMimeType(const wxString& mimeType)
{
    wxASSERT_MSG( m_hIC != NULL, wxT("wxMimeTypesManager not Initialized!") );

    ICMapEntry entry;
    long pos;

    // low level functions - iterate through the database
    for (long i = 1; i <= m_lCount; ++i)
    {
        OSStatus status = ICGetIndMapEntry( (ICInstance) m_hIC, (Handle) m_hDatabase, i, &pos, &entry );
        wxASSERT_MSG( status == noErr, wxString::Format(wxT("Error: %d"), (int)status) );

        if (status == noErr)
        {
            if ( wxMacMakeStringFromPascal(entry.MIMEType) == mimeType)
            {
                wxFileType* pFileType = new wxFileType();
                pFileType->m_impl->Init((wxMimeTypesManagerImpl*)this, pos);

                return pFileType;
            }
        }
    }

    return NULL;
}

size_t wxMimeTypesManagerImpl::EnumAllFileTypes(wxArrayString& mimetypes)
{
    wxASSERT_MSG( m_hIC != NULL, wxT("wxMimeTypesManager not Initialized!") );

    ICMapEntry entry;
    long pos, lStartCount;

    // low level functions - iterate through the database
    lStartCount = (long) mimetypes.GetCount();
    for (long i = 1; i <= m_lCount; ++i)
    {
        OSStatus status = ICGetIndMapEntry( (ICInstance) m_hIC, (Handle) m_hDatabase, i, &pos, &entry );
        if ( status == noErr )
            mimetypes.Add( wxMacMakeStringFromPascal(entry.MIMEType) );
    }

    return mimetypes.GetCount() - lStartCount;
}

pascal  OSStatus  MoreProcGetProcessTypeSignature(
    const ProcessSerialNumberPtr pPSN,
    OSType *pProcessType,
    OSType *pCreator)
{
    OSStatus anErr = noErr;
    ProcessInfoRec infoRec;
    ProcessSerialNumber localPSN;

    infoRec.processInfoLength = sizeof(ProcessInfoRec);
    infoRec.processName = NULL;
#ifndef __LP64__
    infoRec.processAppSpec = NULL;
#endif

    if ( pPSN == NULL )
    {
        localPSN.highLongOfPSN = 0;
        localPSN.lowLongOfPSN  = kCurrentProcess;
    }
    else
    {
        localPSN = *pPSN;
    }

    anErr = GetProcessInformation(&localPSN, &infoRec);
    if (anErr == noErr)
    {
        *pProcessType = infoRec.processType;
        *pCreator = infoRec.processSignature;
    }

    return anErr;
}

//
//
//  TODO: clean this up, its messy
//
//
//

#include "wx/osx/core/cfstring.h"

#define wxCF_RELEASE true
#define wxCF_RETAIN  false

// ----------------------------------------------------------------------------
// wxCFDictionary
// ----------------------------------------------------------------------------

class wxCFDictionary
{
public:
    wxCFDictionary(CFTypeRef ref)
    {
        m_cfmdRef = (CFMutableDictionaryRef) ref;
    }

    wxCFDictionary(CFIndex cfiSize = 0)
    {
        CFDictionaryKeyCallBacks kcbs;
        CFDictionaryValueCallBacks vcbs;
        BuildKeyCallbacks(&kcbs);
        BuildValueCallbacks(&vcbs);

        m_cfmdRef = CFDictionaryCreateMutable(
            kCFAllocatorDefault, cfiSize, &kcbs, &vcbs );
    }

    ~wxCFDictionary()
    { Clear(); }

    void Clear()
    {
        if (m_cfmdRef)
            CFRelease(m_cfmdRef);
    }

    static const void* RetainProc(CFAllocatorRef, const void* v)
    { return (const void*) CFRetain(v); }

    static void ReleaseProc(CFAllocatorRef, const void* v)
    { CFRelease(v); }

    void MakeMutable(CFIndex cfiSize = 0)
    {
        CFDictionaryRef oldref = (CFDictionaryRef) m_cfmdRef;

        m_cfmdRef = CFDictionaryCreateMutableCopy(
            kCFAllocatorDefault, cfiSize, oldref );

        CFRelease( oldref );
    }

    void BuildKeyCallbacks(CFDictionaryKeyCallBacks* pCbs)
    {
        pCbs->version = 0;
        pCbs->retain = RetainProc;
        pCbs->release = ReleaseProc;
        pCbs->copyDescription = NULL;
        pCbs->equal = NULL;
        pCbs->hash = NULL;
    }

    void BuildValueCallbacks(CFDictionaryValueCallBacks* pCbs)
    {
        pCbs->version = 0;
        pCbs->retain = RetainProc;
        pCbs->release = ReleaseProc;
        pCbs->copyDescription = NULL;
        pCbs->equal = NULL;
    }

    operator CFTypeRef () const
    { return (CFTypeRef)m_cfmdRef; }

    CFDictionaryRef GetCFDictionary() const
    { return (CFDictionaryRef)m_cfmdRef; }

    CFMutableDictionaryRef GetCFMutableDictionary()
    { return (CFMutableDictionaryRef) m_cfmdRef; }

    CFTypeRef operator [] (CFTypeRef cftEntry) const
    {
        wxASSERT(IsValid());
        return (CFTypeRef) CFDictionaryGetValue((CFDictionaryRef)m_cfmdRef, cftEntry);
    }

    CFIndex GetCount() const
    {
        wxASSERT(IsValid());
        return CFDictionaryGetCount((CFDictionaryRef)m_cfmdRef);
    }

    void Add(CFTypeRef cftKey, CFTypeRef cftValue)
    {
        wxASSERT(IsValid());
        wxASSERT(Exists(cftKey) == false);
        CFDictionaryAddValue(m_cfmdRef, cftKey, cftValue);
    }

    void Remove(CFTypeRef cftKey)
    {
        wxASSERT(IsValid());
        wxASSERT(Exists(cftKey));
        CFDictionaryRemoveValue(m_cfmdRef, cftKey);
    }

    void Set(CFTypeRef cftKey, CFTypeRef cftValue)
    {
        wxASSERT(IsValid());
        wxASSERT(Exists(cftKey));
        CFDictionarySetValue(m_cfmdRef, cftKey, cftValue);
    }

    bool Exists(CFTypeRef cftKey) const
    {
        wxASSERT(IsValid());
        return CFDictionaryContainsKey((CFDictionaryRef)m_cfmdRef, cftKey);
    }

    bool IsOk() const
    { return m_cfmdRef != NULL; }

    bool IsValid() const
    { return IsOk() && CFGetTypeID((CFTypeRef)m_cfmdRef) == CFDictionaryGetTypeID(); }

    void PrintOut(wxString& sMessage)
    {
        PrintOutDictionary(sMessage, m_cfmdRef);
    }

    static void PrintOutDictionary(wxString& sMessage, CFDictionaryRef cfdRef)
    {
        CFIndex cfiCount = CFDictionaryGetCount(cfdRef);
        CFTypeRef* pKeys = new CFTypeRef[cfiCount];
        CFTypeRef* pValues = new CFTypeRef[cfiCount];

        CFDictionaryGetKeysAndValues(cfdRef, pKeys, pValues);

        for (CFIndex i = 0; i < cfiCount; ++i)
        {
            wxString sKey = wxCFStringRef(CFCopyTypeIDDescription(CFGetTypeID(pKeys[i]))).AsString();
            wxString sValue = wxCFStringRef(CFCopyTypeIDDescription(CFGetTypeID(pValues[i]))).AsString();

            sMessage <<
                wxString::Format(wxT("[{#%d} Key : %s]"), (int) i,
                        sKey.c_str());

            PrintOutType(sMessage, sKey, pKeys[i]);

            sMessage <<
                wxString::Format(wxT("\n\t[Value : %s]"),
                        sValue.c_str());

            PrintOutType(sMessage, sValue, pValues[i]);

            sMessage << wxT("\n");
        }

        delete [] pKeys;
        delete [] pValues;
    }

    static void PrintOutArray(wxString& sMessage, CFArrayRef cfaRef)
    {
        for (CFIndex i = 0; i < CFArrayGetCount(cfaRef); ++i)
        {
            wxString sValue = wxCFStringRef(CFCopyTypeIDDescription(CFGetTypeID(
                CFArrayGetValueAtIndex(cfaRef, i)
                ))).AsString();

            sMessage <<
                wxString::Format(wxT("\t\t[{#%d} ArrayValue : %s]\n"), (int) i,
                        sValue.c_str());

            PrintOutType(sMessage, sValue, CFArrayGetValueAtIndex(cfaRef, i));
        }
    }

    static void PrintOutType(wxString& sMessage, const wxString& sValue, CFTypeRef cfRef)
    {
            sMessage << wxT(" {");

            if (sValue == wxT("CFString"))
            {
                 sMessage << wxCFStringRef(wxCFRetain((CFStringRef)cfRef)).AsString();
            }
            else if (sValue == wxT("CFNumber"))
            {
                int nOut;
                CFNumberGetValue((CFNumberRef)cfRef, kCFNumberIntType, &nOut);
                sMessage << nOut;
            }
            else if (sValue == wxT("CFDictionary"))
            {
                PrintOutDictionary(sMessage, (CFDictionaryRef)cfRef);
            }
            else if (sValue == wxT("CFArray"))
            {
                PrintOutArray(sMessage, (CFArrayRef)cfRef);
            }
            else if (sValue == wxT("CFBoolean"))
            {
                sMessage << (cfRef == kCFBooleanTrue ? wxT("true") : wxT("false"));
            }
            else if (sValue == wxT("CFURL"))
            {
                sMessage << wxCFStringRef(CFURLCopyPath((CFURLRef) cfRef)).AsString();
            }
            else
            {
                sMessage << wxT("*****UNKNOWN TYPE******");
            }

            sMessage << wxT("} ");
    }

#if wxUSE_MIMETYPE
    void MakeValidXML();
#endif

    CFTypeRef WriteAsXML()
    {
        return CFPropertyListCreateXMLData(kCFAllocatorDefault, m_cfmdRef);
    }

    bool ReadAsXML(CFTypeRef cfData, wxString* pErrorMsg = NULL)
    {
        Clear();
        CFStringRef cfsError=NULL;
        m_cfmdRef = (CFMutableDictionaryRef) CFPropertyListCreateFromXMLData(
                    kCFAllocatorDefault,
                    (CFDataRef)cfData,
                    kCFPropertyListMutableContainersAndLeaves,
                    &cfsError );
        if (cfsError)
        {
            if (pErrorMsg)
                *pErrorMsg = wxCFStringRef(cfsError).AsString();
            else
                CFRelease(cfsError);
        }

        return m_cfmdRef != NULL;
    }

private:
    CFMutableDictionaryRef m_cfmdRef;
};

// ----------------------------------------------------------------------------
// wxCFArray
// ----------------------------------------------------------------------------

class wxCFArray
{
public:
    wxCFArray(CFTypeRef ref)
    {
        m_cfmaRef = (CFMutableArrayRef)ref;
    }

    wxCFArray(CFIndex cfiSize = 0) : m_cfmaRef(NULL)
    { Create(cfiSize); }

    ~wxCFArray()
    { Clear(); }

    void MakeMutable(CFIndex cfiSize = 0)
    {
        wxASSERT(IsValid());

        CFMutableArrayRef oldref = m_cfmaRef;
        m_cfmaRef = CFArrayCreateMutableCopy(
                kCFAllocatorDefault,
                cfiSize,
                (CFArrayRef)oldref);
        CFRelease(oldref);
    }

    void BuildCallbacks(CFArrayCallBacks* pCbs)
    {
        pCbs->version = 0;
        pCbs->retain = RetainProc;
        pCbs->release = ReleaseProc;
        pCbs->copyDescription = NULL;
        pCbs->equal = NULL;
    }

    void Create(CFIndex cfiSize = 0)
    {
        Clear();
        CFArrayCallBacks cb;
        BuildCallbacks(&cb);

        m_cfmaRef = CFArrayCreateMutable(kCFAllocatorDefault, cfiSize, &cb);
    }

    void Clear()
    { if (m_cfmaRef) CFRelease(m_cfmaRef); }

    static const void* RetainProc(CFAllocatorRef, const void* v)
    { return (const void*) CFRetain(v); }

    static void ReleaseProc(CFAllocatorRef, const void* v)
    { CFRelease(v); }

    operator CFTypeRef () const
    { return (CFTypeRef)m_cfmaRef; }

    CFArrayRef GetCFArray() const
    { return (CFArrayRef)m_cfmaRef; }

    CFMutableArrayRef GetCFMutableArray()
    { return (CFMutableArrayRef) m_cfmaRef; }

    CFTypeRef operator [] (CFIndex cfiIndex) const
    {
        wxASSERT(IsValid());
        return (CFTypeRef) CFArrayGetValueAtIndex((CFArrayRef)m_cfmaRef, cfiIndex);
    }

    CFIndex GetCount()
    {
        wxASSERT(IsValid());
        return CFArrayGetCount((CFArrayRef)m_cfmaRef);
    }

    void Add(CFTypeRef cftValue)
    {
        wxASSERT(IsValid());
        CFArrayAppendValue(m_cfmaRef, cftValue);
    }

    void Remove(CFIndex cfiIndex)
    {
        wxASSERT(IsValid());
        wxASSERT(cfiIndex < GetCount());
        CFArrayRemoveValueAtIndex(m_cfmaRef, cfiIndex);
    }

    void Set(CFIndex cfiIndex, CFTypeRef cftValue)
    {
        wxASSERT(IsValid());
        wxASSERT(cfiIndex < GetCount());
        CFArraySetValueAtIndex(m_cfmaRef, cfiIndex, cftValue);
    }

    bool IsOk() const
    { return m_cfmaRef != NULL; }

    bool IsValid() const
    {
        return IsOk() && CFGetTypeID((CFTypeRef)m_cfmaRef) == CFArrayGetTypeID();
    }

#if wxUSE_MIMETYPE
    void MakeValidXML();
#endif

private:
    CFMutableArrayRef m_cfmaRef;
};

// ----------------------------------------------------------------------------
// wxCFNumber
// ----------------------------------------------------------------------------

class wxCFNumber
{
public:
    wxCFNumber(int nValue)
    {
        m_cfnRef = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &nValue);
    }

    wxCFNumber(CFTypeRef ref) : m_cfnRef((CFNumberRef)ref)
    {
    }

    virtual ~wxCFNumber()
    {
        if (m_cfnRef)
            CFRelease(m_cfnRef);
    }

    operator CFTypeRef() const
    { return (CFTypeRef) m_cfnRef; }

    int GetValue()
    {
        int nOut;
        CFNumberGetValue( m_cfnRef, kCFNumberIntType, &nOut );

        return nOut;
    }

    bool IsOk()
    { return m_cfnRef != NULL; }

private:
    CFNumberRef m_cfnRef;
};

// ----------------------------------------------------------------------------
// wxCFURL
// ----------------------------------------------------------------------------

class wxCFURL
{
public:
    wxCFURL(CFTypeRef ref = NULL) : m_cfurlRef((CFURLRef)ref)
    {
    }

    wxCFURL(const wxCFStringRef& URLString, CFTypeRef BaseURL = NULL)
    {
        Create(URLString, BaseURL);
    }

    void Create(const wxCFStringRef& URLString, CFTypeRef BaseURL = NULL)
    {
        m_cfurlRef = CFURLCreateWithString(
            kCFAllocatorDefault,
            (CFStringRef)(CFTypeRef)URLString,
            (CFURLRef) BaseURL);
    }

    virtual ~wxCFURL()
    {
        if (m_cfurlRef)
            CFRelease(m_cfurlRef);
    }

    wxString BuildWXString()
    {
        return wxCFStringRef(CFURLCopyPath(m_cfurlRef)).AsString();
    }

    operator CFTypeRef() const
    { return (CFTypeRef)m_cfurlRef; }

    bool IsOk()
    { return m_cfurlRef != NULL; }

private:
    CFURLRef m_cfurlRef;
};

// ----------------------------------------------------------------------------
// wxCFData
// ----------------------------------------------------------------------------

#define wxCFDATA_RELEASEBUFFER  1

class wxCFData
{
public:
    wxCFData(CFTypeRef ref) : m_cfdaRef((CFDataRef)ref)
    {
    }

    wxCFData(const UInt8* pBytes, CFIndex len, bool bKeep = wxCFDATA_RELEASEBUFFER)
    {
        if (bKeep == wxCFDATA_RELEASEBUFFER)
        {
            m_cfdaRef = CFDataCreateWithBytesNoCopy
                            (kCFAllocatorDefault, pBytes, len, kCFAllocatorDefault);
        }
        else
        {
            m_cfdaRef = CFDataCreate(kCFAllocatorDefault, pBytes, len);
        }
    }

    virtual ~wxCFData()
    {
        if (m_cfdaRef)
            CFRelease(m_cfdaRef);
    }

    const UInt8* GetValue()
    { return CFDataGetBytePtr(m_cfdaRef); }

    CFIndex GetCount()
    { return CFDataGetLength(m_cfdaRef); }

    operator CFTypeRef() const
    { return (CFTypeRef)m_cfdaRef; }

    bool IsOk()
    { return m_cfdaRef != NULL; }

private:
    CFDataRef m_cfdaRef;
};

void wxCFDictionary::MakeValidXML()
{
        CFIndex cfiCount = GetCount();
        CFTypeRef* pKeys = new CFTypeRef[cfiCount];
        CFTypeRef* pValues = new CFTypeRef[cfiCount];

        CFDictionaryGetKeysAndValues(m_cfmdRef, pKeys, pValues);

        // for plist xml format, all dictionary keys must be cfstrings and
        // no values in the dictionary or subkeys/values can be NULL;
        // additionally, CFURLs are not allowed
        for (CFIndex i = 0; i < cfiCount; ++i)
        {
            // must be an array, dictionary, string, bool, or int and cannot be null
            // and dictionaries can only contain cfstring keys
            CFTypeRef cfRef = pValues[i];
            if (!pKeys[i] ||
                CFGetTypeID(pKeys[i]) != CFStringGetTypeID() ||
                !cfRef)
            {
                Remove(pKeys[i]);
                --i;
                --cfiCount;
                delete [] pKeys;
                delete [] pValues;
                pKeys = new CFTypeRef[cfiCount];
                pValues = new CFTypeRef[cfiCount];
                CFDictionaryGetKeysAndValues(m_cfmdRef, pKeys, pValues);
            }
            else if (CFGetTypeID(cfRef) == CFArrayGetTypeID())
            {
                CFRetain(cfRef);
                wxCFArray cfaCurrent(cfRef);
                cfaCurrent.MakeMutable();
                cfaCurrent.MakeValidXML();
                Set(pKeys[i], cfaCurrent);
            }
            else if (CFGetTypeID(cfRef) == CFDictionaryGetTypeID())
            {
                CFRetain(cfRef);
                wxCFDictionary cfdCurrent(cfRef);
                cfdCurrent.MakeMutable();
                cfdCurrent.MakeValidXML();
                Set(pKeys[i], cfdCurrent);
            }
            else if ( CFGetTypeID(cfRef) != CFStringGetTypeID() &&
                      CFGetTypeID(cfRef) != CFNumberGetTypeID() &&
                      CFGetTypeID(cfRef) != CFBooleanGetTypeID() )
            {
                Remove(pKeys[i]);
                --i;
                --cfiCount;
                delete [] pKeys;
                delete [] pValues;
                pKeys = new CFTypeRef[cfiCount];
                pValues = new CFTypeRef[cfiCount];
                CFDictionaryGetKeysAndValues(m_cfmdRef, pKeys, pValues);
            }
        }

        delete [] pValues;
        delete [] pKeys;
}

void wxCFArray::MakeValidXML()
{
        for (CFIndex i = 0; i < GetCount(); ++i)
        {
            //must be an array, dictionary, string, bool, or int and cannot be null
            //and dictionaries can only contain cfstring keys
            CFTypeRef cfRef = (*this)[i];
            if (!cfRef)
            {
                Remove(i);
                --i;
            }
            else if (CFGetTypeID(cfRef) == CFArrayGetTypeID())
            {
                CFRetain(cfRef);
                wxCFArray cfaCurrent(cfRef);
                cfaCurrent.MakeMutable();
                cfaCurrent.MakeValidXML();
                Set(i, cfaCurrent);
            }
            else if (CFGetTypeID(cfRef) == CFDictionaryGetTypeID())
            {
                CFRetain(cfRef);
                wxCFDictionary cfdCurrent(cfRef);
                cfdCurrent.MakeMutable();
                cfdCurrent.MakeValidXML();
                Set(i, cfdCurrent);
            }
            else if ( CFGetTypeID(cfRef) != CFStringGetTypeID() &&
                      CFGetTypeID(cfRef) != CFNumberGetTypeID() &&
                      CFGetTypeID(cfRef) != CFBooleanGetTypeID() )
            {
                Remove(i);
                --i;
            }
        }
}

//
//
//
//  END TODO
//
//
//

wxFileType* wxMimeTypesManagerImpl::Associate(const wxFileTypeInfo& ftInfo)
{
    bool bInfoSuccess = false;

    const wxArrayString& asExtensions = ftInfo.GetExtensions();
    size_t dwFoundIndex = 0;
    if (!asExtensions.GetCount())
    {
        wxLogDebug(wxT("Must have extension to associate with"));
    }

    // Find and write to Info.plist in main bundle (note that some other
    // apps have theirs named differently, i.e. IE's is named Info-macos.plist
    // some apps (non-wx) use the 'plst' resource instead
    CFBundleRef cfbMain = CFBundleGetMainBundle();
    if (cfbMain)
    {
        UInt32 dwBundleType, dwBundleCreator;
        CFBundleGetPackageInfo(cfbMain, &dwBundleType, &dwBundleCreator);

        // if launching terminal non-app, version will be 'BNDL' (generic bundle, maybe in other cases too),
        // which will give us the incorrect info.plist path
        // otherwise it will be 'APPL', or in the case of a framework, 'FMWK'
        if (dwBundleType == 'APPL')
        {
            wxCFURL cfurlBundleLoc((CFTypeRef)CFBundleCopyBundleURL(cfbMain));
//             wxCFURL cfurlBundleLoc((CFTypeRef)CFBundleCopyExecutableURL(cfbMain));
            wxString sInfoPath;
//            sInfoPath << wxT("file://");
            sInfoPath << cfurlBundleLoc.BuildWXString();
            sInfoPath << wxT("Contents/Info.plist");

//        wxCFDictionary cfdInfo( CFBundleGetInfoDictionary(cfbMain), wxCF_RETAIN );
            wxCFDictionary cfdInfo;
            bool bInfoOpenSuccess = false;
            wxFile indictfile;
            if (indictfile.Open(sInfoPath, wxFile::read))
            {
                CFIndex cfiBufLen = (CFIndex) indictfile.Length();
                const UInt8* pBuffer = new UInt8[cfiBufLen];
                indictfile.Read((void*)pBuffer, cfiBufLen);
                wxCFData cfdaInDict(pBuffer, cfiBufLen);
                wxString sError;
                bInfoOpenSuccess = cfdInfo.ReadAsXML(cfdaInDict, &sError);
                if (!bInfoOpenSuccess)
                {
                    wxLogDebug(sError);
                }
                indictfile.Close();
            }

            if (bInfoOpenSuccess)
            {
                cfdInfo.MakeMutable( cfdInfo.GetCount() + 1 );

                wxCFArray cfaDocTypes( wxCFRetain( cfdInfo[ wxCFStringRef(wxT("CFBundleDocumentTypes")) ] ) );

                bool bAddDocTypesArrayToDictionary = !cfaDocTypes.IsOk();
                if (bAddDocTypesArrayToDictionary)
                    cfaDocTypes.Create();
                else
                    cfaDocTypes.MakeMutable( cfaDocTypes.GetCount() + 1 );

                bool bEntryFound = false;

                // search for duplicates
                CFIndex i;
                for (i = 0; i < cfaDocTypes.GetCount(); ++i)
                {
                    wxCFDictionary cfdDocTypeEntry( wxCFRetain( cfaDocTypes[i] ) );

                    // A lot of apps don't support MIME types for some reason
                    // so we go by extensions only
                    wxCFArray cfaExtensions( wxCFRetain( cfdDocTypeEntry[ wxCFStringRef(wxT("CFBundleTypeExtensions")) ] ) );

                    if (!cfaExtensions.IsOk())
                        continue;

                    for (CFIndex iExt = 0; iExt < cfaExtensions.GetCount(); ++iExt)
                    {
                        for (size_t iWXExt = 0; iWXExt < asExtensions.GetCount(); ++iWXExt)
                        {
                            if (asExtensions[iWXExt] ==
                                    wxCFStringRef( wxCFRetain( (CFStringRef) cfaExtensions[iExt] ) ).AsString())
                            {
                                bEntryFound = true;
                                dwFoundIndex = iWXExt;

                                break;
                            }
                        } //end of wxstring array

                        if (bEntryFound)
                            break;
                    } //end for cf array

                    if (bEntryFound)
                        break;
                } //end for doctypes

                wxCFDictionary cfdNewEntry;

                if (!ftInfo.GetDescription().empty())
                {
                    cfdNewEntry.Add( wxCFStringRef(wxT("CFBundleTypeName")),
                                wxCFStringRef(ftInfo.GetDescription()) );
                }

                if (!ftInfo.GetIconFile().empty())
                {
                    cfdNewEntry.Add( wxCFStringRef(wxT("CFBundleTypeIconFile")),
                                    wxCFStringRef(ftInfo.GetIconFile()) );
                }

                wxCFArray cfaOSTypes;
                wxCFArray cfaExtensions;
                wxCFArray cfaMimeTypes;

                //OSTypes is a cfarray of four-char-codes - '****' for unrestricted
                cfaOSTypes.Add( wxCFStringRef(wxT("****")) );
                cfdNewEntry.Add( wxCFStringRef(wxT("CFBundleTypeOSTypes")), cfaOSTypes );

                //'*' for unrestricted
                if (ftInfo.GetExtensionsCount() != 0)
                {
                    for (size_t iExtension = 0; iExtension < ftInfo.GetExtensionsCount(); ++iExtension)
                    {
                        cfaExtensions.Add( wxCFStringRef( asExtensions[iExtension] ) );
                    }

                    cfdNewEntry.Add( wxCFStringRef(wxT("CFBundleTypeExtensions")), cfaExtensions );
                }

                if (!ftInfo.GetMimeType().empty())
                {
                    cfaMimeTypes.Add( wxCFStringRef(ftInfo.GetMimeType()) );
                    cfdNewEntry.Add( wxCFStringRef(wxT("CFBundleTypeMIMETypes")), cfaMimeTypes );
                }

                // Editor - can perform all actions
                // Viewer - all actions except manipulation/saving
                // None - can perform no actions
                cfdNewEntry.Add( wxCFStringRef(wxT("CFBundleTypeRole")), wxCFStringRef(wxT("Editor")) );

                // Is application bundled?
                cfdNewEntry.Add( wxCFStringRef(wxT("LSTypeIsPackage")), kCFBooleanTrue );

                if (bEntryFound)
                    cfaDocTypes.Set(i, cfdNewEntry);
                else
                    cfaDocTypes.Add(cfdNewEntry);

                // set the doc types array in the muted dictionary
                if (bAddDocTypesArrayToDictionary)
                    cfdInfo.Add(wxCFStringRef(wxT("CFBundleDocumentTypes")), cfaDocTypes);
                else
                    cfdInfo.Set(wxCFStringRef(wxT("CFBundleDocumentTypes")), cfaDocTypes);

                cfdInfo.MakeValidXML();

                wxFile outdictfile;
                if (outdictfile.Open(sInfoPath, wxFile::write))
                {
                    wxCFData cfdaInfo(cfdInfo.WriteAsXML());
                    if (cfdaInfo.IsOk())
                    {
                        if (outdictfile.Write(cfdaInfo.GetValue(), cfdaInfo.GetCount()) !=
                            (wxFileOffset)cfdaInfo.GetCount())
                        {
                            wxLogDebug(wxT("error in writing to file"));
                        }
                        else
                        {
                            bInfoSuccess = true;

//#if defined(__DARWIN__)
//                //force launch services to update its database for the finder
//                OSStatus status = LSRegisterURL((CFURLRef)(CFTypeRef)cfurlBundleLoc, true);
//                if (status != noErr)
//                {
//                    wxLogDebug(wxT("LSRegisterURL Failed."));
//                }
//#endif
                        }
                        outdictfile.Close();
                    }
                    else
                    {
                        outdictfile.Close();
                        wxLogDebug(wxT("Could not read in new dictionary"));
                    }
                }
                else
                {
                    wxLogDebug(wxString(wxT("Could not open [")) +
                    sInfoPath + wxT("] for writing."));
                }
            }
            else
            {
                wxLogDebug(wxT("No info dictionary in main bundle"));
            }
        }
        else
        {
            wxLogDebug(wxT("Can only call associate from bundled app within XXX.app"));
        }
    }
    else
    {
        wxLogDebug(wxT("No main bundle"));
    }

    if (!bInfoSuccess)
        return NULL;

    // on mac you have to embed it into the mac's file reference resource ('FREF' I believe)
    // or, alternately, you could just add an entry to m_hDatabase, but you'd need to get
    // the app's signature somehow...

    OSType processType, creator;
    OSStatus status = MoreProcGetProcessTypeSignature(NULL, &processType, &creator);

    if (status == noErr)
    {
        Str255 psCreatorName;
#ifndef __LP64__
        FSSpec dummySpec;
        status = FindApplication(creator, false, psCreatorName, &dummySpec);
#else
        FSRef fsref;
        status = LSFindApplicationForInfo( creator, NULL, NULL, &fsref ,NULL);
        HFSUniStr255 name;
        status = FSGetCatalogInfo(&fsref, kFSCatInfoNone, NULL, &name, NULL, NULL);
        CFStringRef str = FSCreateStringFromHFSUniStr( 0 , &name );
        CFStringGetPascalString(str, psCreatorName, 256, CFStringGetSystemEncoding());
        CFRelease( str );
#endif

        if (status == noErr)
        {
            //get the file type if it exists -
            //if it really does then modify the database then save it,
            //otherwise we need to create a whole new entry
            wxFileType* pFileType = GetFileTypeFromExtension(asExtensions[dwFoundIndex]);
            if (pFileType)
            {
                ICMapEntry entry;
                ICGetMapEntry( (ICInstance) m_hIC, (Handle) m_hDatabase,
                    pFileType->m_impl->m_lIndex, &entry );

                memcpy(entry.creatorAppName, psCreatorName, sizeof(Str255));
                entry.fileCreator = creator;

                status = ICSetMapEntry( (ICInstance) m_hIC, (Handle) m_hDatabase,
                    pFileType->m_impl->m_lIndex, &entry );

                //success
                if (status == noErr)
                {
                     return pFileType;

                    //kICAttrNoChange means we don't care about attributes such as
                    //locking in the database
//                    status = ICSetPrefHandle((ICInstance) m_hIC, kICMapping,
//                                             kICAttrNoChange, (Handle) m_hDatabase);
//                    if (status == noErr)
//                        return pFileType;
//                    else
//                    {
//                        wxLogDebug(wxString::Format(wxT("%i - %s"), (int)status, wxT("ICSetPrefHandle failed.")));
//                    }
                }
                else
                {
                    wxLogDebug(wxString::Format(wxT("%i - %s"), __LINE__, wxT("ICSetMapEntry failed.")));
                }

                // failure - cleanup
                delete pFileType;
            }
            else
            {
                // TODO: Maybe force all 3 of these to be non-empty?
                Str255 psExtension, psMimeType, psDescription;

                wxMacStringToPascal(wxString(wxT(".")) + ftInfo.GetExtensions()[0], psExtension);
                wxMacStringToPascal(ftInfo.GetMimeType(), psMimeType);
                wxMacStringToPascal(ftInfo.GetDescription(), psDescription);

                Str255 psPostCreatorName;
                wxMacStringToPascal(wxEmptyString, psPostCreatorName);

                //add the entry to the database
                ICMapEntry entry;
                entry.totalLength = sizeof(ICMapEntry);
                entry.fixedLength = kICMapFixedLength;
                entry.version = 0;
                entry.fileType = 0; //TODO:  File type?
                entry.fileCreator = creator;
                entry.postCreator = 0;
                entry.flags = kICMapDataForkBit; //TODO:  Maybe resource is valid by default too?
                PLstrcpy( entry.extension , psExtension ) ;
                memcpy(entry.creatorAppName, psCreatorName, sizeof(Str255));
                memcpy(entry.postAppName, psPostCreatorName, sizeof(Str255));
                memcpy(entry.MIMEType, psMimeType, sizeof(Str255));
                memcpy(entry.entryName, psDescription, sizeof(Str255));

                status = ICAddMapEntry( (ICInstance) m_hIC, (Handle) m_hDatabase, &entry);
                if (status == noErr)
                {
                    return GetFileTypeFromExtension(ftInfo.GetMimeType());

//                    kICAttrNoChange means we don't care about attributes such as
//                    locking in the database
//                    status = ICSetPrefHandle((ICInstance) m_hIC, kICMapping,
//                                             kICAttrNoChange, (Handle) m_hDatabase);

                    // return the entry in the database if successful
//                    if (status == noErr)
//                        return GetFileTypeFromExtension(ftInfo.GetMimeType());
//                    else
//                    {
//                        wxLogDebug(wxString::Format(wxT("%i - %s"), __LINE__, wxT("ICSetPrefHandle failed.")));
 //                   }
                }
                else
                {
                    wxLogDebug(wxString::Format(wxT("%i - %s"), __LINE__, wxT("ICAppMapEntry failed.")));
                }
            }
        } // end if FindApplcation was successful
        else
        {
            wxLogDebug(wxString::Format(wxT("%i - %s"), __LINE__, wxT("FindApplication failed.")));
        }
    } // end if it could obtain app's signature
    else
    {
        wxLogDebug(wxString::Format(wxT("%i - %s"), __LINE__, wxT("GetProcessSignature failed.")));
    }

    return NULL;
}

bool
wxMimeTypesManagerImpl::Unassociate(wxFileType *pFileType)
{
    wxASSERT(pFileType);
    bool bInfoSuccess = false;

    wxArrayString asExtensions;
    pFileType->GetExtensions(asExtensions);

    if (!asExtensions.GetCount())
    {
        wxLogDebug(wxT("Must have extension to disassociate"));
        return false;
    }

    // Find and write to Info.plist in main bundle (note that some other
    // apps have theirs named differently, i.e. IE's is named Info-macos.plist
    // some apps (non-wx) use the 'plst' resource instead
    CFBundleRef cfbMain = CFBundleGetMainBundle();
    if (cfbMain)
    {
        UInt32 dwBundleType, dwBundleCreator;
        CFBundleGetPackageInfo(cfbMain, &dwBundleType, &dwBundleCreator);

        // if launching terminal non-app, version will be 'BNDL' (generic bundle, maybe in other cases too),
        // which will give us the incorrect info.plist path
        // otherwise it will be 'APPL', or in the case of a framework, 'FMWK'
        if (dwBundleType == 'APPL')
        {

            wxCFURL cfurlBundleLoc((CFTypeRef)CFBundleCopyBundleURL(cfbMain));
//             wxCFURL cfurlBundleLoc((CFTypeRef)CFBundleCopyExecutableURL(cfbMain));
            wxString sInfoPath;
//            sInfoPath << wxT("file://");
            sInfoPath << cfurlBundleLoc.BuildWXString();
            sInfoPath << wxT("Contents/Info.plist");

//        wxCFDictionary cfdInfo( (CFTypeRef) CFBundleGetInfoDictionary(cfbMain), wxCF_RETAIN );
            wxCFDictionary cfdInfo;
            bool bInfoOpenSuccess = false;
            wxFile indictfile;
            if (indictfile.Open(sInfoPath, wxFile::read))
            {
                CFIndex cfiBufLen = (CFIndex) indictfile.Length();
                const UInt8* pBuffer = new UInt8[cfiBufLen];
                indictfile.Read((void*)pBuffer, cfiBufLen);
                wxCFData cfdaInDict(pBuffer, cfiBufLen);
                wxString sError;
                bInfoOpenSuccess = cfdInfo.ReadAsXML(cfdaInDict, &sError);
                if (!bInfoOpenSuccess)
                {
                    wxLogDebug(sError);
                }
                indictfile.Close();
            }

            if (bInfoOpenSuccess)
            {
                cfdInfo.MakeMutable( cfdInfo.GetCount() + 1 );

                wxCFArray cfaDocTypes( wxCFRetain( cfdInfo[ wxCFStringRef(wxT("CFBundleDocumentTypes")) ] ) );

                if (cfaDocTypes.IsOk())
                {
                    bool bEntryFound = false;

                    //search for duplicate
                    CFIndex i;
                    for (i = 0; i < cfaDocTypes.GetCount(); ++i)
                    {
                        wxCFDictionary cfdDocTypeEntry( wxCFRetain( cfaDocTypes[i] ) );

                        //A lot of apps dont do to mime types for some reason
                        //so we go by extensions only
                        wxCFArray cfaExtensions( wxCFRetain( cfdDocTypeEntry[ wxCFStringRef(wxT("CFBundleTypeExtensions")) ]) );

                        if (!cfaExtensions.IsOk())
                            continue;

                        for (CFIndex iExt = 0; iExt < cfaExtensions.GetCount(); ++iExt)
                        {
                            for (size_t iWXExt = 0; iWXExt < asExtensions.GetCount(); ++iWXExt)
                            {
                                if (asExtensions[iWXExt] ==
                                    wxCFStringRef( wxCFRetain( (CFStringRef) cfaExtensions[iExt] ) ).AsString())
                                {
                                    bEntryFound = true;
                                    cfaDocTypes.Remove(i);
                                    cfdInfo.Set( wxCFStringRef(wxT("CFBundleDocumentTypes")) , cfaDocTypes );
                                    break;
                                }
                            } //end of wxstring array

                            if (bEntryFound)
                                break;
                        } //end for cf array

                        if (bEntryFound)
                            break;
                    }//end for doctypes

                    if (bEntryFound)
                    {
                        cfdInfo.MakeValidXML();

                        wxFile outdictfile;
                        if (outdictfile.Open(sInfoPath, wxFile::write))
                        {
                            wxCFData cfdaInfo(cfdInfo.WriteAsXML());
                            if (cfdaInfo.IsOk())
                            {
                                if (outdictfile.Write(cfdaInfo.GetValue(), cfdaInfo.GetCount()) !=
                                    (wxFileOffset)cfdaInfo.GetCount())
                                {
                                    wxLogDebug(wxT("error in writing to file"));
                                }
                                else
                                {
                                    bInfoSuccess = true;

//#if defined(__DARWIN__)
//                //force launch services to update its database for the finder
//                OSStatus status = LSRegisterURL((CFURLRef)(CFTypeRef)cfurlBundleLoc, true);
//                if (status != noErr)
//                {
//                    wxLogDebug(wxT("LSRegisterURL Failed."));
//                }
//#endif
                                }
                                outdictfile.Close();
                            }
                            else
                            {
                                outdictfile.Close();
                                wxLogDebug(wxT("Could not read in new dictionary"));
                            }
                        }
                        else
                        {
                            wxLogDebug(
                                wxString(wxT("Could not open [")) +
                                sInfoPath + wxT("] for writing."));
                        }
                    }
                    else
                    {
                        wxLogDebug(wxT("Entry not found to remove"));

                        wxString sPrintOut;
                        wxCFDictionary::PrintOutArray(sPrintOut, (CFArrayRef)(CFTypeRef)cfaDocTypes);
                        wxLogDebug(sPrintOut);

                        for (size_t i = 0; i < asExtensions.GetCount(); ++i)
                        {
                            wxLogDebug(asExtensions[i]);
                        }
                    }
                }
                else
                {
                    wxLogDebug(wxT("No doc types array found"));
                    wxString sPrintOut;  cfdInfo.PrintOut(sPrintOut);  wxLogDebug(sPrintOut);
                }
            }
            else
            {
                wxLogDebug(wxT("No info dictionary in main bundle"));
            }
        }
        else
        {
            wxLogDebug(wxT("Can only call associate from bundled app within XXX.app"));
        }
    }
    else
    {
        wxLogDebug(wxT("No main bundle"));
    }

    if (!bInfoSuccess)
        return false;

    // this should be as easy as removing the entry from the database
    // and then saving the database
    OSStatus status = ICDeleteMapEntry( (ICInstance) m_hIC, (Handle) m_hDatabase,
                            pFileType->m_impl->m_lIndex);

    if (status == noErr)
    {
            return true;

        //kICAttrNoChange means we don't care about attributes such as
        //locking in the database
//        status = ICSetPrefHandle((ICInstance) m_hIC, kICMapping,
//                    kICAttrNoChange, (Handle) m_hDatabase);

//        if (status == noErr)
//        {
//            return true;
//        }
//        else
//        {
//            wxLogDebug(wxString::Format(wxT("%i - %s"), __LINE__, wxT("ICSetPrefHandle failed.")));
//        }
    }
    else
    {
        wxLogDebug(wxString::Format(wxT("%i - %s"), __LINE__, wxT("ICDeleteMapEntry failed.")));
    }

    return false;
}

#if 0
    CFWriteStreamRef cfwsInfo = CFWriteStreamCreateWithFile(
           kCFAllocatorDefault,
           (CFURLRef) (CFTypeRef)cfurlInfoLoc );

//     CFShow(cfdInfo);
        if (cfwsInfo)
        {
            Boolean bOpened = CFWriteStreamOpen(cfwsInfo);
            if (bOpened)
            {
                CFStringRef cfsError;
                CFIndex cfiWritten = CFPropertyListWriteToStream((CFPropertyListRef)(CFTypeRef)cfdInfo,
                                    cfwsInfo,
                                    kCFPropertyListXMLFormat_v1_0, //100
                                    &cfsError);
                if (cfsError && cfiWritten == 0)
                {
                    wxLogDebug(wxCFStringRef(cfsError).BuildWXString());
                    wxString sMessage;
                    cfdInfo.PrintOut(sMessage);
                    wxLogDebug(sMessage);
                }
                else
                {
                    bInfoSuccess = true;
//#if defined(__DARWIN__)
//                //force launch services to update its database for the finder
//                OSStatus status = LSRegisterURL((CFURLRef)(CFTypeRef)cfurlBundleLoc, true);
//                if (status != noErr)
//                {
//                    wxLogDebug(wxT("LSRegisterURL Failed."));
//                }
//#endif
                }

                CFWriteStreamClose(cfwsInfo);
#endif

#endif //wxUSE_MIMETYPE
