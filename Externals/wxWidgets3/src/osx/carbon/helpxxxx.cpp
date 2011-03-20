/////////////////////////////////////////////////////////////////////////////
// Name:        src/osx/carbon/helpxxxx.cpp
// Purpose:     Help system: native implementation
// Author:      Stefan Csomor
// Modified by:
// Created:     1998-01-01
// RCS-ID:      $Id: helpxxxx.cpp 67254 2011-03-20 00:14:35Z DS $
// Copyright:   (c) Stefan Csomor
// Licence:       wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#include "wx/wxprec.h"

#include "wx/stubs/helpxxxx.h"

#include <string.h>

IMPLEMENT_DYNAMIC_CLASS(wxXXXXHelpController, wxHelpControllerBase)

wxXXXXHelpController::wxXXXXHelpController()
{
    m_helpFile = "";
}

wxXXXXHelpController::~wxXXXXHelpController()
{
}

bool wxXXXXHelpController::Initialize(const wxString& filename)
{
    m_helpFile = filename;
    // TODO any other inits
    return TRUE;
}

bool wxXXXXHelpController::LoadFile(const wxString& file)
{
    m_helpFile = file;
    // TODO
    return TRUE;
}

bool wxXXXXHelpController::DisplayContents()
{
    // TODO
    return FALSE;
}

bool wxXXXXHelpController::DisplaySection(int section)
{
    // TODO
    return FALSE;
}

bool wxXXXXHelpController::DisplayBlock(long block)
{
    // TODO
    return FALSE;
}

bool wxXXXXHelpController::KeywordSearch(const wxString& k,
                                         wxHelpSearchMode WXUNUSED(mode))
{
    if (m_helpFile == "") return FALSE;

    // TODO
    return FALSE;
}

// Can't close the help window explicitly in WinHelp
bool wxXXXXHelpController::Quit()
{
    // TODO
    return FALSE;
}

void wxXXXXHelpController::OnQuit()
{
}

