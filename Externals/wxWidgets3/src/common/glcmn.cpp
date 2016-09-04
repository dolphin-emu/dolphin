///////////////////////////////////////////////////////////////////////////////
// Name:        src/common/glcmn.cpp
// Purpose:     wxGLCanvasBase implementation
// Author:      Vadim Zeitlin
// Created:     2007-04-09
// Copyright:   (c) 2007 Vadim Zeitlin <vadim@wxwindows.org>
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

// ============================================================================
// declarations
// ============================================================================

// ----------------------------------------------------------------------------
// headers
// ----------------------------------------------------------------------------

// for compilers that support precompilation, includes "wx.h".
#include "wx/wxprec.h"

#ifdef __BORLANDC__
    #pragma hdrstop
#endif

#if wxUSE_GLCANVAS

#ifndef WX_PRECOMP
    #include "wx/log.h"
#endif // WX_PRECOMP

#include "wx/glcanvas.h"

// DLL options compatibility check:
#include "wx/build.h"
WX_CHECK_BUILD_OPTIONS("wxGL")

wxIMPLEMENT_CLASS(wxGLApp, wxApp);

// ============================================================================
// implementation
// ============================================================================

void wxGLAttribsBase::AddAttribBits(int searchVal, int combineVal)
{
    // Search for searchVal
    wxVector<int>::iterator it = m_GLValues.begin();
    while ( it != m_GLValues.end() && *it != searchVal )
        it++;
    // Have we searchVal?
    if ( it != m_GLValues.end() )
    {
        if ( ++it == m_GLValues.end() )
        {
            m_GLValues.push_back(combineVal);
        }
        else
        {
            *it |= combineVal;
        }
    }
    else
    {
        // Add the identifier and the bits
        m_GLValues.push_back(searchVal);
        m_GLValues.push_back(combineVal);
    }
}

// ============================================================================

wxGLCanvasBase::wxGLCanvasBase()
{
#if WXWIN_COMPATIBILITY_2_8
    m_glContext = NULL;
#endif

    // we always paint background entirely ourselves so prevent wx from erasing
    // it to avoid flicker
    SetBackgroundStyle(wxBG_STYLE_CUSTOM);
}

bool wxGLCanvasBase::SetCurrent(const wxGLContext& context) const
{
    // although on MSW it works even if the window is still hidden, it doesn't
    // work in other ports (notably X11-based ones) and documentation mentions
    // that SetCurrent() can only be called for a shown window, so check for it
    wxASSERT_MSG( IsShown(), wxT("can't make hidden GL canvas current") );


    return context.SetCurrent(*static_cast<const wxGLCanvas *>(this));
}

bool wxGLCanvasBase::SetColour(const wxString& colour)
{
    wxColour col = wxTheColourDatabase->Find(colour);
    if ( !col.IsOk() )
        return false;

#ifdef wxHAS_OPENGL_ES
    wxGLAPI::glColor3f((GLfloat) (col.Red() / 256.), (GLfloat) (col.Green() / 256.),
                (GLfloat) (col.Blue() / 256.));
#else
    GLboolean isRGBA;
    glGetBooleanv(GL_RGBA_MODE, &isRGBA);
    if ( isRGBA )
    {
        glColor3f((GLfloat) (col.Red() / 256.), (GLfloat) (col.Green() / 256.),
                (GLfloat) (col.Blue() / 256.));
    }
    else // indexed colour
    {
        GLint pix = GetColourIndex(col);
        if ( pix == -1 )
        {
            wxLogError(_("Failed to allocate colour for OpenGL"));
            return false;
        }

        glIndexi(pix);
    }
#endif
    return true;
}

wxGLCanvasBase::~wxGLCanvasBase()
{
#if WXWIN_COMPATIBILITY_2_8
    delete m_glContext;
#endif // WXWIN_COMPATIBILITY_2_8
}

#if WXWIN_COMPATIBILITY_2_8

wxGLContext *wxGLCanvasBase::GetContext() const
{
    return m_glContext;
}

void wxGLCanvasBase::SetCurrent()
{
    if ( m_glContext )
        SetCurrent(*m_glContext);
}

void wxGLCanvasBase::OnSize(wxSizeEvent& WXUNUSED(event))
{
}

#endif // WXWIN_COMPATIBILITY_2_8

/* static */
bool wxGLCanvasBase::IsExtensionInList(const char *list, const char *extension)
{
    if ( !list )
        return false;

    for ( const char *p = list; *p; p++ )
    {
        // advance up to the next possible match
        p = wxStrstr(p, extension);
        if ( !p )
            break;

        // check that the extension appears at the beginning/ending of the list
        // or is preceded/followed by a space to avoid mistakenly finding
        // "glExtension" in a list containing some "glFunkyglExtension"
        if ( (p == list || p[-1] == ' ') )
        {
            char c = p[strlen(extension)];
            if ( c == '\0' || c == ' ' )
                return true;
        }
    }

    return false;
}

/* static */
bool wxGLCanvasBase::ParseAttribList(const int *attribList,
                                     wxGLAttributes& dispAttrs,
                                     wxGLContextAttrs* ctxAttrs)
{
    // Some attributes are usually needed
    dispAttrs.PlatformDefaults();
    if ( ctxAttrs )
        ctxAttrs->PlatformDefaults();

    if ( !attribList )
    {
        // Set default attributes
        dispAttrs.Defaults();
        dispAttrs.EndList();
        if ( ctxAttrs )
            ctxAttrs->EndList();
        return true;
    }

    int src = 0;
    int minColo[4] = {0, 0, 0, 0};
    int minAcum[4] = {0, 0, 0, 0};
    int num = 0;
    while ( attribList[src] )
    {
        // Check a non zero-terminated list. This may help a bit with malformed lists.
        if ( ++num > 200 )
        {
            wxFAIL_MSG("The attributes list is not zero-terminated");
        }

        switch ( attribList[src++] )
        {
            // Pixel format attributes

            case WX_GL_RGBA:
                dispAttrs.RGBA();
                break;

            case WX_GL_BUFFER_SIZE:
                dispAttrs.BufferSize(attribList[src++]);
                break;

            case WX_GL_LEVEL:
                dispAttrs.Level(attribList[src++]);
                break;

            case WX_GL_DOUBLEBUFFER:
                dispAttrs.DoubleBuffer();
                break;

            case WX_GL_STEREO:
                dispAttrs.Stereo();
                break;

            case WX_GL_AUX_BUFFERS:
                dispAttrs.AuxBuffers(attribList[src++]);
                break;

            case WX_GL_MIN_RED:
                minColo[0] = attribList[src++];
                break;

            case WX_GL_MIN_GREEN:
                minColo[1] = attribList[src++];
                break;

            case WX_GL_MIN_BLUE:
                minColo[2] = attribList[src++];
                break;

            case WX_GL_MIN_ALPHA:
                minColo[3] = attribList[src++];
                break;

            case WX_GL_DEPTH_SIZE:
                dispAttrs.Depth(attribList[src++]);
                break;

            case WX_GL_STENCIL_SIZE:
                dispAttrs.Stencil(attribList[src++]);
                break;

            case WX_GL_MIN_ACCUM_RED:
                minAcum[0] = attribList[src++];
                break;

            case WX_GL_MIN_ACCUM_GREEN:
                minAcum[1] = attribList[src++];
                break;

            case WX_GL_MIN_ACCUM_BLUE:
                minAcum[2] = attribList[src++];
                break;

            case WX_GL_MIN_ACCUM_ALPHA:
                minAcum[3] = attribList[src++];
                break;

            case WX_GL_SAMPLE_BUFFERS:
                dispAttrs.SampleBuffers(attribList[src++]);
                break;

            case WX_GL_SAMPLES:
                dispAttrs.Samplers(attribList[src++]);
                break;

            case WX_GL_FRAMEBUFFER_SRGB:
                dispAttrs.FrameBuffersRGB();
                break;

            // Context attributes

            case WX_GL_CORE_PROFILE:
                if ( ctxAttrs )
                    ctxAttrs->CoreProfile();
                break;

            case WX_GL_MAJOR_VERSION:
                if ( ctxAttrs )
                    ctxAttrs->MajorVersion(attribList[src]);
                src++;
                break;

            case WX_GL_MINOR_VERSION:
                if ( ctxAttrs )
                    ctxAttrs->MinorVersion(attribList[src]);
                src++;
                break;

            case wx_GL_COMPAT_PROFILE:
                if ( ctxAttrs )
                    ctxAttrs->CompatibilityProfile();
                break;

            case WX_GL_FORWARD_COMPAT:
                if ( ctxAttrs )
                    ctxAttrs->ForwardCompatible();
                break;

            case WX_GL_ES2:
                if ( ctxAttrs )
                    ctxAttrs->ES2();
                break;

            case WX_GL_DEBUG:
                if ( ctxAttrs )
                    ctxAttrs->DebugCtx();
                break;

            case WX_GL_ROBUST_ACCESS:
                if ( ctxAttrs )
                    ctxAttrs->Robust();
                break;

            case WX_GL_NO_RESET_NOTIFY:
                if ( ctxAttrs )
                    ctxAttrs->NoResetNotify();
                break;

            case WX_GL_LOSE_ON_RESET:
                if ( ctxAttrs )
                    ctxAttrs->LoseOnReset();
                break;

            case WX_GL_RESET_ISOLATION:
                if ( ctxAttrs )
                    ctxAttrs->ResetIsolation();
                break;

            case WX_GL_RELEASE_FLUSH:
                if ( ctxAttrs )
                    ctxAttrs->ReleaseFlush(1);
                break;

            case WX_GL_RELEASE_NONE:
                if ( ctxAttrs )
                    ctxAttrs->ReleaseFlush(0);
                break;

            default:
                wxFAIL_MSG("Unexpected value in attributes list");
                return false;
        }
    }

    // Set color and accumulation
    if ( minColo[0] || minColo[1] || minColo[2] || minColo[3] )
        dispAttrs.MinRGBA(minColo[0], minColo[1], minColo[2], minColo[3]);
    if ( minAcum[0] || minAcum[1] || minAcum[2] || minAcum[3] )
        dispAttrs.MinAcumRGBA(minAcum[0], minAcum[1], minAcum[2], minAcum[3]);

    // The attributes lists must be zero-terminated
    dispAttrs.EndList();
    if ( ctxAttrs )
        ctxAttrs->EndList();

    return true;
}

// ============================================================================
// compatibility layer for OpenGL 3 and OpenGL ES
// ============================================================================

static wxGLAPI s_glAPI;

#if wxUSE_OPENGL_EMULATION

#include "wx/vector.h"

static GLenum s_mode;

static GLfloat s_currentTexCoord[2];
static GLfloat s_currentColor[4];
static GLfloat s_currentNormal[3];

// TODO move this into a different construct with locality for all attributes
// of a vertex

static wxVector<GLfloat> s_texCoords;
static wxVector<GLfloat> s_vertices;
static wxVector<GLfloat> s_normals;
static wxVector<GLfloat> s_colors;

static bool s_texCoordsUsed;
static bool s_colorsUsed;
static bool s_normalsUsed;

bool SetState( int flag, bool desired )
{
    bool former = glIsEnabled( flag );
    if ( former != desired )
    {
        if ( desired )
            glEnableClientState(flag);
        else
            glDisableClientState(flag);
    }
    return former;
}

void RestoreState( int flag, bool desired )
{
    if ( desired )
        glEnableClientState(flag);
    else
        glDisableClientState(flag);
}
#endif

wxGLAPI::wxGLAPI()
{
#if wxUSE_OPENGL_EMULATION
    s_mode = 0xFF;
#endif
}

wxGLAPI::~wxGLAPI()
{
}

void wxGLAPI::glFrustum(GLfloat left, GLfloat right, GLfloat bottom,
                            GLfloat top, GLfloat zNear, GLfloat zFar)
{
#if wxUSE_OPENGL_EMULATION
    ::glFrustumf(left, right, bottom, top, zNear, zFar);
#else
    ::glFrustum(left, right, bottom, top, zNear, zFar);
#endif
}

void wxGLAPI::glBegin(GLenum mode)
{
#if wxUSE_OPENGL_EMULATION
    if ( s_mode != 0xFF )
    {
        wxFAIL_MSG("nested glBegin");
    }

    s_mode = mode;
    s_texCoordsUsed = false;
    s_colorsUsed = false;
    s_normalsUsed = false;

    s_texCoords.clear();
    s_normals.clear();
    s_colors.clear();
    s_vertices.clear();
#else
    ::glBegin(mode);
#endif
}

void wxGLAPI::glTexCoord2f(GLfloat s, GLfloat t)
{
#if wxUSE_OPENGL_EMULATION
    if ( s_mode == 0xFF )
    {
        wxFAIL_MSG("glTexCoord2f called outside glBegin/glEnd");
    }

    else
    {
        s_texCoordsUsed = true;
        s_currentTexCoord[0] = s;
        s_currentTexCoord[1] = t;
    }
#else
    ::glTexCoord2f(s,t);
#endif
}

void wxGLAPI::glVertex3f(GLfloat x, GLfloat y, GLfloat z)
{
#if wxUSE_OPENGL_EMULATION
    if ( s_mode == 0xFF )
    {
        wxFAIL_MSG("glVertex3f called outside glBegin/glEnd");
    }
    else
    {
        s_texCoords.push_back(s_currentTexCoord[0]);
        s_texCoords.push_back(s_currentTexCoord[1]);

        s_normals.push_back(s_currentNormal[0]);
        s_normals.push_back(s_currentNormal[1]);
        s_normals.push_back(s_currentNormal[2]);

        s_colors.push_back(s_currentColor[0]);
        s_colors.push_back(s_currentColor[1]);
        s_colors.push_back(s_currentColor[2]);
        s_colors.push_back(s_currentColor[3]);

        s_vertices.push_back(x);
        s_vertices.push_back(y);
        s_vertices.push_back(z);
    }
#else
    ::glVertex3f(x,y,z);
#endif
}

void wxGLAPI::glNormal3f(GLfloat nx, GLfloat ny, GLfloat nz)
{
#if wxUSE_OPENGL_EMULATION
    if ( s_mode == 0xFF )
        ::glNormal3f(nx,ny,nz);
    else
    {
        s_normalsUsed = true;
        s_currentNormal[0] = nx;
        s_currentNormal[1] = ny;
        s_currentNormal[2] = nz;
    }
#else
    ::glNormal3f(nx,ny,nz);
#endif
}

void wxGLAPI::glColor4f(GLfloat r, GLfloat g, GLfloat b, GLfloat a)
{
#if wxUSE_OPENGL_EMULATION
    if ( s_mode == 0xFF )
        ::glColor4f(r,g,b,a);
    else
    {
        s_colorsUsed = true;
        s_currentColor[0] = r;
        s_currentColor[1] = g;
        s_currentColor[2] = b;
        s_currentColor[3] = a;
    }
#else
    ::glColor4f(r,g,b,a);
#endif
}

void wxGLAPI::glColor3f(GLfloat r, GLfloat g, GLfloat b)
{
#if wxUSE_OPENGL_EMULATION
    glColor4f(r,g,b,1.0);
#else
    ::glColor3f(r,g,b);
#endif
}

void wxGLAPI::glEnd()
{
#if wxUSE_OPENGL_EMULATION
    bool formerColors = SetState( GL_COLOR_ARRAY, s_colorsUsed );
    bool formerNormals = SetState( GL_NORMAL_ARRAY, s_normalsUsed );
    bool formerTexCoords = SetState( GL_TEXTURE_COORD_ARRAY, s_texCoordsUsed );
    bool formerVertex = glIsEnabled(GL_VERTEX_ARRAY);

    if( !formerVertex )
        glEnableClientState(GL_VERTEX_ARRAY);

    if ( s_colorsUsed )
        glColorPointer( 4, GL_FLOAT, 0, &s_colors[0] );

    if ( s_normalsUsed )
        glNormalPointer( GL_FLOAT, 0, &s_normals[0] );

    if ( s_texCoordsUsed )
        glTexCoordPointer( 2, GL_FLOAT, 0, &s_texCoords[0] );

    glVertexPointer(3, GL_FLOAT, 0, &s_vertices[0]);
    glDrawArrays( s_mode, 0, s_vertices.size() / 3 );

    if ( s_colorsUsed != formerColors )
        RestoreState( GL_COLOR_ARRAY, formerColors );

    if ( s_normalsUsed != formerNormals )
        RestoreState( GL_NORMAL_ARRAY, formerColors );

    if ( s_texCoordsUsed != formerTexCoords )
        RestoreState( GL_TEXTURE_COORD_ARRAY, formerColors );

    if( !formerVertex )
        glDisableClientState(GL_VERTEX_ARRAY);

    s_mode = 0xFF;
#else
    ::glEnd();
#endif
}

#endif // wxUSE_GLCANVAS

