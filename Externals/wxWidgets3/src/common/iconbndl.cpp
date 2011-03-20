/////////////////////////////////////////////////////////////////////////////
// Name:        src/common/iconbndl.cpp
// Purpose:     wxIconBundle
// Author:      Mattia Barbon, Vadim Zeitlin
// Created:     23.03.2002
// RCS-ID:      $Id: iconbndl.cpp 66374 2010-12-14 18:43:49Z VZ $
// Copyright:   (c) Mattia barbon
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

// For compilers that support precompilation, includes "wx.h".
#include "wx/wxprec.h"

#ifdef __BORLANDC__
    #pragma hdrstop
#endif

#include "wx/iconbndl.h"

#ifndef WX_PRECOMP
    #include "wx/settings.h"
    #include "wx/log.h"
    #include "wx/intl.h"
    #include "wx/bitmap.h"
    #include "wx/image.h"
    #include "wx/stream.h"
#endif

#include "wx/wfstream.h"

#include "wx/arrimpl.cpp"
WX_DEFINE_OBJARRAY(wxIconArray)

IMPLEMENT_DYNAMIC_CLASS(wxIconBundle, wxGDIObject)

#define M_ICONBUNDLEDATA static_cast<wxIconBundleRefData*>(m_refData)

// ----------------------------------------------------------------------------
// wxIconBundleRefData
// ----------------------------------------------------------------------------

class WXDLLEXPORT wxIconBundleRefData : public wxGDIRefData
{
public:
    wxIconBundleRefData() { }

    // We need the copy ctor for CloneGDIRefData() but notice that we use the
    // base class default ctor in it and not the copy one which it doesn't have.
    wxIconBundleRefData(const wxIconBundleRefData& other)
        : wxGDIRefData(),
          m_icons(other.m_icons)
    {
    }

    // default assignment operator and dtor are ok

    virtual bool IsOk() const { return !m_icons.empty(); }

    wxIconArray m_icons;
};

// ============================================================================
// wxIconBundle implementation
// ============================================================================

wxIconBundle::wxIconBundle()
{
}

#if wxUSE_STREAMS && wxUSE_IMAGE

#if wxUSE_FFILE || wxUSE_FILE
wxIconBundle::wxIconBundle(const wxString& file, wxBitmapType type)
            : wxGDIObject()
{
    AddIcon(file, type);
}
#endif // wxUSE_FFILE || wxUSE_FILE

wxIconBundle::wxIconBundle(wxInputStream& stream, wxBitmapType type)
            : wxGDIObject()
{
    AddIcon(stream, type);
}
#endif // wxUSE_STREAMS && wxUSE_IMAGE

wxIconBundle::wxIconBundle(const wxIcon& icon)
            : wxGDIObject()
{
    AddIcon(icon);
}

wxGDIRefData *wxIconBundle::CreateGDIRefData() const
{
    return new wxIconBundleRefData;
}

wxGDIRefData *wxIconBundle::CloneGDIRefData(const wxGDIRefData *data) const
{
    return new wxIconBundleRefData(*static_cast<const wxIconBundleRefData *>(data));
}

void wxIconBundle::DeleteIcons()
{
    UnRef();
}

#if wxUSE_STREAMS && wxUSE_IMAGE

namespace
{

// Adds icon from 'input' to the bundle. Shows 'errorMessage' on failure
// (it must contain "%d", because it is used to report # of image in the file
// that failed to load):
void DoAddIcon(wxIconBundle& bundle,
               wxInputStream& input,
               wxBitmapType type,
               const wxString& errorMessage)
{
    wxImage image;

    const wxFileOffset posOrig = input.TellI();

    const size_t count = wxImage::GetImageCount(input, type);
    for ( size_t i = 0; i < count; ++i )
    {
        if ( i )
        {
            // the call to LoadFile() for the first sub-image updated the
            // stream position but we need to start reading the subsequent
            // sub-image at the image beginning too
            input.SeekI(posOrig);
        }

        if ( !image.LoadFile(input, type, i) )
        {
            wxLogError(errorMessage, i);
            continue;
        }

        if ( type == wxBITMAP_TYPE_ANY )
        {
            // store the type so that we don't need to try all handlers again
            // for the subsequent images, they should all be of the same type
            type = image.GetType();
        }

        wxIcon tmp;
        tmp.CopyFromBitmap(wxBitmap(image));
        bundle.AddIcon(tmp);
    }
}

} // anonymous namespace

#if wxUSE_FFILE || wxUSE_FILE

void wxIconBundle::AddIcon(const wxString& file, wxBitmapType type)
{
#ifdef __WXMAC__
    // Deal with standard icons
    if ( type == wxBITMAP_TYPE_ICON_RESOURCE )
    {
        wxIcon tmp(file, type);
        if (tmp.Ok())
        {
            AddIcon(tmp);
            return;
        }
    }
#endif // __WXMAC__

#if wxUSE_FFILE
    wxFFileInputStream stream(file);
#elif wxUSE_FILE
    wxFileInputStream stream(file);
#endif
    DoAddIcon
    (
        *this,
        stream, type,
        wxString::Format(_("Failed to load image %%d from file '%s'."), file)
    );
}

#endif // wxUSE_FFILE || wxUSE_FILE

void wxIconBundle::AddIcon(wxInputStream& stream, wxBitmapType type)
{
    DoAddIcon(*this, stream, type, _("Failed to load image %d from stream."));
}

#endif // wxUSE_STREAMS && wxUSE_IMAGE

wxIcon wxIconBundle::GetIcon(const wxSize& size) const
{
    const size_t count = GetIconCount();

    // optimize for the common case of icon bundles containing one icon only
    wxIcon iconBest;
    switch ( count )
    {
        case 0:
            // nothing to do, iconBest is already invalid
            break;

        case 1:
            iconBest = M_ICONBUNDLEDATA->m_icons[0];
            break;

        default:
            // there is more than one icon, find the best match:
            wxCoord sysX = wxSystemSettings::GetMetric( wxSYS_ICON_X ),
                    sysY = wxSystemSettings::GetMetric( wxSYS_ICON_Y );

            const wxIconArray& iconArray = M_ICONBUNDLEDATA->m_icons;
            for ( size_t i = 0; i < count; i++ )
            {
                const wxIcon& icon = iconArray[i];
                wxCoord sx = icon.GetWidth(),
                        sy = icon.GetHeight();

                // if we got an icon of exactly the requested size, we're done
                if ( sx == size.x && sy == size.y )
                {
                    iconBest = icon;
                    break;
                }

                // the best icon is by default (arbitrarily) the first one but
                // if we find a system-sized icon, take it instead
                if ((sx == sysX && sy == sysY) || !iconBest.IsOk())
                    iconBest = icon;
            }
    }

#if defined( __WXMAC__ ) && wxOSX_USE_CARBON
    return wxIcon(iconBest.GetHICON(), size);
#else
    return iconBest;
#endif
}

wxIcon wxIconBundle::GetIconOfExactSize(const wxSize& size) const
{
    wxIcon icon = GetIcon(size);
    if ( icon.Ok() &&
            (icon.GetWidth() != size.x || icon.GetHeight() != size.y) )
    {
        icon = wxNullIcon;
    }

    return icon;
}

void wxIconBundle::AddIcon(const wxIcon& icon)
{
    wxCHECK_RET( icon.IsOk(), wxT("invalid icon") );

    AllocExclusive();

    wxIconArray& iconArray = M_ICONBUNDLEDATA->m_icons;

    // replace existing icon with the same size if we already have it
    const size_t count = iconArray.size();
    for ( size_t i = 0; i < count; ++i )
    {
        wxIcon& tmp = iconArray[i];
        if ( tmp.Ok() &&
                tmp.GetWidth() == icon.GetWidth() &&
                tmp.GetHeight() == icon.GetHeight() )
        {
            tmp = icon;
            return;
        }
    }

    // if we don't, add an icon with new size
    iconArray.Add(icon);
}

size_t wxIconBundle::GetIconCount() const
{
    return IsOk() ? M_ICONBUNDLEDATA->m_icons.size() : 0;
}

wxIcon wxIconBundle::GetIconByIndex(size_t n) const
{
    wxCHECK_MSG( n < GetIconCount(), wxNullIcon, wxT("invalid index") );

    return M_ICONBUNDLEDATA->m_icons[n];
}
