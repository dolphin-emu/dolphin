/////////////////////////////////////////////////////////////////////////////
// Name:        src/osx/core/hid.cpp
// Purpose:     DARWIN HID layer for WX Implementation
// Author:      Ryan Norton
// Modified by:
// Created:     11/11/2003
// Copyright:   (c) Ryan Norton
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

// ===========================================================================
// declarations
// ===========================================================================

// ---------------------------------------------------------------------------
// headers
// ---------------------------------------------------------------------------

// For compilers that support precompilation, includes "wx.h".
#include "wx/wxprec.h"

#ifdef __BORLANDC__
    #pragma hdrstop
#endif

#if wxOSX_USE_COCOA_OR_CARBON

#include "wx/osx/core/hid.h"

#ifndef WX_PRECOMP
    #include "wx/dynarray.h"
    #include "wx/string.h"
    #include "wx/log.h"
    #include "wx/utils.h"
    #include "wx/module.h"
#endif

#include "wx/osx/private.h"

// ============================================================================
// implementation
// ============================================================================

// ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//
// wxHIDDevice
//
// ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

// ----------------------------------------------------------------------------
// wxHIDDevice::Create
//
//  nClass is the HID Page such as
//      kHIDPage_GenericDesktop
//  nType is the HID Usage such as
//      kHIDUsage_GD_Joystick,kHIDUsage_GD_Mouse,kHIDUsage_GD_Keyboard
//  nDev is the device number to use
//
// ----------------------------------------------------------------------------
bool wxHIDDevice::Create (int nClass, int nType, int nDev)
{
    //Create the mach port
    if(IOMasterPort(bootstrap_port, &m_pPort) != kIOReturnSuccess)
    {
        wxLogSysError(wxT("Could not create mach port"));
        return false;
    }

    //Dictionary that will hold first
    //the matching dictionary for determining which kind of devices we want,
    //then later some registry properties from an iterator (see below)
    //
    //The call to IOServiceMatching filters down the
    //the services we want to hid services (and also eats the
    //dictionary up for us (consumes one reference))
    CFMutableDictionaryRef pDictionary = IOServiceMatching(kIOHIDDeviceKey);
    if(pDictionary == NULL)
    {
        wxLogSysError( wxT("IOServiceMatching(kIOHIDDeviceKey) failed") );
        return false;
    }

    //Here we'll filter down the services to what we want
    if (nType != -1)
    {
        CFNumberRef pType = CFNumberCreate(kCFAllocatorDefault,
                                    kCFNumberIntType, &nType);
        CFDictionarySetValue(pDictionary, CFSTR(kIOHIDPrimaryUsageKey), pType);
        CFRelease(pType);
    }
    if (nClass != -1)
    {
        CFNumberRef pClass = CFNumberCreate(kCFAllocatorDefault,
                                    kCFNumberIntType, &nClass);
        CFDictionarySetValue(pDictionary, CFSTR(kIOHIDPrimaryUsagePageKey), pClass);
        CFRelease(pClass);
    }

    //Now get the matching services
    io_iterator_t pIterator;
    if( IOServiceGetMatchingServices(m_pPort,
                        pDictionary, &pIterator) != kIOReturnSuccess )
    {
        wxLogSysError(wxT("No Matching HID Services"));
        return false;
    }

    //Were there any devices matched?
    if(pIterator == 0)
        return false; // No devices found

    //Now we iterate through them
    io_object_t pObject;
    while ( (pObject = IOIteratorNext(pIterator)) != 0)
    {
        if(--nDev != 0)
        {
            IOObjectRelease(pObject);
            continue;
        }

        if ( IORegistryEntryCreateCFProperties
             (
                pObject,
                &pDictionary,
                kCFAllocatorDefault,
                kNilOptions
             ) != KERN_SUCCESS )
        {
            wxLogDebug(wxT("IORegistryEntryCreateCFProperties failed"));
        }

        //
        // Now we get the attributes of each "product" in the iterator
        //

        //Get [product] name
        CFStringRef cfsProduct = (CFStringRef)
            CFDictionaryGetValue(pDictionary, CFSTR(kIOHIDProductKey));
        m_szProductName =
            wxCFStringRef( wxCFRetain(cfsProduct)
                               ).AsString();

        //Get the Product ID Key
        CFNumberRef cfnProductId = (CFNumberRef)
            CFDictionaryGetValue(pDictionary, CFSTR(kIOHIDProductIDKey));
        if (cfnProductId)
        {
            CFNumberGetValue(cfnProductId, kCFNumberIntType, &m_nProductId);
        }

        //Get the Vendor ID Key
        CFNumberRef cfnVendorId = (CFNumberRef)
            CFDictionaryGetValue(pDictionary, CFSTR(kIOHIDVendorIDKey));
        if (cfnVendorId)
        {
            CFNumberGetValue(cfnVendorId, kCFNumberIntType, &m_nManufacturerId);
        }

        //
        // End attribute getting
        //

        //Create the interface (good grief - long function names!)
        SInt32 nScore;
        IOCFPlugInInterface** ppPlugin;
        if(IOCreatePlugInInterfaceForService(pObject,
                                             kIOHIDDeviceUserClientTypeID,
                                             kIOCFPlugInInterfaceID, &ppPlugin,
                                             &nScore) !=  kIOReturnSuccess)
        {
            wxLogSysError(wxT("Could not create HID Interface for product"));
            return false;
        }

        //Now, the final thing we can check before we fall back to asserts
        //(because the dtor only checks if the device is ok, so if anything
        //fails from now on the dtor will delete the device anyway, so we can't break from this).

        //Get the HID interface from the plugin to the mach port
        if((*ppPlugin)->QueryInterface(ppPlugin,
                               CFUUIDGetUUIDBytes(kIOHIDDeviceInterfaceID),
                               (void**) &m_ppDevice) != S_OK)
        {
            wxLogSysError(wxT("Could not get device interface from HID interface"));
            return false;
        }

        //release the plugin
        (*ppPlugin)->Release(ppPlugin);

        //open the HID interface...
        if ( (*m_ppDevice)->open(m_ppDevice, 0) != S_OK )
        {
            wxLogDebug(wxT("HID device: open failed"));
        }

        //
        //Now the hard part - in order to scan things we need "cookies"
        //
        CFArrayRef cfaCookies = (CFArrayRef)CFDictionaryGetValue(pDictionary,
                                 CFSTR(kIOHIDElementKey));
        BuildCookies(cfaCookies);

        //cleanup
        CFRelease(pDictionary);
        IOObjectRelease(pObject);

        //iterator cleanup
        IOObjectRelease(pIterator);

        return true;
    }

    //iterator cleanup
    IOObjectRelease(pIterator);

    return false; //no device
}//end Create()

// ----------------------------------------------------------------------------
// wxHIDDevice::GetCount [static]
//
//  Obtains the number of devices on a system for a given HID Page (nClass)
// and HID Usage (nType).
// ----------------------------------------------------------------------------
size_t wxHIDDevice::GetCount (int nClass, int nType)
{
    //Create the mach port
    mach_port_t             pPort;
    if(IOMasterPort(bootstrap_port, &pPort) != kIOReturnSuccess)
    {
        wxLogSysError(wxT("Could not create mach port"));
        return false;
    }

    //Dictionary that will hold first
    //the matching dictionary for determining which kind of devices we want,
    //then later some registry properties from an iterator (see below)
    CFMutableDictionaryRef pDictionary = IOServiceMatching(kIOHIDDeviceKey);
    if(pDictionary == NULL)
    {
        wxLogSysError( wxT("IOServiceMatching(kIOHIDDeviceKey) failed") );
        return false;
    }

    //Here we'll filter down the services to what we want
    if (nType != -1)
    {
        CFNumberRef pType = CFNumberCreate(kCFAllocatorDefault,
                                    kCFNumberIntType, &nType);
        CFDictionarySetValue(pDictionary, CFSTR(kIOHIDPrimaryUsageKey), pType);
        CFRelease(pType);
    }
    if (nClass != -1)
    {
        CFNumberRef pClass = CFNumberCreate(kCFAllocatorDefault,
                                    kCFNumberIntType, &nClass);
        CFDictionarySetValue(pDictionary, CFSTR(kIOHIDPrimaryUsagePageKey), pClass);
        CFRelease(pClass);
    }

    //Now get the matching services
    io_iterator_t pIterator;
    if( IOServiceGetMatchingServices(pPort,
                                     pDictionary, &pIterator) != kIOReturnSuccess )
    {
        wxLogSysError(wxT("No Matching HID Services"));
        return false;
    }

    //If the iterator doesn't exist there are no devices :)
    if ( !pIterator )
        return 0;

    //Now we iterate through them
    size_t nCount = 0;
    io_object_t pObject;
    while ( (pObject = IOIteratorNext(pIterator)) != 0)
    {
        ++nCount;
        IOObjectRelease(pObject);
    }

    //cleanup
    IOObjectRelease(pIterator);
    mach_port_deallocate(mach_task_self(), pPort);

    return nCount;
}//end Create()

// ----------------------------------------------------------------------------
// wxHIDDevice::AddCookie
//
// Adds a cookie to the internal cookie array from a CFType
// ----------------------------------------------------------------------------
void wxHIDDevice::AddCookie(CFTypeRef Data, int i)
{
    CFNumberGetValue(
                (CFNumberRef) CFDictionaryGetValue    ( (CFDictionaryRef) Data
                                        , CFSTR(kIOHIDElementCookieKey)
                                        ),
                kCFNumberIntType,
                &m_pCookies[i]
                );
}

// ----------------------------------------------------------------------------
// wxHIDDevice::AddCookieInQueue
//
// Adds a cookie to the internal cookie array from a CFType and additionally
// adds it to the internal HID Queue
// ----------------------------------------------------------------------------
void wxHIDDevice::AddCookieInQueue(CFTypeRef Data, int i)
{
    //3rd Param flags (none yet)
    AddCookie(Data, i);
    if ( (*m_ppQueue)->addElement(m_ppQueue, m_pCookies[i], 0) != S_OK )
    {
        wxLogDebug(wxT("HID device: adding element failed"));
    }
}

// ----------------------------------------------------------------------------
// wxHIDDevice::InitCookies
//
// Create the internal cookie array, optionally creating a HID Queue
// ----------------------------------------------------------------------------
void wxHIDDevice::InitCookies(size_t dwSize, bool bQueue)
{
    m_pCookies = new IOHIDElementCookie[dwSize];
    if (bQueue)
    {
        wxASSERT( m_ppQueue == NULL);
        m_ppQueue = (*m_ppDevice)->allocQueue(m_ppDevice);
        if ( !m_ppQueue )
        {
            wxLogDebug(wxT("HID device: allocQueue failed"));
            return;
        }

        //Param 2, flags, none yet
        if ( (*m_ppQueue)->create(m_ppQueue, 0, 512) != S_OK )
        {
            wxLogDebug(wxT("HID device: create failed"));
        }
    }

    //make sure that cookie array is clear
    memset(m_pCookies, 0, sizeof(*m_pCookies) * dwSize);
}

// ----------------------------------------------------------------------------
// wxHIDDevice::IsActive
//
// Returns true if a cookie of the device is active - for example if a key is
// held down, joystick button pressed, caps lock active, etc..
// ----------------------------------------------------------------------------
bool wxHIDDevice::IsActive(int nIndex)
{
    if(!HasElement(nIndex))
    {
        //cookie at index does not exist - getElementValue
        //could return true which would be incorrect so we
        //check here
        return false;
    }

    IOHIDEventStruct Event;
    (*m_ppDevice)->getElementValue(m_ppDevice, m_pCookies[nIndex], &Event);
    return !!Event.value;
}

// ----------------------------------------------------------------------------
// wxHIDDevice::HasElement
//
// Returns true if the element in the internal cookie array exists
// ----------------------------------------------------------------------------
bool wxHIDDevice::HasElement(int nIndex)
{
    return m_pCookies[nIndex] != 0;
}

// ----------------------------------------------------------------------------
// wxHIDDevice Destructor
//
// Frees all memory and objects from the structure
// ----------------------------------------------------------------------------
wxHIDDevice::~wxHIDDevice()
{
    if (m_ppDevice != NULL)
    {
        if (m_ppQueue != NULL)
        {
            (*m_ppQueue)->stop(m_ppQueue);
            (*m_ppQueue)->dispose(m_ppQueue);
            (*m_ppQueue)->Release(m_ppQueue);
        }
        (*m_ppDevice)->close(m_ppDevice);
        (*m_ppDevice)->Release(m_ppDevice);
        mach_port_deallocate(mach_task_self(), m_pPort);
    }

    if (m_pCookies != NULL)
    {
        delete [] m_pCookies;
    }
}

// ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//
// wxHIDKeyboard
//
// ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

//There are no right shift, alt etc. in the wx headers yet so just sort
//of "define our own" for now
enum
{
    WXK_RSHIFT = 400,
    WXK_RALT,
    WXK_RCONTROL,
    WXK_RAW_RCONTROL,
};

// ----------------------------------------------------------------------------
// wxHIDKeyboard::GetCount [static]
//
// Get number of HID keyboards available
// ----------------------------------------------------------------------------
int wxHIDKeyboard::GetCount()
{
    return wxHIDDevice::GetCount(kHIDPage_GenericDesktop,
                               kHIDUsage_GD_Keyboard);
}

// ----------------------------------------------------------------------------
// wxHIDKeyboard::Create
//
// Create the HID Keyboard
// ----------------------------------------------------------------------------
bool wxHIDKeyboard::Create(int nDev /* = 1*/)
{
    return wxHIDDevice::Create(kHIDPage_GenericDesktop,
                               kHIDUsage_GD_Keyboard,
                               nDev);
}

// ----------------------------------------------------------------------------
// wxHIDKeyboard::AddCookie
//
// Overloaded version of wxHIDDevice::AddCookie that simply does not
// add a cookie if a duplicate is found
// ----------------------------------------------------------------------------
void wxHIDKeyboard::AddCookie(CFTypeRef Data, int i)
{
    if(!HasElement(i))
        wxHIDDevice::AddCookie(Data, i);
}

// ----------------------------------------------------------------------------
// wxHIDKeyboard::BuildCookies
//
// Callback from Create() to build the HID cookies for the internal cookie
// array
// ----------------------------------------------------------------------------
void wxHIDKeyboard::BuildCookies(CFArrayRef Array)
{
    //Create internal cookie array
    InitCookies(500);

    //Begin recursing in array
    DoBuildCookies(Array);
}

void wxHIDKeyboard::DoBuildCookies(CFArrayRef Array)
{
    //Now go through each possible cookie
    int i;
    long nUsage;
//    bool bEOTriggered = false;
    for (i = 0; i < CFArrayGetCount(Array); ++i)
    {
        const void* ref = CFDictionaryGetValue(
                (CFDictionaryRef)CFArrayGetValueAtIndex(Array, i),
                CFSTR(kIOHIDElementKey)
                                              );

        if (ref != NULL)
        {
            DoBuildCookies((CFArrayRef) ref);
        }
        else
    {

            //
            // Get the usage #
            //
        CFNumberGetValue(
                (CFNumberRef)
                    CFDictionaryGetValue((CFDictionaryRef)
                        CFArrayGetValueAtIndex(Array, i),
                        CFSTR(kIOHIDElementUsageKey)
                                        ),
                              kCFNumberLongType,
                              &nUsage);

            //
            // Now translate the usage # into a wx keycode
            //

        //
        // OK, this is strange - basically this kind of strange -
        // Starting from 0xEO these elements (like shift) appear twice in
        // the array!  The ones at the end are bogus I guess - the funny part
        // is that besides the fact that the ones at the front have a Unit
        // and UnitExponent key with a value of 0 and a different cookie value,
        // there is no discernable difference between the two...
        //
        // Will the real shift please stand up?
        //
        // Something to spend a support request on, if I had one, LOL.
        //
        //if(nUsage == 0xE0)
        //{
        //    if(bEOTriggered)
        //       break;
        //    bEOTriggered = true;
        //}
        //Instead of that though we now just don't add duplicate keys

        if (nUsage >= kHIDUsage_KeyboardA && nUsage <= kHIDUsage_KeyboardZ)
            AddCookie(CFArrayGetValueAtIndex(Array, i), 'A' + (nUsage - kHIDUsage_KeyboardA) );
        else if (nUsage >= kHIDUsage_Keyboard1 && nUsage <= kHIDUsage_Keyboard9)
            AddCookie(CFArrayGetValueAtIndex(Array, i), '1' + (nUsage - kHIDUsage_Keyboard1) );
        else if (nUsage >= kHIDUsage_KeyboardF1 && nUsage <= kHIDUsage_KeyboardF12)
            AddCookie(CFArrayGetValueAtIndex(Array, i), WXK_F1 + (nUsage - kHIDUsage_KeyboardF1) );
        else if (nUsage >= kHIDUsage_KeyboardF13 && nUsage <= kHIDUsage_KeyboardF24)
            AddCookie(CFArrayGetValueAtIndex(Array, i), WXK_F13 + (nUsage - kHIDUsage_KeyboardF13) );
        else if (nUsage >= kHIDUsage_Keypad1 && nUsage <= kHIDUsage_Keypad9)
            AddCookie(CFArrayGetValueAtIndex(Array, i), WXK_NUMPAD1 + (nUsage - kHIDUsage_Keypad1) );
        else switch (nUsage)
        {
            //0's (wx & ascii go 0-9, but HID goes 1-0)
            case kHIDUsage_Keyboard0:
                AddCookie(CFArrayGetValueAtIndex(Array, i), '0');
                break;
            case kHIDUsage_Keypad0:
                AddCookie(CFArrayGetValueAtIndex(Array, i), WXK_NUMPAD0);
                break;

            //Basic
            case kHIDUsage_KeyboardReturnOrEnter:
                AddCookie(CFArrayGetValueAtIndex(Array, i), WXK_RETURN);
                break;
            case kHIDUsage_KeyboardEscape:
                AddCookie(CFArrayGetValueAtIndex(Array, i), WXK_ESCAPE);
                break;
            case kHIDUsage_KeyboardDeleteOrBackspace:
                AddCookie(CFArrayGetValueAtIndex(Array, i), WXK_BACK);
                break;
            case kHIDUsage_KeyboardTab:
                AddCookie(CFArrayGetValueAtIndex(Array, i), WXK_TAB);
                break;
            case kHIDUsage_KeyboardSpacebar:
                AddCookie(CFArrayGetValueAtIndex(Array, i), WXK_SPACE);
                break;
            case kHIDUsage_KeyboardPageUp:
                AddCookie(CFArrayGetValueAtIndex(Array, i), WXK_PAGEUP);
                break;
            case kHIDUsage_KeyboardEnd:
                AddCookie(CFArrayGetValueAtIndex(Array, i), WXK_END);
                break;
            case kHIDUsage_KeyboardPageDown:
                AddCookie(CFArrayGetValueAtIndex(Array, i), WXK_PAGEDOWN);
                break;
            case kHIDUsage_KeyboardRightArrow:
                AddCookie(CFArrayGetValueAtIndex(Array, i), WXK_RIGHT);
                break;
            case kHIDUsage_KeyboardLeftArrow:
                AddCookie(CFArrayGetValueAtIndex(Array, i), WXK_LEFT);
                break;
            case kHIDUsage_KeyboardDownArrow:
                AddCookie(CFArrayGetValueAtIndex(Array, i), WXK_DOWN);
                break;
            case kHIDUsage_KeyboardUpArrow:
                AddCookie(CFArrayGetValueAtIndex(Array, i), WXK_UP);
                break;

            //LEDS
            case kHIDUsage_KeyboardCapsLock:
                AddCookie(CFArrayGetValueAtIndex(Array, i),WXK_CAPITAL);
                break;
            case kHIDUsage_KeypadNumLock:
                AddCookie(CFArrayGetValueAtIndex(Array, i),WXK_NUMLOCK);
                break;
            case kHIDUsage_KeyboardScrollLock:
                AddCookie(CFArrayGetValueAtIndex(Array, i),WXK_SCROLL);
                break;

            //Menu keys, Shift, other specials
            case kHIDUsage_KeyboardLeftControl:
                AddCookie(CFArrayGetValueAtIndex(Array, i),WXK_RAW_CONTROL);
                break;
            case kHIDUsage_KeyboardLeftShift:
                AddCookie(CFArrayGetValueAtIndex(Array, i),WXK_SHIFT);
                break;
            case kHIDUsage_KeyboardLeftAlt:
                AddCookie(CFArrayGetValueAtIndex(Array, i),WXK_ALT);
                break;
            case kHIDUsage_KeyboardLeftGUI:
                AddCookie(CFArrayGetValueAtIndex(Array, i),WXK_CONTROL);
                break;
            case kHIDUsage_KeyboardRightControl:
                AddCookie(CFArrayGetValueAtIndex(Array, i),WXK_RAW_RCONTROL);
                break;
            case kHIDUsage_KeyboardRightShift:
                AddCookie(CFArrayGetValueAtIndex(Array, i),WXK_RSHIFT);
                break;
            case kHIDUsage_KeyboardRightAlt:
                AddCookie(CFArrayGetValueAtIndex(Array, i),WXK_RALT);
                break;
            case kHIDUsage_KeyboardRightGUI:
                AddCookie(CFArrayGetValueAtIndex(Array, i),WXK_RCONTROL);
                break;

            //Default
            default:
            //not in wx keycodes - do nothing....
            break;
            } //end mightly long switch
        } //end if the current element is not an array...
    } //end for loop for Array
}//end buildcookies

// ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//
// wxHIDModule
//
// ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

class wxHIDModule : public wxModule
{
    wxDECLARE_DYNAMIC_CLASS(wxHIDModule);

public:
        static wxArrayPtrVoid sm_keyboards;
        virtual bool OnInit() wxOVERRIDE
        {
            return true;
        }
        virtual void OnExit() wxOVERRIDE
        {
            for(size_t i = 0; i < sm_keyboards.GetCount(); ++i)
                delete (wxHIDKeyboard*) sm_keyboards[i];
            sm_keyboards.Clear();
        }
};

wxIMPLEMENT_DYNAMIC_CLASS(wxHIDModule, wxModule);

wxArrayPtrVoid wxHIDModule::sm_keyboards;

// ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//
// wxGetKeyState()
//
// ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

bool wxGetKeyState (wxKeyCode key)
{
    wxASSERT_MSG(key != WXK_LBUTTON && key != WXK_RBUTTON && key !=
        WXK_MBUTTON, wxT("can't use wxGetKeyState() for mouse buttons"));

    CGKeyCode cgcode = wxCharCodeWXToOSX((wxKeyCode)key);
    return CGEventSourceKeyState(kCGEventSourceStateCombinedSessionState, cgcode);
}

#endif //__DARWIN__
