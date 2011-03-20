/////////////////////////////////////////////////////////////////////////////
// Name:        src/common/cairo.cpp
// Purpose:     Cairo library
// Author:      Anthony Betaudeau
// Created:     2007-08-25
// RCS-ID:      $Id: cairo.cpp 65561 2010-09-17 11:17:55Z DS $
// Copyright:   (c) Anthony Bretaudeau
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////


// For compilers that support precompilation, includes "wx/wx.h".
#include "wx/wxprec.h"

#ifdef __BORLANDC__
    #pragma hdrstop
#endif

#include "wx/cairo.h"

#if wxUSE_CAIRO

#ifndef WX_PRECOMP
    #include "wx/module.h"
    #include "wx/log.h"
#endif


wxCairoLibrary *wxCairoLibrary::ms_lib = NULL;

//----------------------------------------------------------------------------
// wxCairoLibrary
//----------------------------------------------------------------------------

wxCairoLibrary::wxCairoLibrary()
{
    wxLogNull log;

    m_libCairo.Load("libcairo.so.2");
    m_ok = m_libCairo.IsLoaded();
    if ( !m_ok )
        return;

    m_libPangoCairo.Load("libpangocairo-1.0.so.0");
    m_ok = m_libPangoCairo.IsLoaded();
    if ( !m_ok )
    {
        m_libCairo.Unload();
        return;
    }

    m_ok = InitializeMethods();
}

wxCairoLibrary::~wxCairoLibrary()
{
}

/* static */ wxCairoLibrary* wxCairoLibrary::Get()
{
    if ( !ms_lib )
    {
        ms_lib = new wxCairoLibrary();
        if ( !ms_lib->IsOk() )
        {
            delete ms_lib;
            ms_lib = NULL;
        }
    }

    return ms_lib;
}

/* static */ void wxCairoLibrary::CleanUp()
{
    if (ms_lib)
    {
        delete ms_lib;
        ms_lib = NULL;
    }
}

bool wxCairoLibrary::IsOk()
{
    return m_ok;
}

bool wxCairoLibrary::InitializeMethods()
{
    wxDL_METHOD_LOAD(m_libCairo, cairo_arc);
    wxDL_METHOD_LOAD(m_libCairo, cairo_arc_negative);
    wxDL_METHOD_LOAD(m_libCairo, cairo_clip);
    wxDL_METHOD_LOAD(m_libCairo, cairo_close_path);
    wxDL_METHOD_LOAD(m_libCairo, cairo_create);
    wxDL_METHOD_LOAD(m_libCairo, cairo_curve_to);
    wxDL_METHOD_LOAD(m_libCairo, cairo_destroy);
    wxDL_METHOD_LOAD(m_libCairo, cairo_fill);
    wxDL_METHOD_LOAD(m_libCairo, cairo_fill_preserve);
    wxDL_METHOD_LOAD(m_libCairo, cairo_get_target);
    wxDL_METHOD_LOAD(m_libCairo, cairo_image_surface_create_for_data);
    wxDL_METHOD_LOAD(m_libCairo, cairo_line_to);
    wxDL_METHOD_LOAD(m_libCairo, cairo_move_to);
    wxDL_METHOD_LOAD(m_libCairo, cairo_new_path);
    wxDL_METHOD_LOAD(m_libCairo, cairo_paint);
    wxDL_METHOD_LOAD(m_libCairo, cairo_pattern_add_color_stop_rgba);
    wxDL_METHOD_LOAD(m_libCairo, cairo_pattern_create_for_surface);
    wxDL_METHOD_LOAD(m_libCairo, cairo_pattern_create_linear);
    wxDL_METHOD_LOAD(m_libCairo, cairo_pattern_create_radial);
    wxDL_METHOD_LOAD(m_libCairo, cairo_pattern_destroy);
    wxDL_METHOD_LOAD(m_libCairo, cairo_pattern_set_extend);
    wxDL_METHOD_LOAD(m_libCairo, cairo_pattern_set_filter);
    wxDL_METHOD_LOAD(m_libCairo, cairo_rectangle);
    wxDL_METHOD_LOAD(m_libCairo, cairo_reset_clip);
    wxDL_METHOD_LOAD(m_libCairo, cairo_restore);
    wxDL_METHOD_LOAD(m_libCairo, cairo_rotate);
    wxDL_METHOD_LOAD(m_libCairo, cairo_save);
    wxDL_METHOD_LOAD(m_libCairo, cairo_scale);
    wxDL_METHOD_LOAD(m_libCairo, cairo_set_dash);
    wxDL_METHOD_LOAD(m_libCairo, cairo_set_fill_rule);
    wxDL_METHOD_LOAD(m_libCairo, cairo_set_line_cap);
    wxDL_METHOD_LOAD(m_libCairo, cairo_set_line_join);
    wxDL_METHOD_LOAD(m_libCairo, cairo_set_line_width);
    wxDL_METHOD_LOAD(m_libCairo, cairo_set_operator);
    wxDL_METHOD_LOAD(m_libCairo, cairo_set_source);
    wxDL_METHOD_LOAD(m_libCairo, cairo_set_source_rgba);
    wxDL_METHOD_LOAD(m_libCairo, cairo_stroke);
    wxDL_METHOD_LOAD(m_libCairo, cairo_stroke_preserve);
    wxDL_METHOD_LOAD(m_libCairo, cairo_surface_create_similar);
    wxDL_METHOD_LOAD(m_libCairo, cairo_surface_destroy);
    wxDL_METHOD_LOAD(m_libCairo, cairo_translate);

    wxDL_METHOD_LOAD(m_libPangoCairo, pango_cairo_update_layout);
    wxDL_METHOD_LOAD(m_libPangoCairo, pango_cairo_show_layout);

    return true;
}

//----------------------------------------------------------------------------
// wxCairoModule
//----------------------------------------------------------------------------

class wxCairoModule : public wxModule
{
public:
    wxCairoModule() { }
    virtual bool OnInit();
    virtual void OnExit();

private:
    DECLARE_DYNAMIC_CLASS(wxCairotModule)
};

bool wxCairoModule::OnInit()
{
    return true;
}

void wxCairoModule::OnExit()
{
    wxCairoLibrary::CleanUp();
}

IMPLEMENT_DYNAMIC_CLASS(wxCairoModule, wxModule)

#endif // wxUSE_CAIRO
