/////////////////////////////////////////////////////////////////////////////
// Name:        src/osx/carbon/graphics.cpp
// Purpose:     wxDC class
// Author:      Stefan Csomor
// Modified by:
// Created:     01/02/97
// copyright:   (c) Stefan Csomor
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#include "wx/wxprec.h"

#include "wx/graphics.h"
#include "wx/private/graphics.h"

#ifndef WX_PRECOMP
    #include "wx/dcclient.h"
    #include "wx/dcmemory.h"
    #include "wx/dcprint.h"
    #include "wx/log.h"
    #include "wx/region.h"
    #include "wx/image.h"
    #include "wx/icon.h"
#endif


#ifdef __MSL__
    #if __MSL__ >= 0x6000
        #include "math.h"
        // in case our functions were defined outside std, we make it known all the same
        namespace std { }
        using namespace std;
    #endif
#endif

#ifdef __WXMAC__
    #include "wx/osx/private.h"
    #include "wx/osx/dcprint.h"
    #include "wx/osx/dcclient.h"
    #include "wx/osx/dcmemory.h"
    #include "wx/osx/private.h"
#else
    #include "CoreServices/CoreServices.h"
    #include "ApplicationServices/ApplicationServices.h"
    #include "wx/osx/core/cfstring.h"
    #include "wx/cocoa/dcclient.h"
#endif

#if wxOSX_USE_COCOA_OR_IPHONE
extern CGContextRef wxOSXGetContextFromCurrentContext() ;
#if wxOSX_USE_COCOA
extern bool wxOSXLockFocus( WXWidget view) ;
extern void wxOSXUnlockFocus( WXWidget view) ;
#endif
#endif

// copying values from NSCompositingModes (see also webkit and cairo sources)

typedef enum CGCompositeOperation {
   kCGCompositeOperationClear           = 0,
   kCGCompositeOperationCopy            = 1,
   kCGCompositeOperationSourceOver      = 2,
   kCGCompositeOperationSourceIn        = 3,
   kCGCompositeOperationSourceOut       = 4,
   kCGCompositeOperationSourceAtop      = 5,
   kCGCompositeOperationDestinationOver = 6,
   kCGCompositeOperationDestinationIn   = 7,
   kCGCompositeOperationDestinationOut  = 8,
   kCGCompositeOperationDestinationAtop = 9,
   kCGCompositeOperationXOR             = 10,
   kCGCompositeOperationPlusDarker      = 11,
// NS only, unsupported by CG : Highlight
   kCGCompositeOperationPlusLighter     = 12
} CGCompositeOperation ;

extern "C"
{
   CG_EXTERN void CGContextSetCompositeOperation (CGContextRef context, int operation);
} ;

//-----------------------------------------------------------------------------
// constants
//-----------------------------------------------------------------------------

#ifndef M_PI
const double M_PI = 3.14159265358979;
#endif

//
// Pen, Brushes and Fonts
//

#pragma mark -
#pragma mark wxMacCoreGraphicsPattern, ImagePattern, HatchPattern classes

OSStatus wxMacDrawCGImage(
                  CGContextRef    inContext,
                  const CGRect *  inBounds,
                  CGImageRef      inImage)
{
    CGContextSaveGState(inContext);
    CGContextTranslateCTM(inContext, inBounds->origin.x, inBounds->origin.y + inBounds->size.height);
    CGRect r = *inBounds;
    r.origin.x = r.origin.y = 0;
    CGContextScaleCTM(inContext, 1, -1);
    CGContextDrawImage(inContext, r, inImage );
    CGContextRestoreGState(inContext);
    return noErr;
}

CGColorRef wxMacCreateCGColor( const wxColour& col )
{
    CGColorRef retval = col.CreateCGColor();

    wxASSERT(retval != NULL);
    return retval;
}

CTFontRef wxMacCreateCTFont( const wxFont& font )
{
#ifdef __WXMAC__
    return wxCFRetain((CTFontRef) font.OSXGetCTFont());
#else
    return CTFontCreateWithName( wxCFStringRef( font.GetFaceName(), wxLocale::GetSystemEncoding() ) , font.GetPointSize() , NULL );
#endif
}

// CGPattern wrapper class: always allocate on heap, never call destructor

class wxMacCoreGraphicsPattern
{
public :
    wxMacCoreGraphicsPattern() {}

    // is guaranteed to be called only with a non-Null CGContextRef
    virtual void Render( CGContextRef ctxRef ) = 0;

    operator CGPatternRef() const { return m_patternRef; }

protected :
    virtual ~wxMacCoreGraphicsPattern()
    {
        // as this is called only when the m_patternRef is been released;
        // don't release it again
    }

    static void _Render( void *info, CGContextRef ctxRef )
    {
        wxMacCoreGraphicsPattern* self = (wxMacCoreGraphicsPattern*) info;
        if ( self && ctxRef )
            self->Render( ctxRef );
    }

    static void _Dispose( void *info )
    {
        wxMacCoreGraphicsPattern* self = (wxMacCoreGraphicsPattern*) info;
        delete self;
    }

    CGPatternRef m_patternRef;

    static const CGPatternCallbacks ms_Callbacks;
};

const CGPatternCallbacks wxMacCoreGraphicsPattern::ms_Callbacks = { 0, &wxMacCoreGraphicsPattern::_Render, &wxMacCoreGraphicsPattern::_Dispose };

class ImagePattern : public wxMacCoreGraphicsPattern
{
public :
    ImagePattern( const wxBitmap* bmp , const CGAffineTransform& transform )
    {
        wxASSERT( bmp && bmp->IsOk() );
#ifdef __WXMAC__
        Init( (CGImageRef) bmp->CreateCGImage() , transform );
#endif
    }

    // ImagePattern takes ownership of CGImageRef passed in
    ImagePattern( CGImageRef image , const CGAffineTransform& transform )
    {
        if ( image )
            CFRetain( image );

        Init( image , transform );
    }

    virtual void Render( CGContextRef ctxRef ) wxOVERRIDE
    {
        if (m_image != NULL)
            wxMacDrawCGImage( ctxRef, &m_imageBounds, m_image );
    }

protected :
    void Init( CGImageRef image, const CGAffineTransform& transform )
    {
        m_image = image;
        if ( m_image )
        {
            m_imageBounds = CGRectMake( (CGFloat) 0.0, (CGFloat) 0.0, (CGFloat)CGImageGetWidth( m_image ), (CGFloat)CGImageGetHeight( m_image ) );
            m_patternRef = CGPatternCreate(
                this , m_imageBounds, transform ,
                m_imageBounds.size.width, m_imageBounds.size.height,
                kCGPatternTilingNoDistortion, true , &wxMacCoreGraphicsPattern::ms_Callbacks );
        }
    }

    virtual ~ImagePattern()
    {
        if ( m_image )
            CGImageRelease( m_image );
    }

    CGImageRef m_image;
    CGRect m_imageBounds;
};

class HatchPattern : public wxMacCoreGraphicsPattern
{
public :
    HatchPattern( int hatchstyle, const CGAffineTransform& transform )
    {
        m_hatch = hatchstyle;
        m_imageBounds = CGRectMake( (CGFloat) 0.0, (CGFloat) 0.0, (CGFloat) 8.0 , (CGFloat) 8.0 );
        m_patternRef = CGPatternCreate(
            this , m_imageBounds, transform ,
            m_imageBounds.size.width, m_imageBounds.size.height,
            kCGPatternTilingNoDistortion, false , &wxMacCoreGraphicsPattern::ms_Callbacks );
    }

    void StrokeLineSegments( CGContextRef ctxRef , const CGPoint pts[] , size_t count )
    {
        CGContextStrokeLineSegments( ctxRef , pts , count );
    }

    virtual void Render( CGContextRef ctxRef ) wxOVERRIDE
    {
        switch ( m_hatch )
        {
            case wxBDIAGONAL_HATCH :
                {
                    CGPoint pts[] =
                    {
                    { (CGFloat) 8.0 , (CGFloat) 0.0 } , { (CGFloat) 0.0 , (CGFloat) 8.0 }
                    };
                    StrokeLineSegments( ctxRef , pts , 2 );
                }
                break;

            case wxCROSSDIAG_HATCH :
                {
                    CGPoint pts[] =
                    {
                        { (CGFloat) 0.0 , (CGFloat) 0.0 } , { (CGFloat) 8.0 , (CGFloat) 8.0 } ,
                        { (CGFloat) 8.0 , (CGFloat) 0.0 } , { (CGFloat) 0.0 , (CGFloat) 8.0 }
                    };
                    StrokeLineSegments( ctxRef , pts , 4 );
                }
                break;

            case wxFDIAGONAL_HATCH :
                {
                    CGPoint pts[] =
                    {
                    { (CGFloat) 0.0 , (CGFloat) 0.0 } , { (CGFloat) 8.0 , (CGFloat) 8.0 }
                    };
                    StrokeLineSegments( ctxRef , pts , 2 );
                }
                break;

            case wxCROSS_HATCH :
                {
                    CGPoint pts[] =
                    {
                    { (CGFloat) 0.0 , (CGFloat) 4.0 } , { (CGFloat) 8.0 , (CGFloat) 4.0 } ,
                    { (CGFloat) 4.0 , (CGFloat) 0.0 } , { (CGFloat) 4.0 , (CGFloat) 8.0 } ,
                    };
                    StrokeLineSegments( ctxRef , pts , 4 );
                }
                break;

            case wxHORIZONTAL_HATCH :
                {
                    CGPoint pts[] =
                    {
                    { (CGFloat) 0.0 , (CGFloat) 4.0 } , { (CGFloat) 8.0 , (CGFloat) 4.0 } ,
                    };
                    StrokeLineSegments( ctxRef , pts , 2 );
                }
                break;

            case wxVERTICAL_HATCH :
                {
                    CGPoint pts[] =
                    {
                    { (CGFloat) 4.0 , (CGFloat) 0.0 } , { (CGFloat) 4.0 , (CGFloat) 8.0 } ,
                    };
                    StrokeLineSegments( ctxRef , pts , 2 );
                }
                break;

            default:
                break;
        }
    }

protected :
    virtual ~HatchPattern() {}

    CGRect      m_imageBounds;
    int         m_hatch;
};

class wxMacCoreGraphicsPenData : public wxGraphicsObjectRefData
{
public:
    wxMacCoreGraphicsPenData( wxGraphicsRenderer* renderer, const wxPen &pen );
    ~wxMacCoreGraphicsPenData();

    void Init();
    virtual void Apply( wxGraphicsContext* context );
    virtual wxDouble GetWidth() { return m_width; }

protected :
    CGLineCap m_cap;
    wxCFRef<CGColorRef> m_color;
    wxCFRef<CGColorSpaceRef> m_colorSpace;

    CGLineJoin m_join;
    CGFloat m_width;

    int m_count;
    const CGFloat *m_lengths;
    CGFloat *m_userLengths;


    bool m_isPattern;
    wxCFRef<CGPatternRef> m_pattern;
    CGFloat* m_patternColorComponents;
};

wxMacCoreGraphicsPenData::wxMacCoreGraphicsPenData( wxGraphicsRenderer* renderer, const wxPen &pen ) :
    wxGraphicsObjectRefData( renderer )
{
    Init();

    m_color.reset( wxMacCreateCGColor( pen.GetColour() ) ) ;

    // TODO: * m_dc->m_scaleX
    m_width = pen.GetWidth();
    if (m_width <= 0.0)
        m_width = (CGFloat) 0.1;

    switch ( pen.GetCap() )
    {
        case wxCAP_ROUND :
            m_cap = kCGLineCapRound;
            break;

        case wxCAP_PROJECTING :
            m_cap = kCGLineCapSquare;
            break;

        case wxCAP_BUTT :
            m_cap = kCGLineCapButt;
            break;

        default :
            m_cap = kCGLineCapButt;
            break;
    }

    switch ( pen.GetJoin() )
    {
        case wxJOIN_BEVEL :
            m_join = kCGLineJoinBevel;
            break;

        case wxJOIN_MITER :
            m_join = kCGLineJoinMiter;
            break;

        case wxJOIN_ROUND :
            m_join = kCGLineJoinRound;
            break;

        default :
            m_join = kCGLineJoinMiter;
            break;
    }

    const CGFloat dashUnit = m_width < 1.0 ? (CGFloat) 1.0 : m_width;

    const CGFloat dotted[] = { (CGFloat) dashUnit , (CGFloat) (dashUnit + 2.0) };
    static const CGFloat short_dashed[] = { (CGFloat) 9.0 , (CGFloat) 6.0 };
    static const CGFloat dashed[] = { (CGFloat) 19.0 , (CGFloat) 9.0 };
    static const CGFloat dotted_dashed[] = { (CGFloat) 9.0 , (CGFloat) 6.0 , (CGFloat) 3.0 , (CGFloat) 3.0 };

    switch ( pen.GetStyle() )
    {
        case wxPENSTYLE_SOLID:
            break;

        case wxPENSTYLE_DOT:
            m_count = WXSIZEOF(dotted);
            m_userLengths = new CGFloat[ m_count ] ;
            memcpy( m_userLengths, dotted, sizeof(dotted) );
            m_lengths = m_userLengths;
            break;

        case wxPENSTYLE_LONG_DASH:
            m_count = WXSIZEOF(dashed);
            m_lengths = dashed;
            break;

        case wxPENSTYLE_SHORT_DASH:
            m_count = WXSIZEOF(short_dashed);
            m_lengths = short_dashed;
            break;

        case wxPENSTYLE_DOT_DASH:
            m_count = WXSIZEOF(dotted_dashed);
            m_lengths = dotted_dashed;
            break;

        case wxPENSTYLE_USER_DASH:
            wxDash *dashes;
            m_count = pen.GetDashes( &dashes );
            if ((dashes != NULL) && (m_count > 0))
            {
                m_userLengths = new CGFloat[m_count];
                for ( int i = 0; i < m_count; ++i )
                {
                    m_userLengths[i] = dashes[i] * dashUnit;

                    if ( i % 2 == 1 && m_userLengths[i] < dashUnit + 2.0 )
                        m_userLengths[i] = (CGFloat) (dashUnit + 2.0);
                    else if ( i % 2 == 0 && m_userLengths[i] < dashUnit )
                        m_userLengths[i] = dashUnit;
                }
            }
            m_lengths = m_userLengths;
            break;

        case wxPENSTYLE_STIPPLE:
            {
                wxBitmap* bmp = pen.GetStipple();
                if ( bmp && bmp->IsOk() )
                {
                    m_colorSpace.reset( CGColorSpaceCreatePattern( NULL ) );
                    m_pattern.reset( (CGPatternRef) *( new ImagePattern( bmp , CGAffineTransformMakeScale( 1,-1 ) ) ) );
                    m_patternColorComponents = new CGFloat[1] ;
                    m_patternColorComponents[0] = (CGFloat) 1.0;
                    m_isPattern = true;
                }
            }
            break;

        default :
            {
                m_isPattern = true;
                m_colorSpace.reset( CGColorSpaceCreatePattern( wxMacGetGenericRGBColorSpace() ) );
                m_pattern.reset( (CGPatternRef) *( new HatchPattern( pen.GetStyle() , CGAffineTransformMakeScale( 1,-1 ) ) ) );
                m_patternColorComponents = new CGFloat[4] ;
                m_patternColorComponents[0] = (CGFloat) (pen.GetColour().Red() / 255.0);
                m_patternColorComponents[1] = (CGFloat) (pen.GetColour().Green() / 255.0);
                m_patternColorComponents[2] = (CGFloat) (pen.GetColour().Blue() / 255.0);
                m_patternColorComponents[3] =  (CGFloat) (pen.GetColour().Alpha() / 255.0);
            }
            break;
    }
    if ((m_lengths != NULL) && (m_count > 0))
    {
        // force the line cap, otherwise we get artifacts (overlaps) and just solid lines
        m_cap = kCGLineCapButt;
    }
}

wxMacCoreGraphicsPenData::~wxMacCoreGraphicsPenData()
{
    delete[] m_userLengths;
    delete[] m_patternColorComponents;
}

void wxMacCoreGraphicsPenData::Init()
{
    m_lengths = NULL;
    m_userLengths = NULL;
    m_width = 0;
    m_count = 0;
    m_patternColorComponents = NULL;
    m_isPattern = false;
}

void wxMacCoreGraphicsPenData::Apply( wxGraphicsContext* context )
{
    CGContextRef cg = (CGContextRef) context->GetNativeContext();
    CGContextSetLineWidth( cg , m_width );
    CGContextSetLineJoin( cg , m_join );

    CGContextSetLineDash( cg , 0 , m_lengths , m_count );
    CGContextSetLineCap( cg , m_cap );

    if ( m_isPattern )
    {
        CGAffineTransform matrix = CGContextGetCTM( cg );
        CGContextSetPatternPhase( cg, CGSizeMake(matrix.tx, matrix.ty) );
        CGContextSetStrokeColorSpace( cg , m_colorSpace );
        CGContextSetStrokePattern( cg, m_pattern , m_patternColorComponents );
    }
    else
    {
        CGContextSetStrokeColorWithColor( cg , m_color );
    }
}

//
// Brush
//

// make sure we all use one class for all conversions from wx to native colour

class wxMacCoreGraphicsColour
{
    public:
        wxMacCoreGraphicsColour();
        wxMacCoreGraphicsColour(const wxBrush &brush);
        ~wxMacCoreGraphicsColour();

        void Apply( CGContextRef cgContext );
    protected:
        void Init();
        wxCFRef<CGColorRef> m_color;
        wxCFRef<CGColorSpaceRef> m_colorSpace;

        bool m_isPattern;
        wxCFRef<CGPatternRef> m_pattern;
        CGFloat* m_patternColorComponents;
} ;

wxMacCoreGraphicsColour::~wxMacCoreGraphicsColour()
{
    delete[] m_patternColorComponents;
}

void wxMacCoreGraphicsColour::Init()
{
    m_isPattern = false;
    m_patternColorComponents = NULL;
}

void wxMacCoreGraphicsColour::Apply( CGContextRef cgContext )
{
    if ( m_isPattern )
    {
        CGAffineTransform matrix = CGContextGetCTM( cgContext );
        CGContextSetPatternPhase( cgContext, CGSizeMake(matrix.tx, matrix.ty) );
        CGContextSetFillColorSpace( cgContext , m_colorSpace );
        CGContextSetFillPattern( cgContext, m_pattern , m_patternColorComponents );
    }
    else
    {
        CGContextSetFillColorWithColor( cgContext, m_color );
    }
}

wxMacCoreGraphicsColour::wxMacCoreGraphicsColour()
{
    Init();
}

wxMacCoreGraphicsColour::wxMacCoreGraphicsColour( const wxBrush &brush )
{
    Init();
    if ( brush.GetStyle() == wxBRUSHSTYLE_SOLID )
    {
        m_color.reset( wxMacCreateCGColor( brush.GetColour() ));
    }
    else if ( brush.IsHatch() )
    {
        m_isPattern = true;
        m_colorSpace.reset( CGColorSpaceCreatePattern( wxMacGetGenericRGBColorSpace() ) );
        m_pattern.reset( (CGPatternRef) *( new HatchPattern( brush.GetStyle() , CGAffineTransformMakeScale( 1,-1 ) ) ) );

        m_patternColorComponents = new CGFloat[4] ;
        m_patternColorComponents[0] = (CGFloat) (brush.GetColour().Red() / 255.0);
        m_patternColorComponents[1] = (CGFloat) (brush.GetColour().Green() / 255.0);
        m_patternColorComponents[2] = (CGFloat) (brush.GetColour().Blue() / 255.0);
        m_patternColorComponents[3] = (CGFloat) (brush.GetColour().Alpha() / 255.0);
    }
    else
    {
        // now brush is a bitmap
        wxBitmap* bmp = brush.GetStipple();
        if ( bmp && bmp->IsOk() )
        {
            m_isPattern = true;
            m_patternColorComponents = new CGFloat[1] ;
            m_patternColorComponents[0] = (CGFloat) 1.0;
            m_colorSpace.reset( CGColorSpaceCreatePattern( NULL ) );
            m_pattern.reset( (CGPatternRef) *( new ImagePattern( bmp , CGAffineTransformMakeScale( 1,-1 ) ) ) );
        }
    }
}

class wxMacCoreGraphicsBrushData : public wxGraphicsObjectRefData
{
public:
    wxMacCoreGraphicsBrushData( wxGraphicsRenderer* renderer );
    wxMacCoreGraphicsBrushData( wxGraphicsRenderer* renderer, const wxBrush &brush );
    ~wxMacCoreGraphicsBrushData ();

    virtual void Apply( wxGraphicsContext* context );
    void CreateLinearGradientBrush(wxDouble x1, wxDouble y1,
                                   wxDouble x2, wxDouble y2,
                                   const wxGraphicsGradientStops& stops);
    void CreateRadialGradientBrush(wxDouble xo, wxDouble yo,
                                   wxDouble xc, wxDouble yc, wxDouble radius,
                                   const wxGraphicsGradientStops& stops);

    virtual bool IsShading() { return m_isShading; }
    CGShadingRef GetShading() { return m_shading; }
protected:
    CGFunctionRef CreateGradientFunction(const wxGraphicsGradientStops& stops);

    static void CalculateShadingValues (void *info, const CGFloat *in, CGFloat *out);
    virtual void Init();

    wxMacCoreGraphicsColour m_cgColor;

    bool m_isShading;
    CGFunctionRef m_gradientFunction;
    CGShadingRef m_shading;

    // information about a single gradient component
    struct GradientComponent
    {
        CGFloat pos;
        CGFloat red;
        CGFloat green;
        CGFloat blue;
        CGFloat alpha;
    };

    // and information about all of them
    struct GradientComponents
    {
        GradientComponents()
        {
            count = 0;
            comps = NULL;
        }

        void Init(unsigned count_)
        {
            count = count_;
            comps = new GradientComponent[count];
        }

        ~GradientComponents()
        {
            delete [] comps;
        }

        unsigned count;
        GradientComponent *comps;
    };

    GradientComponents m_gradientComponents;
};

wxMacCoreGraphicsBrushData::wxMacCoreGraphicsBrushData( wxGraphicsRenderer* renderer) : wxGraphicsObjectRefData( renderer )
{
    Init();
}

void
wxMacCoreGraphicsBrushData::CreateLinearGradientBrush(wxDouble x1, wxDouble y1,
                                                      wxDouble x2, wxDouble y2,
                                                      const wxGraphicsGradientStops& stops)
{
    m_gradientFunction = CreateGradientFunction(stops);
    m_shading = CGShadingCreateAxial( wxMacGetGenericRGBColorSpace(), CGPointMake((CGFloat) x1, (CGFloat) y1),
                                        CGPointMake((CGFloat) x2,(CGFloat) y2), m_gradientFunction, true, true ) ;
    m_isShading = true ;
}

void
wxMacCoreGraphicsBrushData::CreateRadialGradientBrush(wxDouble xo, wxDouble yo,
                                                      wxDouble xc, wxDouble yc,
                                                      wxDouble radius,
                                                      const wxGraphicsGradientStops& stops)
{
    m_gradientFunction = CreateGradientFunction(stops);
    m_shading = CGShadingCreateRadial( wxMacGetGenericRGBColorSpace(), CGPointMake((CGFloat) xo,(CGFloat) yo), 0,
                                        CGPointMake((CGFloat) xc,(CGFloat) yc), (CGFloat) radius, m_gradientFunction, true, true ) ;
    m_isShading = true ;
}

wxMacCoreGraphicsBrushData::wxMacCoreGraphicsBrushData(wxGraphicsRenderer* renderer, const wxBrush &brush) : wxGraphicsObjectRefData( renderer ),
    m_cgColor( brush )
{
    Init();

}

wxMacCoreGraphicsBrushData::~wxMacCoreGraphicsBrushData()
{
    if ( m_shading )
        CGShadingRelease(m_shading);

    if( m_gradientFunction )
        CGFunctionRelease(m_gradientFunction);
}

void wxMacCoreGraphicsBrushData::Init()
{
    m_gradientFunction = NULL;
    m_shading = NULL;
    m_isShading = false;
}

void wxMacCoreGraphicsBrushData::Apply( wxGraphicsContext* context )
{
    CGContextRef cg = (CGContextRef) context->GetNativeContext();

    if ( m_isShading )
    {
        // nothing to set as shades are processed by clipping using the path and filling
    }
    else
    {
        m_cgColor.Apply( cg );
    }
}

void wxMacCoreGraphicsBrushData::CalculateShadingValues (void *info, const CGFloat *in, CGFloat *out)
{
    const GradientComponents& stops = *(GradientComponents*) info ;

    CGFloat f = *in;
    if (f <= 0.0)
    {
        // Start
        out[0] = stops.comps[0].red;
        out[1] = stops.comps[0].green;
        out[2] = stops.comps[0].blue;
        out[3] = stops.comps[0].alpha;
    }
    else if (f >= 1.0)
    {
        // end
        out[0] = stops.comps[stops.count - 1].red;
        out[1] = stops.comps[stops.count - 1].green;
        out[2] = stops.comps[stops.count - 1].blue;
        out[3] = stops.comps[stops.count - 1].alpha;
    }
    else
    {
        // Find first component with position greater than f
        unsigned i;
        for ( i = 0; i < stops.count; i++ )
        {
            if (stops.comps[i].pos > f)
                break;
        }

        // Interpolated between stops
        CGFloat diff = (f - stops.comps[i-1].pos);
        CGFloat range = (stops.comps[i].pos - stops.comps[i-1].pos);
        CGFloat fact = diff / range;

        out[0] = stops.comps[i - 1].red + (stops.comps[i].red - stops.comps[i - 1].red) * fact;
        out[1] = stops.comps[i - 1].green + (stops.comps[i].green - stops.comps[i - 1].green) * fact;
        out[2] = stops.comps[i - 1].blue + (stops.comps[i].blue - stops.comps[i - 1].blue) * fact;
        out[3] = stops.comps[i - 1].alpha + (stops.comps[i].alpha - stops.comps[i - 1].alpha) * fact;
    }
}

CGFunctionRef
wxMacCoreGraphicsBrushData::CreateGradientFunction(const wxGraphicsGradientStops& stops)
{

    static const CGFunctionCallbacks callbacks = { 0, &CalculateShadingValues, NULL };
    static const CGFloat input_value_range [2] = { 0, 1 };
    static const CGFloat output_value_ranges [8] = { 0, 1, 0, 1, 0, 1, 0, 1 };

    m_gradientComponents.Init(stops.GetCount());
    for ( unsigned i = 0; i < m_gradientComponents.count; i++ )
    {
        const wxGraphicsGradientStop stop = stops.Item(i);

        m_gradientComponents.comps[i].pos = stop.GetPosition();

        const wxColour col = stop.GetColour();
        m_gradientComponents.comps[i].red = (CGFloat) (col.Red() / 255.0);
        m_gradientComponents.comps[i].green = (CGFloat) (col.Green() / 255.0);
        m_gradientComponents.comps[i].blue = (CGFloat) (col.Blue() / 255.0);
        m_gradientComponents.comps[i].alpha = (CGFloat) (col.Alpha() / 255.0);
    }

    return CGFunctionCreate ( &m_gradientComponents,  1,
                            input_value_range,
                            4,
                            output_value_ranges,
                            &callbacks);
}

//
// Font
//

#if wxOSX_USE_IPHONE

extern UIFont* CreateUIFont( const wxFont& font );
extern void DrawTextInContext( CGContextRef context, CGPoint where, UIFont *font, NSString* text );
extern CGSize MeasureTextInContext( UIFont *font, NSString* text );

#endif

class wxMacCoreGraphicsFontData : public wxGraphicsObjectRefData
{
public:
    wxMacCoreGraphicsFontData( wxGraphicsRenderer* renderer, const wxFont &font, const wxColour& col );
    ~wxMacCoreGraphicsFontData();

#if wxOSX_USE_ATSU_TEXT
    virtual ATSUStyle GetATSUStyle() { return m_macATSUIStyle; }
#endif
    CTFontRef OSXGetCTFont() const { return m_ctFont ; }
    wxColour GetColour() const { return m_colour ; }

    bool GetUnderlined() const { return m_underlined ; }
    bool GetStrikethrough() const { return m_strikethrough; }

#if wxOSX_USE_IPHONE
    UIFont* GetUIFont() const { return m_uiFont; }
#endif
private :
    wxColour m_colour;
    bool m_underlined,
         m_strikethrough;
#if wxOSX_USE_ATSU_TEXT
    ATSUStyle m_macATSUIStyle;
#endif
    wxCFRef< CTFontRef > m_ctFont;
#if wxOSX_USE_IPHONE
    UIFont*  m_uiFont;
#endif
};

wxMacCoreGraphicsFontData::wxMacCoreGraphicsFontData(wxGraphicsRenderer* renderer, const wxFont &font, const wxColour& col) : wxGraphicsObjectRefData( renderer )
{
    m_colour = col;
    m_underlined = font.GetUnderlined();
    m_strikethrough = font.GetStrikethrough();

    m_ctFont.reset( wxMacCreateCTFont( font ) );
#if wxOSX_USE_IPHONE
    m_uiFont = CreateUIFont(font);
    wxMacCocoaRetain( m_uiFont );
#endif
#if wxOSX_USE_ATSU_TEXT
    OSStatus status = noErr;
    m_macATSUIStyle = NULL;

    status = ATSUCreateAndCopyStyle( (ATSUStyle) font.MacGetATSUStyle() , &m_macATSUIStyle );

    wxASSERT_MSG( status == noErr, wxT("couldn't create ATSU style") );

    // we need the scale here ...

    Fixed atsuSize = IntToFixed( int( 1 * font.GetPointSize()) );
    RGBColor atsuColor ;
    col.GetRGBColor( &atsuColor );
    ATSUAttributeTag atsuTags[] =
    {
            kATSUSizeTag ,
            kATSUColorTag ,
    };
    ByteCount atsuSizes[WXSIZEOF(atsuTags)] =
    {
            sizeof( Fixed ) ,
            sizeof( RGBColor ) ,
    };
    ATSUAttributeValuePtr atsuValues[WXSIZEOF(atsuTags)] =
    {
            &atsuSize ,
            &atsuColor ,
    };

    status = ::ATSUSetAttributes(
        m_macATSUIStyle, WXSIZEOF(atsuTags),
        atsuTags, atsuSizes, atsuValues);

    wxASSERT_MSG( status == noErr , wxT("couldn't modify ATSU style") );
#endif
}

wxMacCoreGraphicsFontData::~wxMacCoreGraphicsFontData()
{
#if wxOSX_USE_ATSU_TEXT
    if ( m_macATSUIStyle )
    {
        ::ATSUDisposeStyle((ATSUStyle)m_macATSUIStyle);
        m_macATSUIStyle = NULL;
    }
#endif
#if wxOSX_USE_IPHONE
    wxMacCocoaRelease( m_uiFont );
#endif
}

class wxMacCoreGraphicsBitmapData : public wxGraphicsBitmapData
{
public:
    wxMacCoreGraphicsBitmapData( wxGraphicsRenderer* renderer, CGImageRef bitmap, bool monochrome );
    ~wxMacCoreGraphicsBitmapData();

    virtual CGImageRef GetBitmap() { return m_bitmap; }
    virtual void* GetNativeBitmap() const wxOVERRIDE { return m_bitmap; }
    bool IsMonochrome() { return m_monochrome; }

#if wxUSE_IMAGE
    wxImage ConvertToImage() const
    {
        return wxBitmap(m_bitmap).ConvertToImage();
    }
#endif // wxUSE_IMAGE

private :
    CGImageRef m_bitmap;
    bool m_monochrome;
};

wxMacCoreGraphicsBitmapData::wxMacCoreGraphicsBitmapData( wxGraphicsRenderer* renderer, CGImageRef bitmap, bool monochrome ) : wxGraphicsBitmapData( renderer ),
    m_bitmap(bitmap), m_monochrome(monochrome)
{
}

wxMacCoreGraphicsBitmapData::~wxMacCoreGraphicsBitmapData()
{
    CGImageRelease( m_bitmap );
}


//
// Graphics Matrix
//

//-----------------------------------------------------------------------------
// wxMacCoreGraphicsMatrix declaration
//-----------------------------------------------------------------------------

class WXDLLIMPEXP_CORE wxMacCoreGraphicsMatrixData : public wxGraphicsMatrixData
{
public :
    wxMacCoreGraphicsMatrixData(wxGraphicsRenderer* renderer) ;

    virtual ~wxMacCoreGraphicsMatrixData() ;

    virtual wxGraphicsObjectRefData *Clone() const wxOVERRIDE ;

    // concatenates the matrix
    virtual void Concat( const wxGraphicsMatrixData *t ) wxOVERRIDE;

    // sets the matrix to the respective values
    virtual void Set(wxDouble a=1.0, wxDouble b=0.0, wxDouble c=0.0, wxDouble d=1.0,
        wxDouble tx=0.0, wxDouble ty=0.0) wxOVERRIDE;

    // gets the component valuess of the matrix
    virtual void Get(wxDouble* a=NULL, wxDouble* b=NULL,  wxDouble* c=NULL,
                     wxDouble* d=NULL, wxDouble* tx=NULL, wxDouble* ty=NULL) const wxOVERRIDE;

    // makes this the inverse matrix
    virtual void Invert() wxOVERRIDE;

    // returns true if the elements of the transformation matrix are equal ?
    virtual bool IsEqual( const wxGraphicsMatrixData* t) const wxOVERRIDE ;

    // return true if this is the identity matrix
    virtual bool IsIdentity() const wxOVERRIDE;

    //
    // transformation
    //

    // add the translation to this matrix
    virtual void Translate( wxDouble dx , wxDouble dy ) wxOVERRIDE;

    // add the scale to this matrix
    virtual void Scale( wxDouble xScale , wxDouble yScale ) wxOVERRIDE;

    // add the rotation to this matrix (radians)
    virtual void Rotate( wxDouble angle ) wxOVERRIDE;

    //
    // apply the transforms
    //

    // applies that matrix to the point
    virtual void TransformPoint( wxDouble *x, wxDouble *y ) const wxOVERRIDE;

    // applies the matrix except for translations
    virtual void TransformDistance( wxDouble *dx, wxDouble *dy ) const wxOVERRIDE;

    // returns the native representation
    virtual void * GetNativeMatrix() const wxOVERRIDE;

private :
    CGAffineTransform m_matrix;
} ;

//-----------------------------------------------------------------------------
// wxMacCoreGraphicsMatrix implementation
//-----------------------------------------------------------------------------

wxMacCoreGraphicsMatrixData::wxMacCoreGraphicsMatrixData(wxGraphicsRenderer* renderer) : wxGraphicsMatrixData(renderer)
{
}

wxMacCoreGraphicsMatrixData::~wxMacCoreGraphicsMatrixData()
{
}

wxGraphicsObjectRefData *wxMacCoreGraphicsMatrixData::Clone() const
{
    wxMacCoreGraphicsMatrixData* m = new wxMacCoreGraphicsMatrixData(GetRenderer()) ;
    m->m_matrix = m_matrix ;
    return m;
}

// concatenates the matrix
void wxMacCoreGraphicsMatrixData::Concat( const wxGraphicsMatrixData *t )
{
    m_matrix = CGAffineTransformConcat(*((CGAffineTransform*) t->GetNativeMatrix()), m_matrix );
}

// sets the matrix to the respective values
void wxMacCoreGraphicsMatrixData::Set(wxDouble a, wxDouble b, wxDouble c, wxDouble d,
    wxDouble tx, wxDouble ty)
{
    m_matrix = CGAffineTransformMake((CGFloat) a,(CGFloat) b,(CGFloat) c,(CGFloat) d,(CGFloat) tx,(CGFloat) ty);
}

// gets the component valuess of the matrix
void wxMacCoreGraphicsMatrixData::Get(wxDouble* a, wxDouble* b,  wxDouble* c,
                                      wxDouble* d, wxDouble* tx, wxDouble* ty) const
{
    if (a)  *a = m_matrix.a;
    if (b)  *b = m_matrix.b;
    if (c)  *c = m_matrix.c;
    if (d)  *d = m_matrix.d;
    if (tx) *tx= m_matrix.tx;
    if (ty) *ty= m_matrix.ty;
}

// makes this the inverse matrix
void wxMacCoreGraphicsMatrixData::Invert()
{
    m_matrix = CGAffineTransformInvert( m_matrix );
}

// returns true if the elements of the transformation matrix are equal ?
bool wxMacCoreGraphicsMatrixData::IsEqual( const wxGraphicsMatrixData* t) const
{
    return CGAffineTransformEqualToTransform(m_matrix, *((CGAffineTransform*) t->GetNativeMatrix()));
}

// return true if this is the identity matrix
bool wxMacCoreGraphicsMatrixData::IsIdentity() const
{
    return ( m_matrix.a == 1 && m_matrix.d == 1 &&
        m_matrix.b == 0 && m_matrix.d == 0 && m_matrix.tx == 0 && m_matrix.ty == 0);
}

//
// transformation
//

// add the translation to this matrix
void wxMacCoreGraphicsMatrixData::Translate( wxDouble dx , wxDouble dy )
{
    m_matrix = CGAffineTransformTranslate( m_matrix, (CGFloat) dx, (CGFloat) dy);
}

// add the scale to this matrix
void wxMacCoreGraphicsMatrixData::Scale( wxDouble xScale , wxDouble yScale )
{
    m_matrix = CGAffineTransformScale( m_matrix, (CGFloat) xScale, (CGFloat) yScale);
}

// add the rotation to this matrix (radians)
void wxMacCoreGraphicsMatrixData::Rotate( wxDouble angle )
{
    m_matrix = CGAffineTransformRotate( m_matrix, (CGFloat) angle);
}

//
// apply the transforms
//

// applies that matrix to the point
void wxMacCoreGraphicsMatrixData::TransformPoint( wxDouble *x, wxDouble *y ) const
{
    CGPoint pt = CGPointApplyAffineTransform( CGPointMake((CGFloat) *x,(CGFloat) *y), m_matrix);

    *x = pt.x;
    *y = pt.y;
}

// applies the matrix except for translations
void wxMacCoreGraphicsMatrixData::TransformDistance( wxDouble *dx, wxDouble *dy ) const
{
    CGSize sz = CGSizeApplyAffineTransform( CGSizeMake((CGFloat) *dx,(CGFloat) *dy) , m_matrix );
    *dx = sz.width;
    *dy = sz.height;
}

// returns the native representation
void * wxMacCoreGraphicsMatrixData::GetNativeMatrix() const
{
    return (void*) &m_matrix;
}

//
// Graphics Path
//

//-----------------------------------------------------------------------------
// wxMacCoreGraphicsPath declaration
//-----------------------------------------------------------------------------

class WXDLLEXPORT wxMacCoreGraphicsPathData : public wxGraphicsPathData
{
public :
    wxMacCoreGraphicsPathData( wxGraphicsRenderer* renderer, CGMutablePathRef path = NULL);

    ~wxMacCoreGraphicsPathData();

    virtual wxGraphicsObjectRefData *Clone() const wxOVERRIDE;

    // begins a new subpath at (x,y)
    virtual void MoveToPoint( wxDouble x, wxDouble y ) wxOVERRIDE;

    // adds a straight line from the current point to (x,y)
    virtual void AddLineToPoint( wxDouble x, wxDouble y ) wxOVERRIDE;

    // adds a cubic Bezier curve from the current point, using two control points and an end point
    virtual void AddCurveToPoint( wxDouble cx1, wxDouble cy1, wxDouble cx2, wxDouble cy2, wxDouble x, wxDouble y ) wxOVERRIDE;

    // closes the current sub-path
    virtual void CloseSubpath() wxOVERRIDE;

    // gets the last point of the current path, (0,0) if not yet set
    virtual void GetCurrentPoint( wxDouble* x, wxDouble* y) const wxOVERRIDE;

    // adds an arc of a circle centering at (x,y) with radius (r) from startAngle to endAngle
    virtual void AddArc( wxDouble x, wxDouble y, wxDouble r, wxDouble startAngle, wxDouble endAngle, bool clockwise ) wxOVERRIDE;

    //
    // These are convenience functions which - if not available natively will be assembled
    // using the primitives from above
    //

    // adds a quadratic Bezier curve from the current point, using a control point and an end point
    virtual void AddQuadCurveToPoint( wxDouble cx, wxDouble cy, wxDouble x, wxDouble y ) wxOVERRIDE;

    // appends a rectangle as a new closed subpath
    virtual void AddRectangle( wxDouble x, wxDouble y, wxDouble w, wxDouble h ) wxOVERRIDE;

    // appends a circle as a new closed subpath
    virtual void AddCircle( wxDouble x, wxDouble y, wxDouble r ) wxOVERRIDE;

    // appends an ellipsis as a new closed subpath fitting the passed rectangle
    virtual void AddEllipse( wxDouble x, wxDouble y, wxDouble w, wxDouble h) wxOVERRIDE;

    // draws a an arc to two tangents connecting (current) to (x1,y1) and (x1,y1) to (x2,y2), also a straight line from (current) to (x1,y1)
    virtual void AddArcToPoint( wxDouble x1, wxDouble y1 , wxDouble x2, wxDouble y2, wxDouble r ) wxOVERRIDE;

    // adds another path
    virtual void AddPath( const wxGraphicsPathData* path ) wxOVERRIDE;

    // returns the native path
    virtual void * GetNativePath() const wxOVERRIDE { return m_path; }

    // give the native path returned by GetNativePath() back (there might be some deallocations necessary)
    virtual void UnGetNativePath(void *WXUNUSED(p)) const wxOVERRIDE {}

    // transforms each point of this path by the matrix
    virtual void Transform( const wxGraphicsMatrixData* matrix ) wxOVERRIDE;

    // gets the bounding box enclosing all points (possibly including control points)
    virtual void GetBox(wxDouble *x, wxDouble *y, wxDouble *w, wxDouble *h) const wxOVERRIDE;

    virtual bool Contains( wxDouble x, wxDouble y, wxPolygonFillMode fillStyle = wxODDEVEN_RULE) const wxOVERRIDE;
private :
    CGMutablePathRef m_path;
};

//-----------------------------------------------------------------------------
// wxMacCoreGraphicsPath implementation
//-----------------------------------------------------------------------------

wxMacCoreGraphicsPathData::wxMacCoreGraphicsPathData( wxGraphicsRenderer* renderer, CGMutablePathRef path) : wxGraphicsPathData(renderer)
{
    if ( path )
        m_path = path;
    else
        m_path = CGPathCreateMutable();
}

wxMacCoreGraphicsPathData::~wxMacCoreGraphicsPathData()
{
    CGPathRelease( m_path );
}

wxGraphicsObjectRefData* wxMacCoreGraphicsPathData::Clone() const
{
    wxMacCoreGraphicsPathData* clone = new wxMacCoreGraphicsPathData(GetRenderer(),CGPathCreateMutableCopy(m_path));
    return clone ;
}


// opens (starts) a new subpath
void wxMacCoreGraphicsPathData::MoveToPoint( wxDouble x1 , wxDouble y1 )
{
    CGPathMoveToPoint( m_path , NULL , (CGFloat) x1 , (CGFloat) y1 );
}

void wxMacCoreGraphicsPathData::AddLineToPoint( wxDouble x1 , wxDouble y1 )
{
    CGPathAddLineToPoint( m_path , NULL , (CGFloat) x1 , (CGFloat) y1 );
}

void wxMacCoreGraphicsPathData::AddCurveToPoint( wxDouble cx1, wxDouble cy1, wxDouble cx2, wxDouble cy2, wxDouble x, wxDouble y )
{
    CGPathAddCurveToPoint( m_path , NULL , (CGFloat) cx1 , (CGFloat) cy1 , (CGFloat) cx2, (CGFloat) cy2, (CGFloat) x , (CGFloat) y );
}

void wxMacCoreGraphicsPathData::AddQuadCurveToPoint( wxDouble cx1, wxDouble cy1, wxDouble x, wxDouble y )
{
    CGPathAddQuadCurveToPoint( m_path , NULL , (CGFloat) cx1 , (CGFloat) cy1 , (CGFloat) x , (CGFloat) y );
}

void wxMacCoreGraphicsPathData::AddRectangle( wxDouble x, wxDouble y, wxDouble w, wxDouble h )
{
    CGRect cgRect = { { (CGFloat) x , (CGFloat) y } , { (CGFloat) w , (CGFloat) h } };
    CGPathAddRect( m_path , NULL , cgRect );
}

void wxMacCoreGraphicsPathData::AddCircle( wxDouble x, wxDouble y , wxDouble r )
{
    CGPathAddEllipseInRect( m_path, NULL, CGRectMake(x-r,y-r,2*r,2*r));
}

void wxMacCoreGraphicsPathData::AddEllipse( wxDouble x, wxDouble y, wxDouble w, wxDouble h )
{
    CGPathAddEllipseInRect( m_path, NULL, CGRectMake(x,y,w,h));
}

// adds an arc of a circle centering at (x,y) with radius (r) from startAngle to endAngle
void wxMacCoreGraphicsPathData::AddArc( wxDouble x, wxDouble y, wxDouble r, wxDouble startAngle, wxDouble endAngle, bool clockwise )
{
    // inverse direction as we the 'normal' state is a y axis pointing down, ie mirrored to the standard core graphics setup
    CGPathAddArc( m_path, NULL , (CGFloat) x, (CGFloat) y, (CGFloat) r, (CGFloat) startAngle, (CGFloat) endAngle, !clockwise);
}

void wxMacCoreGraphicsPathData::AddArcToPoint( wxDouble x1, wxDouble y1 , wxDouble x2, wxDouble y2, wxDouble r )
{
    CGPathAddArcToPoint( m_path, NULL , (CGFloat) x1, (CGFloat) y1, (CGFloat) x2, (CGFloat) y2, (CGFloat) r);
}

void wxMacCoreGraphicsPathData::AddPath( const wxGraphicsPathData* path )
{
    CGPathAddPath( m_path , NULL, (CGPathRef) path->GetNativePath() );
}

// closes the current subpath
void wxMacCoreGraphicsPathData::CloseSubpath()
{
    CGPathCloseSubpath( m_path );
}

// gets the last point of the current path, (0,0) if not yet set
void wxMacCoreGraphicsPathData::GetCurrentPoint( wxDouble* x, wxDouble* y) const
{
    CGPoint p = CGPathGetCurrentPoint( m_path );
    *x = p.x;
    *y = p.y;
}

// transforms each point of this path by the matrix
void wxMacCoreGraphicsPathData::Transform( const wxGraphicsMatrixData* matrix )
{
    CGMutablePathRef p = CGPathCreateMutable() ;
    CGPathAddPath( p, (CGAffineTransform*) matrix->GetNativeMatrix() , m_path );
    CGPathRelease( m_path );
    m_path = p;
}

// gets the bounding box enclosing all points (possibly including control points)
void wxMacCoreGraphicsPathData::GetBox(wxDouble *x, wxDouble *y, wxDouble *w, wxDouble *h) const
{
    CGRect bounds = CGPathGetBoundingBox( m_path ) ;
    *x = bounds.origin.x;
    *y = bounds.origin.y;
    *w = bounds.size.width;
    *h = bounds.size.height;
}

bool wxMacCoreGraphicsPathData::Contains( wxDouble x, wxDouble y, wxPolygonFillMode fillStyle) const
{
    return CGPathContainsPoint( m_path, NULL, CGPointMake((CGFloat) x,(CGFloat) y), fillStyle == wxODDEVEN_RULE );
}

//
// Graphics Context
//

//-----------------------------------------------------------------------------
// wxMacCoreGraphicsContext declaration
//-----------------------------------------------------------------------------

class WXDLLEXPORT wxMacCoreGraphicsContext : public wxGraphicsContext
{
public:
    wxMacCoreGraphicsContext( wxGraphicsRenderer* renderer, CGContextRef cgcontext, wxDouble width = 0, wxDouble height = 0 );

    wxMacCoreGraphicsContext( wxGraphicsRenderer* renderer, wxWindow* window );

    wxMacCoreGraphicsContext( wxGraphicsRenderer* renderer);

    ~wxMacCoreGraphicsContext();

    void Init();

    virtual void StartPage( wxDouble width, wxDouble height ) wxOVERRIDE;

    virtual void EndPage() wxOVERRIDE;

    virtual void Flush() wxOVERRIDE;

    // push the current state of the context, ie the transformation matrix on a stack
    virtual void PushState() wxOVERRIDE;

    // pops a stored state from the stack
    virtual void PopState() wxOVERRIDE;

    // clips drawings to the region
    virtual void Clip( const wxRegion &region ) wxOVERRIDE;

    // clips drawings to the rect
    virtual void Clip( wxDouble x, wxDouble y, wxDouble w, wxDouble h ) wxOVERRIDE;

    // resets the clipping to original extent
    virtual void ResetClip() wxOVERRIDE;

    virtual void * GetNativeContext() wxOVERRIDE;

    virtual bool SetAntialiasMode(wxAntialiasMode antialias) wxOVERRIDE;

    virtual bool SetInterpolationQuality(wxInterpolationQuality interpolation) wxOVERRIDE;
    
    virtual bool SetCompositionMode(wxCompositionMode op) wxOVERRIDE;

    virtual void BeginLayer(wxDouble opacity) wxOVERRIDE;

    virtual void EndLayer() wxOVERRIDE;

    //
    // transformation
    //

    // translate
    virtual void Translate( wxDouble dx , wxDouble dy ) wxOVERRIDE;

    // scale
    virtual void Scale( wxDouble xScale , wxDouble yScale ) wxOVERRIDE;

    // rotate (radians)
    virtual void Rotate( wxDouble angle ) wxOVERRIDE;

    // concatenates this transform with the current transform of this context
    virtual void ConcatTransform( const wxGraphicsMatrix& matrix ) wxOVERRIDE;

    // sets the transform of this context
    virtual void SetTransform( const wxGraphicsMatrix& matrix ) wxOVERRIDE;

    // gets the matrix of this context
    virtual wxGraphicsMatrix GetTransform() const wxOVERRIDE;
    //
    // setting the paint
    //

    // strokes along a path with the current pen
    virtual void StrokePath( const wxGraphicsPath &path ) wxOVERRIDE;

    // fills a path with the current brush
    virtual void FillPath( const wxGraphicsPath &path, wxPolygonFillMode fillStyle = wxODDEVEN_RULE ) wxOVERRIDE;

    // draws a path by first filling and then stroking
    virtual void DrawPath( const wxGraphicsPath &path, wxPolygonFillMode fillStyle = wxODDEVEN_RULE ) wxOVERRIDE;

    virtual bool ShouldOffset() const wxOVERRIDE
    {
        if ( !m_enableOffset )
            return false;
        
        int penwidth = 0 ;
        if ( !m_pen.IsNull() )
        {
            penwidth = (int)((wxMacCoreGraphicsPenData*)m_pen.GetRefData())->GetWidth();
            if ( penwidth == 0 )
                penwidth = 1;
        }
        return ( penwidth % 2 ) == 1;
    }
    //
    // text
    //

    virtual void GetTextExtent( const wxString &text, wxDouble *width, wxDouble *height,
        wxDouble *descent, wxDouble *externalLeading ) const wxOVERRIDE;

    virtual void GetPartialTextExtents(const wxString& text, wxArrayDouble& widths) const wxOVERRIDE;

    //
    // image support
    //

    virtual void DrawBitmap( const wxBitmap &bmp, wxDouble x, wxDouble y, wxDouble w, wxDouble h ) wxOVERRIDE;

    virtual void DrawBitmap( const wxGraphicsBitmap &bmp, wxDouble x, wxDouble y, wxDouble w, wxDouble h ) wxOVERRIDE;

    virtual void DrawIcon( const wxIcon &icon, wxDouble x, wxDouble y, wxDouble w, wxDouble h ) wxOVERRIDE;
    
    // fast convenience methods
    
    
    virtual void DrawRectangle( wxDouble x, wxDouble y, wxDouble w, wxDouble h ) wxOVERRIDE;

    void SetNativeContext( CGContextRef cg );

    wxDECLARE_NO_COPY_CLASS(wxMacCoreGraphicsContext);

private:
    bool EnsureIsValid();
    void CheckInvariants() const;

    virtual void DoDrawText( const wxString &str, wxDouble x, wxDouble y ) wxOVERRIDE;
    virtual void DoDrawRotatedText( const wxString &str, wxDouble x, wxDouble y, wxDouble angle ) wxOVERRIDE;

    CGContextRef m_cgContext;
    WXWidget m_view;
    bool m_contextSynthesized;
    CGAffineTransform m_windowTransform;
    bool m_invisible;

#if wxOSX_USE_COCOA_OR_CARBON
    wxCFRef<HIShapeRef> m_clipRgn;
#endif
};

//-----------------------------------------------------------------------------
// device context implementation
//
// more and more of the dc functionality should be implemented by calling
// the appropricate wxMacCoreGraphicsContext, but we will have to do that step by step
// also coordinate conversions should be moved to native matrix ops
//-----------------------------------------------------------------------------

// we always stock two context states, one at entry, to be able to preserve the
// state we were called with, the other one after changing to HI Graphics orientation
// (this one is used for getting back clippings etc)

//-----------------------------------------------------------------------------
// wxMacCoreGraphicsContext implementation
//-----------------------------------------------------------------------------

class wxQuartzOffsetHelper
{
public :
    wxQuartzOffsetHelper( CGContextRef cg , bool offset )
    {
        m_cg = cg;
        m_offset = offset;
        if ( m_offset )
        {
            m_userOffset = CGContextConvertSizeToUserSpace( m_cg, CGSizeMake( 0.5 , 0.5 ) );
            CGContextTranslateCTM( m_cg, m_userOffset.width , m_userOffset.height );
        }
        else
        {
            m_userOffset = CGSizeMake(0.0, 0.0);
        }

    }
    ~wxQuartzOffsetHelper( )
    {
        if ( m_offset )
            CGContextTranslateCTM( m_cg, -m_userOffset.width , -m_userOffset.height );
    }
public :
    CGSize m_userOffset;
    CGContextRef m_cg;
    bool m_offset;
} ;

void wxMacCoreGraphicsContext::Init()
{
    m_cgContext = NULL;
    m_contextSynthesized = false;
    m_width = 0;
    m_height = 0;
#if wxOSX_USE_COCOA_OR_IPHONE
    m_view = NULL;
#endif
    m_invisible = false;
    m_antialias = wxANTIALIAS_DEFAULT;
    m_interpolation = wxINTERPOLATION_DEFAULT;
}

wxMacCoreGraphicsContext::wxMacCoreGraphicsContext( wxGraphicsRenderer* renderer, CGContextRef cgcontext, wxDouble width, wxDouble height ) : wxGraphicsContext(renderer)
{
    Init();
    SetNativeContext(cgcontext);
    m_width = width;
    m_height = height;
}

wxMacCoreGraphicsContext::wxMacCoreGraphicsContext( wxGraphicsRenderer* renderer, wxWindow* window ): wxGraphicsContext(renderer)
{
    Init();

    m_enableOffset = window->GetContentScaleFactor() <= 1;
    wxSize sz = window->GetSize();
    m_width = sz.x;
    m_height = sz.y;

#if wxOSX_USE_COCOA_OR_IPHONE
    m_view = window->GetHandle();

#if wxOSX_USE_COCOA
    if ( ! window->GetPeer()->IsFlipped() )
    {
        m_windowTransform = CGAffineTransformMakeTranslation( 0 , m_height );
        m_windowTransform = CGAffineTransformScale( m_windowTransform , 1 , -1 );
    }
    else
#endif
    {
        m_windowTransform = CGAffineTransformIdentity;
    }
#else
    int originX , originY;
    originX = originY = 0;
    Rect bounds = { 0,0,0,0 };
    m_windowRef = (WindowRef) window->MacGetTopLevelWindowRef();
    window->MacWindowToRootWindow( &originX , &originY );
    GetWindowBounds( m_windowRef, kWindowContentRgn, &bounds );
    m_windowTransform = CGAffineTransformMakeTranslation( 0 , bounds.bottom - bounds.top );
    m_windowTransform = CGAffineTransformScale( m_windowTransform , 1 , -1 );
    m_windowTransform = CGAffineTransformTranslate( m_windowTransform, originX, originY ) ;
#endif
}

wxMacCoreGraphicsContext::wxMacCoreGraphicsContext(wxGraphicsRenderer* renderer) : wxGraphicsContext(renderer)
{
    Init();
}

wxMacCoreGraphicsContext::~wxMacCoreGraphicsContext()
{
    SetNativeContext(NULL);
}


void wxMacCoreGraphicsContext::CheckInvariants() const
{
    // check invariants here for debugging ...
}



void wxMacCoreGraphicsContext::StartPage( wxDouble width, wxDouble height )
{
    CGRect r;
    if ( width != 0 && height != 0)
        r = CGRectMake( (CGFloat) 0.0 , (CGFloat) 0.0 , (CGFloat) width  , (CGFloat) height );
    else
        r = CGRectMake( (CGFloat) 0.0 , (CGFloat) 0.0 , (CGFloat) m_width  , (CGFloat) m_height );

    CGContextBeginPage(m_cgContext,  &r );
//    CGContextTranslateCTM( m_cgContext , 0 ,  height == 0 ? m_height : height );
//    CGContextScaleCTM( m_cgContext , 1 , -1 );
}

void wxMacCoreGraphicsContext::EndPage()
{
    CGContextEndPage(m_cgContext);
}

void wxMacCoreGraphicsContext::Flush()
{
    CGContextFlush(m_cgContext);
}

bool wxMacCoreGraphicsContext::EnsureIsValid()
{
    CheckInvariants();
    
    if ( !m_cgContext )
    {
        if (m_invisible)
            return false;

#if wxOSX_USE_COCOA
        if ( wxOSXLockFocus(m_view) )
        {
            m_cgContext = wxOSXGetContextFromCurrentContext();
            wxASSERT_MSG( m_cgContext != NULL, wxT("Unable to retrieve drawing context from View"));
        }
        else
        {
            m_invisible = true;
        }
#endif
#if wxOSX_USE_IPHONE
        m_cgContext = wxOSXGetContextFromCurrentContext();
        if ( m_cgContext == NULL )
        {
            m_invisible = true;
        }
#endif
        if ( m_cgContext )
        {
            CGContextSaveGState( m_cgContext );
#if wxOSX_USE_COCOA_OR_CARBON
            if ( m_clipRgn.get() )
            {
                wxCFRef<HIMutableShapeRef> hishape( HIShapeCreateMutableCopy( m_clipRgn ) );
                // if the shape is empty, HIShapeReplacePathInCGContext doesn't work
                if ( HIShapeIsEmpty(hishape))
                {
                    CGRect empty = CGRectMake( 0,0,0,0 );
                    CGContextClipToRect( m_cgContext, empty );
                }
                else
                {
                    HIShapeReplacePathInCGContext( hishape, m_cgContext );
                    CGContextClip( m_cgContext );
                }
            }
#endif
            CGContextConcatCTM( m_cgContext, m_windowTransform );
            CGContextSetTextMatrix( m_cgContext, CGAffineTransformIdentity );
            m_contextSynthesized = true;
            CGContextSaveGState( m_cgContext );

#if 0 // turn on for debugging of clientdc
            static float color = 0.5 ;
            static int channel = 0 ;
            CGRect bounds = CGRectMake(-1000,-1000,2000,2000);
            CGContextSetRGBFillColor( m_cgContext, channel == 0 ? color : 0.5 ,
                channel == 1 ? color : 0.5 , channel == 2 ? color : 0.5 , 1 );
            CGContextFillRect( m_cgContext, bounds );
            color += 0.1 ;
            if ( color > 0.9 )
            {
                color = 0.5 ;
                channel++ ;
                if ( channel == 3 )
                    channel = 0 ;
            }
#endif
        }
    }
    CheckInvariants();

    return m_cgContext != NULL;
}

bool wxMacCoreGraphicsContext::SetAntialiasMode(wxAntialiasMode antialias)
{
    if (!EnsureIsValid())
        return true;

    if (m_antialias == antialias)
        return true;

    m_antialias = antialias;

    bool antialiasMode;
    switch (antialias)
    {
        case wxANTIALIAS_DEFAULT:
            antialiasMode = true;
            break;
        case wxANTIALIAS_NONE:
            antialiasMode = false;
            break;
        default:
            return false;
    }
    CGContextSetShouldAntialias(m_cgContext, antialiasMode);
    CheckInvariants();
    return true;
}

bool wxMacCoreGraphicsContext::SetInterpolationQuality(wxInterpolationQuality interpolation)
{
    if (!EnsureIsValid())
        return true;
    
    if (m_interpolation == interpolation)
        return true;

    m_interpolation = interpolation;
    CGInterpolationQuality quality;
    
    switch (interpolation) 
    {
        case wxINTERPOLATION_DEFAULT:
            quality = kCGInterpolationDefault;
            break;
        case wxINTERPOLATION_NONE:
            quality = kCGInterpolationNone;
            break;
        case wxINTERPOLATION_FAST:
            quality = kCGInterpolationLow;
            break;
        case wxINTERPOLATION_GOOD:
            quality = kCGInterpolationMedium;
            break;
        case wxINTERPOLATION_BEST:
            quality = kCGInterpolationHigh;
            break;
        default:
            return false;
    }
    CGContextSetInterpolationQuality(m_cgContext, quality);
    CheckInvariants();
    return true;
}

bool wxMacCoreGraphicsContext::SetCompositionMode(wxCompositionMode op)
{
    if (!EnsureIsValid())
        return true;

    if ( m_composition == op )
        return true;

    m_composition = op;

    if (m_composition == wxCOMPOSITION_DEST)
        return true;

    // TODO REMOVE if we don't need it because of bugs in 10.5
#if 0
    {
        CGCompositeOperation cop = kCGCompositeOperationSourceOver;
        CGBlendMode mode = kCGBlendModeNormal;
        switch( op )
        {
        case wxCOMPOSITION_CLEAR:
            cop = kCGCompositeOperationClear;
            break;
        case wxCOMPOSITION_SOURCE:
            cop = kCGCompositeOperationCopy;
            break;
        case wxCOMPOSITION_OVER:
            mode = kCGBlendModeNormal;
            break;
        case wxCOMPOSITION_IN:
            cop = kCGCompositeOperationSourceIn;
            break;
        case wxCOMPOSITION_OUT:
            cop = kCGCompositeOperationSourceOut;
            break;
        case wxCOMPOSITION_ATOP:
            cop = kCGCompositeOperationSourceAtop;
            break;
        case wxCOMPOSITION_DEST_OVER:
            cop = kCGCompositeOperationDestinationOver;
            break;
        case wxCOMPOSITION_DEST_IN:
            cop = kCGCompositeOperationDestinationIn;
            break;
        case wxCOMPOSITION_DEST_OUT:
            cop = kCGCompositeOperationDestinationOut;
            break;
        case wxCOMPOSITION_DEST_ATOP:
           cop = kCGCompositeOperationDestinationAtop;
            break;
        case wxCOMPOSITION_XOR:
            cop = kCGCompositeOperationXOR;
            break;
        case wxCOMPOSITION_ADD:
            mode = kCGBlendModePlusLighter ;
            break;
        default:
            return false;
        }
        if ( cop != kCGCompositeOperationSourceOver )
            CGContextSetCompositeOperation(m_cgContext, cop);
        else
            CGContextSetBlendMode(m_cgContext, mode);
    }
#endif
    {
        CGBlendMode mode = kCGBlendModeNormal;
        switch( op )
        {
        case wxCOMPOSITION_CLEAR:
            mode = kCGBlendModeClear;
            break;
        case wxCOMPOSITION_SOURCE:
            mode = kCGBlendModeCopy;
            break;
        case wxCOMPOSITION_OVER:
            mode = kCGBlendModeNormal;
            break;
        case wxCOMPOSITION_IN:
            mode = kCGBlendModeSourceIn;
            break;
        case wxCOMPOSITION_OUT:
            mode = kCGBlendModeSourceOut;
            break;
        case wxCOMPOSITION_ATOP:
            mode = kCGBlendModeSourceAtop;
            break;
        case wxCOMPOSITION_DEST_OVER:
            mode = kCGBlendModeDestinationOver;
            break;
        case wxCOMPOSITION_DEST_IN:
            mode = kCGBlendModeDestinationIn;
            break;
        case wxCOMPOSITION_DEST_OUT:
            mode = kCGBlendModeDestinationOut;
            break;
        case wxCOMPOSITION_DEST_ATOP:
           mode = kCGBlendModeDestinationAtop;
            break;
        case wxCOMPOSITION_XOR:
            mode = kCGBlendModeExclusion; // Not kCGBlendModeXOR!
            break;

        case wxCOMPOSITION_ADD:
            mode = kCGBlendModePlusLighter ;
            break;
        default:
            return false;
        }
        CGContextSetBlendMode(m_cgContext, mode);
    }

    CheckInvariants();
    return true;
}

void wxMacCoreGraphicsContext::BeginLayer(wxDouble opacity)
{
    CheckInvariants();
    CGContextSaveGState(m_cgContext);
    CGContextSetAlpha(m_cgContext, (CGFloat) opacity);
    CGContextBeginTransparencyLayer(m_cgContext, 0);
    CheckInvariants();
}

void wxMacCoreGraphicsContext::EndLayer()
{
    CheckInvariants();
    CGContextEndTransparencyLayer(m_cgContext);
    CGContextRestoreGState(m_cgContext);
    CheckInvariants();
}

void wxMacCoreGraphicsContext::Clip( const wxRegion &region )
{
    CheckInvariants();
#if wxOSX_USE_COCOA_OR_CARBON
    if( m_cgContext )
    {
        wxCFRef<HIShapeRef> shape = wxCFRefFromGet(region.GetWXHRGN());
        // if the shape is empty, HIShapeReplacePathInCGContext doesn't work
        if ( HIShapeIsEmpty(shape))
        {
            CGRect empty = CGRectMake( 0,0,0,0 );
            CGContextClipToRect( m_cgContext, empty );
        }
        else
        {
            HIShapeReplacePathInCGContext( shape, m_cgContext );
            CGContextClip( m_cgContext );
        }
    }
    else
    {
        // this offsetting to device coords is not really correct, but since we cannot apply affine transforms
        // to regions we try at least to have correct translations
        HIMutableShapeRef mutableShape = HIShapeCreateMutableCopy( region.GetWXHRGN() );

        CGPoint transformedOrigin = CGPointApplyAffineTransform( CGPointZero, m_windowTransform );
        HIShapeOffset( mutableShape, transformedOrigin.x, transformedOrigin.y );
        m_clipRgn.reset(mutableShape);
    }
#else
    // allow usage as measuring context
    // wxASSERT_MSG( m_cgContext != NULL, "Needs a valid context for clipping" );
#endif
    CheckInvariants();    
}

// clips drawings to the rect
void wxMacCoreGraphicsContext::Clip( wxDouble x, wxDouble y, wxDouble w, wxDouble h )
{
    CheckInvariants();
    CGRect r = CGRectMake( (CGFloat) x , (CGFloat) y , (CGFloat) w , (CGFloat) h );
    if ( m_cgContext )
    {
        CGContextClipToRect( m_cgContext, r );
    }
    else
    {
#if wxOSX_USE_COCOA_OR_CARBON
        // the clipping itself must be stored as device coordinates, otherwise
        // we cannot apply it back correctly
        r.origin= CGPointApplyAffineTransform( r.origin, m_windowTransform );
        r.size= CGSizeApplyAffineTransform(r.size, m_windowTransform);
        m_clipRgn.reset(HIShapeCreateWithRect(&r));
#else
    // allow usage as measuring context
    // wxFAIL_MSG( "Needs a valid context for clipping" );
#endif
    }
    CheckInvariants();    
}

    // resets the clipping to original extent
void wxMacCoreGraphicsContext::ResetClip()
{
    if ( m_cgContext )
    {
        // there is no way for clearing the clip, we can only revert to the stored
        // state, but then we have to make sure everything else is NOT restored
        CGAffineTransform transform = CGContextGetCTM( m_cgContext );
        CGContextRestoreGState( m_cgContext );
        CGContextSaveGState( m_cgContext );
        CGAffineTransform transformNew = CGContextGetCTM( m_cgContext );
        transformNew = CGAffineTransformInvert( transformNew ) ;
        CGContextConcatCTM( m_cgContext, transformNew);
        CGContextConcatCTM( m_cgContext, transform);
    }
    else
    {
#if wxOSX_USE_COCOA_OR_CARBON
        m_clipRgn.reset();
#else
    // allow usage as measuring context
    // wxFAIL_MSG( "Needs a valid context for clipping" );
#endif
    }
    CheckInvariants();    
}

void wxMacCoreGraphicsContext::StrokePath( const wxGraphicsPath &path )
{
    if ( m_pen.IsNull() )
        return ;

    if (!EnsureIsValid())
        return;

    if (m_composition == wxCOMPOSITION_DEST)
        return;

    wxQuartzOffsetHelper helper( m_cgContext , ShouldOffset() );

    ((wxMacCoreGraphicsPenData*)m_pen.GetRefData())->Apply(this);
    CGContextAddPath( m_cgContext , (CGPathRef) path.GetNativePath() );
    CGContextStrokePath( m_cgContext );
    
    CheckInvariants();
}

void wxMacCoreGraphicsContext::DrawPath( const wxGraphicsPath &path , wxPolygonFillMode fillStyle )
{
    if (!EnsureIsValid())
        return;

    if (m_composition == wxCOMPOSITION_DEST)
        return;

    if ( !m_brush.IsNull() && ((wxMacCoreGraphicsBrushData*)m_brush.GetRefData())->IsShading() )
    {
        // when using shading, we cannot draw pen and brush at the same time
        // revert to the base implementation of first filling and then stroking
        wxGraphicsContext::DrawPath( path, fillStyle );
        return;
    }

    CGPathDrawingMode mode = kCGPathFill ;
    if ( m_brush.IsNull() )
    {
        if ( m_pen.IsNull() )
            return;
        else
            mode = kCGPathStroke;
    }
    else
    {
        if ( m_pen.IsNull() )
        {
            if ( fillStyle == wxODDEVEN_RULE )
                mode = kCGPathEOFill;
            else
                mode = kCGPathFill;
        }
        else
        {
            if ( fillStyle == wxODDEVEN_RULE )
                mode = kCGPathEOFillStroke;
            else
                mode = kCGPathFillStroke;
        }
    }

    if ( !m_brush.IsNull() )
        ((wxMacCoreGraphicsBrushData*)m_brush.GetRefData())->Apply(this);
    if ( !m_pen.IsNull() )
        ((wxMacCoreGraphicsPenData*)m_pen.GetRefData())->Apply(this);

    wxQuartzOffsetHelper helper( m_cgContext , ShouldOffset() );

    CGContextAddPath( m_cgContext , (CGPathRef) path.GetNativePath() );
    CGContextDrawPath( m_cgContext , mode );
    
    CheckInvariants();
}

void wxMacCoreGraphicsContext::FillPath( const wxGraphicsPath &path , wxPolygonFillMode fillStyle )
{
    if ( m_brush.IsNull() )
        return;

    if (!EnsureIsValid())
        return;

    if (m_composition == wxCOMPOSITION_DEST)
        return;

    if ( ((wxMacCoreGraphicsBrushData*)m_brush.GetRefData())->IsShading() )
    {
        CGContextSaveGState( m_cgContext );
        CGContextAddPath( m_cgContext , (CGPathRef) path.GetNativePath() );
        CGContextClip( m_cgContext );
        CGContextDrawShading( m_cgContext, ((wxMacCoreGraphicsBrushData*)m_brush.GetRefData())->GetShading() );
        CGContextRestoreGState( m_cgContext);
    }
    else
    {
        ((wxMacCoreGraphicsBrushData*)m_brush.GetRefData())->Apply(this);
        CGContextAddPath( m_cgContext , (CGPathRef) path.GetNativePath() );
        if ( fillStyle == wxODDEVEN_RULE )
            CGContextEOFillPath( m_cgContext );
        else
            CGContextFillPath( m_cgContext );
    }
    
    CheckInvariants();
}

void wxMacCoreGraphicsContext::SetNativeContext( CGContextRef cg )
{
    // we allow either setting or clearing but not replacing
    wxASSERT( m_cgContext == NULL || cg == NULL );

    if ( m_cgContext )
    {
        CheckInvariants();
        CGContextRestoreGState( m_cgContext );
        CGContextRestoreGState( m_cgContext );
        if ( m_contextSynthesized )
        {
#if wxOSX_USE_COCOA
            wxOSXUnlockFocus(m_view);
#endif
        }
        else
            CGContextRelease(m_cgContext);
    }

    m_cgContext = cg;

    // FIXME: This check is needed because currently we need to use a DC/GraphicsContext
    // in order to get font properties, like wxFont::GetPixelSize, but since we don't have
    // a native window attached to use, I create a wxGraphicsContext with a NULL CGContextRef
    // for this one operation.

    // When wxFont::GetPixelSize on Mac no longer needs a graphics context, this check
    // can be removed.
    if (m_cgContext)
    {
        CGContextRetain(m_cgContext);
        CGContextSaveGState( m_cgContext );
        CGContextSetTextMatrix( m_cgContext, CGAffineTransformIdentity );
        CGContextSaveGState( m_cgContext );
        m_contextSynthesized = false;
    }
}

void wxMacCoreGraphicsContext::Translate( wxDouble dx , wxDouble dy )
{
    if ( m_cgContext )
        CGContextTranslateCTM( m_cgContext, (CGFloat) dx, (CGFloat) dy );
    else
        m_windowTransform = CGAffineTransformTranslate(m_windowTransform, (CGFloat) dx, (CGFloat) dy);
}

void wxMacCoreGraphicsContext::Scale( wxDouble xScale , wxDouble yScale )
{
    if ( m_cgContext )
        CGContextScaleCTM( m_cgContext , (CGFloat) xScale , (CGFloat) yScale );
    else
        m_windowTransform = CGAffineTransformScale(m_windowTransform, (CGFloat) xScale, (CGFloat) yScale);
}

void wxMacCoreGraphicsContext::Rotate( wxDouble angle )
{
    if ( m_cgContext )
        CGContextRotateCTM( m_cgContext , (CGFloat) angle );
    else
        m_windowTransform = CGAffineTransformRotate(m_windowTransform, (CGFloat) angle);
}

void wxMacCoreGraphicsContext::DrawBitmap( const wxBitmap &bmp, wxDouble x, wxDouble y, wxDouble w, wxDouble h )
{
    wxGraphicsBitmap bitmap = GetRenderer()->CreateBitmap(bmp);
    DrawBitmap(bitmap, x, y, w, h);
}

void wxMacCoreGraphicsContext::DrawBitmap( const wxGraphicsBitmap &bmp, wxDouble x, wxDouble y, wxDouble w, wxDouble h )
{
    if (!EnsureIsValid())
        return;

    if (m_composition == wxCOMPOSITION_DEST)
        return;

#ifdef __WXMAC__
    wxMacCoreGraphicsBitmapData* refdata = static_cast<wxMacCoreGraphicsBitmapData*>(bmp.GetRefData());
    CGImageRef image = refdata->GetBitmap();
    CGRect r = CGRectMake( (CGFloat) x , (CGFloat) y , (CGFloat) w , (CGFloat) h );
    if ( refdata->IsMonochrome() == 1 )
    {
        // is a mask, the '1' in the mask tell where to draw the current brush
        if (  !m_brush.IsNull() )
        {
            if ( ((wxMacCoreGraphicsBrushData*)m_brush.GetRefData())->IsShading() )
            {
                // TODO clip to mask
            /*
                CGContextSaveGState( m_cgContext );
                CGContextAddPath( m_cgContext , (CGPathRef) path.GetNativePath() );
                CGContextClip( m_cgContext );
                CGContextDrawShading( m_cgContext, ((wxMacCoreGraphicsBrushData*)m_brush.GetRefData())->GetShading() );
                CGContextRestoreGState( m_cgContext);
            */
            }
            else
            {
                ((wxMacCoreGraphicsBrushData*)m_brush.GetRefData())->Apply(this);
                wxMacDrawCGImage( m_cgContext , &r , image );
            }
        }
    }
    else
    {
        wxMacDrawCGImage( m_cgContext , &r , image );
    }
#endif
    
    CheckInvariants();
}

void wxMacCoreGraphicsContext::DrawIcon( const wxIcon &icon, wxDouble x, wxDouble y, wxDouble w, wxDouble h )
{
    if (!EnsureIsValid())
        return;

    if (m_composition == wxCOMPOSITION_DEST)
        return;

#if wxOSX_USE_COCOA
    {
        CGRect r = CGRectMake( (CGFloat) x , (CGFloat) y , (CGFloat) w , (CGFloat) h );
        const WX_NSImage nsImage = icon.GetNSImage();

        CGImageRef cgImage = wxOSXGetCGImageFromNSImage( nsImage , &r, m_cgContext );
        wxMacDrawCGImage( m_cgContext, &r, cgImage);
    }
#endif
    
    CheckInvariants();
}

void wxMacCoreGraphicsContext::PushState()
{
    if (!EnsureIsValid())
        return;

    CGContextSaveGState( m_cgContext );
}

void wxMacCoreGraphicsContext::PopState()
{
    if (!EnsureIsValid())
        return;

    CGContextRestoreGState( m_cgContext );
}

void wxMacCoreGraphicsContext::DoDrawText( const wxString &str, wxDouble x, wxDouble y )
{
    wxCHECK_RET( !m_font.IsNull(), wxT("wxMacCoreGraphicsContext::DrawText - no valid font set") );

    if (!EnsureIsValid())
        return;

    if (m_composition == wxCOMPOSITION_DEST)
        return;

    wxMacCoreGraphicsFontData* fref = (wxMacCoreGraphicsFontData*)m_font.GetRefData();
    wxCFStringRef text(str, wxLocale::GetSystemEncoding() );
    CTFontRef font = fref->OSXGetCTFont();
    CGColorRef col = wxMacCreateCGColor( fref->GetColour() );
#if 0
    // right now there's no way to get continuous underlines, only words, so we emulate it
    CTUnderlineStyle ustyle = fref->GetUnderlined() ? kCTUnderlineStyleSingle : kCTUnderlineStyleNone ;
    wxCFRef<CFNumberRef> underlined( CFNumberCreate(NULL, kCFNumberSInt32Type, &ustyle) );
     CFStringRef keys[] = { kCTFontAttributeName , kCTForegroundColorAttributeName, kCTUnderlineStyleAttributeName };
    CFTypeRef values[] = { font, col, underlined };
#else
    CFStringRef keys[] = { kCTFontAttributeName , kCTForegroundColorAttributeName };
    CFTypeRef values[] = { font, col };
#endif
    wxCFRef<CFDictionaryRef> attributes( CFDictionaryCreate(kCFAllocatorDefault, (const void**) &keys, (const void**) &values,
                                                    WXSIZEOF( keys ), &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks) );
    wxCFRef<CFAttributedStringRef> attrtext( CFAttributedStringCreate(kCFAllocatorDefault, text, attributes) );
    wxCFRef<CTLineRef> line( CTLineCreateWithAttributedString(attrtext) );

    y += CTFontGetAscent(font);

    CGContextSaveGState(m_cgContext);
    CGAffineTransform textMatrix = CGContextGetTextMatrix(m_cgContext);

    CGContextTranslateCTM(m_cgContext, (CGFloat) x, (CGFloat) y);
    CGContextScaleCTM(m_cgContext, 1, -1);
    CGContextSetTextMatrix(m_cgContext, CGAffineTransformIdentity);

    CTLineDraw( line, m_cgContext );

    if ( fref->GetUnderlined() ) {
        //AKT: draw horizontal line 1 pixel thick and with 1 pixel gap under baseline
        CGFloat width = CTLineGetTypographicBounds(line, NULL, NULL, NULL);

        CGPoint points[] = { {0.0, -2.0},  {width, -2.0} };
        
        CGContextSetStrokeColorWithColor(m_cgContext, col);
        CGContextSetShouldAntialias(m_cgContext, false);
        CGContextSetLineWidth(m_cgContext, 1.0);
        CGContextStrokeLineSegments(m_cgContext, points, 2);
    }
    if ( fref->GetStrikethrough() )
    {
        CGFloat width = CTLineGetTypographicBounds(line, NULL, NULL, NULL);
        CGFloat height = CTFontGetXHeight( font );
        CGPoint points[] = { {0.0, height * 0.6f},  {width, height * 0.6f} };
        CGContextSetStrokeColorWithColor(m_cgContext, col);
        CGContextSetShouldAntialias(m_cgContext, false);
        CGContextSetLineWidth(m_cgContext, 1.0);
        CGContextStrokeLineSegments(m_cgContext, points, 2);
    }

    CGContextRestoreGState(m_cgContext);
    CGContextSetTextMatrix(m_cgContext, textMatrix);
    CGColorRelease( col );
    CheckInvariants();
}

void wxMacCoreGraphicsContext::DoDrawRotatedText(const wxString &str,
                                                 wxDouble x, wxDouble y,
                                                 wxDouble angle)
{
    wxCHECK_RET( !m_font.IsNull(), wxT("wxMacCoreGraphicsContext::DrawText - no valid font set") );

    if (!EnsureIsValid())
        return;

    if (m_composition == wxCOMPOSITION_DEST)
        return;

    // default implementation takes care of rotation and calls non rotated DrawText afterwards
    wxGraphicsContext::DoDrawRotatedText( str, x, y, angle );

    CheckInvariants();
}

void wxMacCoreGraphicsContext::GetTextExtent( const wxString &str, wxDouble *width, wxDouble *height,
                            wxDouble *descent, wxDouble *externalLeading ) const
{
    wxCHECK_RET( !m_font.IsNull(), wxT("wxMacCoreGraphicsContext::GetTextExtent - no valid font set") );

    if ( width )
        *width = 0;
    if ( height )
        *height = 0;
    if ( descent )
        *descent = 0;
    if ( externalLeading )
        *externalLeading = 0;

    // In wxWidgets (MSW-inspired) API it is possible to call GetTextExtent()
    // with an empty string to get just the descent and the leading of the
    // font, so support this (mis)use.
    wxString strToMeasure(str);
    if (str.empty())
        strToMeasure = wxS(" ");

    wxMacCoreGraphicsFontData* fref = (wxMacCoreGraphicsFontData*)m_font.GetRefData();
    CTFontRef font = fref->OSXGetCTFont();

    wxCFStringRef text(strToMeasure, wxLocale::GetSystemEncoding() );
    CFStringRef keys[] = { kCTFontAttributeName  };
    CFTypeRef values[] = { font };
    wxCFRef<CFDictionaryRef> attributes( CFDictionaryCreate(kCFAllocatorDefault, (const void**) &keys, (const void**) &values,
                                                            WXSIZEOF( keys ), &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks) );
    wxCFRef<CFAttributedStringRef> attrtext( CFAttributedStringCreate(kCFAllocatorDefault, text, attributes) );
    wxCFRef<CTLineRef> line( CTLineCreateWithAttributedString(attrtext) );

    CGFloat a, d, l, w;
    w = CTLineGetTypographicBounds(line, &a, &d, &l);

    if ( !str.empty() )
    {
        if ( width )
            *width = w;
        if ( height )
            *height = a+d+l;
    }

    if ( descent )
        *descent = d;
    if ( externalLeading )
        *externalLeading = l;

    CheckInvariants();    
}

void wxMacCoreGraphicsContext::GetPartialTextExtents(const wxString& text, wxArrayDouble& widths) const
{
    widths.Empty();
    widths.Add(0, text.length());

    wxCHECK_RET( !m_font.IsNull(), wxT("wxMacCoreGraphicsContext::DrawText - no valid font set") );

    if (text.empty())
        return;

    wxMacCoreGraphicsFontData* fref = (wxMacCoreGraphicsFontData*)m_font.GetRefData();
    CTFontRef font = fref->OSXGetCTFont();

    wxCFStringRef t(text, wxLocale::GetSystemEncoding() );
    CFStringRef keys[] = { kCTFontAttributeName  };
    CFTypeRef values[] = { font };
    wxCFRef<CFDictionaryRef> attributes( CFDictionaryCreate(kCFAllocatorDefault, (const void**) &keys, (const void**) &values,
                                                            WXSIZEOF( keys ), &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks) );
    wxCFRef<CFAttributedStringRef> attrtext( CFAttributedStringCreate(kCFAllocatorDefault, t, attributes) );
    wxCFRef<CTLineRef> line( CTLineCreateWithAttributedString(attrtext) );

    int chars = text.length();
    for ( int pos = 0; pos < (int)chars; pos ++ )
    {
        widths[pos] = CTLineGetOffsetForStringIndex( line, pos+1 , NULL );
    }

    CheckInvariants();
}

void * wxMacCoreGraphicsContext::GetNativeContext()
{
    return m_cgContext;
}


void wxMacCoreGraphicsContext::DrawRectangle( wxDouble x, wxDouble y, wxDouble w, wxDouble h )
{
    if (!EnsureIsValid())
        return;

    if (m_composition == wxCOMPOSITION_DEST) 
        return; 

    // when using shading, we have to go back to drawing paths 
    if ( !m_brush.IsNull() && ((wxMacCoreGraphicsBrushData*)m_brush.GetRefData())->IsShading() )
    {
        wxGraphicsContext::DrawRectangle( x,y,w,h );
        return;
    }

    CGRect rect = CGRectMake( (CGFloat) x , (CGFloat) y , (CGFloat) w , (CGFloat) h );
    if ( !m_brush.IsNull() )
    {
        ((wxMacCoreGraphicsBrushData*)m_brush.GetRefData())->Apply(this);
        CGContextFillRect(m_cgContext, rect);
    }
    
    wxQuartzOffsetHelper helper( m_cgContext , ShouldOffset() );
    if ( !m_pen.IsNull() )
    {
        ((wxMacCoreGraphicsPenData*)m_pen.GetRefData())->Apply(this);
        CGContextStrokeRect(m_cgContext, rect);
    }
}

// concatenates this transform with the current transform of this context
void wxMacCoreGraphicsContext::ConcatTransform( const wxGraphicsMatrix& matrix )
{
    if ( m_cgContext )
        CGContextConcatCTM( m_cgContext, *(CGAffineTransform*) matrix.GetNativeMatrix());
    else
        m_windowTransform = CGAffineTransformConcat(*(CGAffineTransform*) matrix.GetNativeMatrix(), m_windowTransform);
}

// sets the transform of this context
void wxMacCoreGraphicsContext::SetTransform( const wxGraphicsMatrix& matrix )
{
    CheckInvariants();
    if ( m_cgContext )
    {
        CGAffineTransform transform = CGContextGetCTM( m_cgContext );
        transform = CGAffineTransformInvert( transform ) ;
        CGContextConcatCTM( m_cgContext, transform);
        CGContextConcatCTM( m_cgContext, *(CGAffineTransform*) matrix.GetNativeMatrix());
    }
    else
    {
        m_windowTransform = *(CGAffineTransform*) matrix.GetNativeMatrix();
    }
    CheckInvariants();
}

// gets the matrix of this context
wxGraphicsMatrix wxMacCoreGraphicsContext::GetTransform() const
{
    wxGraphicsMatrix m = CreateMatrix();
    *((CGAffineTransform*) m.GetNativeMatrix()) = ( m_cgContext == NULL ? m_windowTransform :
        CGContextGetCTM( m_cgContext ));
    return m;
}


#if wxUSE_IMAGE

// ----------------------------------------------------------------------------
// wxMacCoreGraphicsImageContext
// ----------------------------------------------------------------------------

// This is a GC that can be used to draw on wxImage. In this implementation we
// simply draw on a wxBitmap using wxMemoryDC and then convert it to wxImage in
// the end so it's not especially interesting and exists mainly for
// compatibility with the other platforms.
class wxMacCoreGraphicsImageContext : public wxMacCoreGraphicsContext
{
public:
    wxMacCoreGraphicsImageContext(wxGraphicsRenderer* renderer,
                                  wxImage& image) :
        wxMacCoreGraphicsContext(renderer),
        m_image(image),
        m_bitmap(image),
        m_memDC(m_bitmap)
    {
        SetNativeContext
        (
            (CGContextRef)(m_memDC.GetGraphicsContext()->GetNativeContext())
        );
        m_width = image.GetWidth();
        m_height = image.GetHeight();
    }

    virtual ~wxMacCoreGraphicsImageContext()
    {
        m_memDC.SelectObject(wxNullBitmap);
        m_image = m_bitmap.ConvertToImage();
    }

private:
    wxImage& m_image;
    wxBitmap m_bitmap;
    wxMemoryDC m_memDC;
};

#endif // wxUSE_IMAGE

//
// Renderer
//

//-----------------------------------------------------------------------------
// wxMacCoreGraphicsRenderer declaration
//-----------------------------------------------------------------------------

class WXDLLIMPEXP_CORE wxMacCoreGraphicsRenderer : public wxGraphicsRenderer
{
public :
    wxMacCoreGraphicsRenderer() {}

    virtual ~wxMacCoreGraphicsRenderer() {}

    // Context

    virtual wxGraphicsContext * CreateContext( const wxWindowDC& dc) wxOVERRIDE;
    virtual wxGraphicsContext * CreateContext( const wxMemoryDC& dc) wxOVERRIDE;
#if wxUSE_PRINTING_ARCHITECTURE
    virtual wxGraphicsContext * CreateContext( const wxPrinterDC& dc) wxOVERRIDE;
#endif

    virtual wxGraphicsContext * CreateContextFromNativeContext( void * context ) wxOVERRIDE;

    virtual wxGraphicsContext * CreateContextFromNativeWindow( void * window ) wxOVERRIDE;

    virtual wxGraphicsContext * CreateContext( wxWindow* window ) wxOVERRIDE;

#if wxUSE_IMAGE
    virtual wxGraphicsContext * CreateContextFromImage(wxImage& image) wxOVERRIDE;
#endif // wxUSE_IMAGE

    virtual wxGraphicsContext * CreateMeasuringContext() wxOVERRIDE;

    // Path

    virtual wxGraphicsPath CreatePath() wxOVERRIDE;

    // Matrix

    virtual wxGraphicsMatrix CreateMatrix( wxDouble a=1.0, wxDouble b=0.0, wxDouble c=0.0, wxDouble d=1.0,
        wxDouble tx=0.0, wxDouble ty=0.0) wxOVERRIDE;


    virtual wxGraphicsPen CreatePen(const wxPen& pen) wxOVERRIDE ;

    virtual wxGraphicsBrush CreateBrush(const wxBrush& brush ) wxOVERRIDE ;

    virtual wxGraphicsBrush
    CreateLinearGradientBrush(wxDouble x1, wxDouble y1,
                              wxDouble x2, wxDouble y2,
                              const wxGraphicsGradientStops& stops) wxOVERRIDE;

    virtual wxGraphicsBrush
    CreateRadialGradientBrush(wxDouble xo, wxDouble yo,
                              wxDouble xc, wxDouble yc,
                              wxDouble radius,
                              const wxGraphicsGradientStops& stops) wxOVERRIDE;

   // sets the font
    virtual wxGraphicsFont CreateFont( const wxFont &font , const wxColour &col = *wxBLACK ) wxOVERRIDE ;
    virtual wxGraphicsFont CreateFont(double sizeInPixels,
                                      const wxString& facename,
                                      int flags = wxFONTFLAG_DEFAULT,
                                      const wxColour& col = *wxBLACK) wxOVERRIDE;

    // create a native bitmap representation
    virtual wxGraphicsBitmap CreateBitmap( const wxBitmap &bitmap ) wxOVERRIDE ;

#if wxUSE_IMAGE
    virtual wxGraphicsBitmap CreateBitmapFromImage(const wxImage& image) wxOVERRIDE;
    virtual wxImage CreateImageFromBitmap(const wxGraphicsBitmap& bmp) wxOVERRIDE;
#endif // wxUSE_IMAGE

    // create a graphics bitmap from a native bitmap
    virtual wxGraphicsBitmap CreateBitmapFromNativeBitmap( void* bitmap ) wxOVERRIDE;

    // create a native bitmap representation
    virtual wxGraphicsBitmap CreateSubBitmap( const wxGraphicsBitmap &bitmap, wxDouble x, wxDouble y, wxDouble w, wxDouble h  ) wxOVERRIDE ;

    virtual wxString GetName() const wxOVERRIDE;
    virtual void GetVersion(int *major, int *minor, int *micro) const wxOVERRIDE;

private :
    wxDECLARE_DYNAMIC_CLASS_NO_COPY(wxMacCoreGraphicsRenderer);
} ;

//-----------------------------------------------------------------------------
// wxMacCoreGraphicsRenderer implementation
//-----------------------------------------------------------------------------

wxIMPLEMENT_DYNAMIC_CLASS(wxMacCoreGraphicsRenderer,wxGraphicsRenderer);

static wxMacCoreGraphicsRenderer gs_MacCoreGraphicsRenderer;

wxGraphicsRenderer* wxGraphicsRenderer::GetDefaultRenderer()
{
    return &gs_MacCoreGraphicsRenderer;
}

wxGraphicsContext * wxMacCoreGraphicsRenderer::CreateContext( const wxWindowDC& dc )
{
    const wxDCImpl* impl = dc.GetImpl();
    wxWindowDCImpl *win_impl = wxDynamicCast( impl, wxWindowDCImpl );
    if (win_impl)
    {
        int w, h;
        win_impl->GetSize( &w, &h );
        CGContextRef cgctx = 0;

        wxASSERT_MSG(win_impl->GetWindow(), "Invalid wxWindow in wxMacCoreGraphicsRenderer::CreateContext");
        if (win_impl->GetWindow())
            cgctx =  (CGContextRef)(win_impl->GetWindow()->MacGetCGContextRef());

        // having a cgctx being NULL is fine (will be created on demand)
        // this is the case for all wxWindowDCs except wxPaintDC
        wxMacCoreGraphicsContext *context = 
            new wxMacCoreGraphicsContext( this, cgctx, (wxDouble) w, (wxDouble) h );
        context->EnableOffset(dc.GetContentScaleFactor() < 2);
        return context;
    }
    return NULL;
}

wxGraphicsContext * wxMacCoreGraphicsRenderer::CreateContext( const wxMemoryDC& dc )
{
#ifdef __WXMAC__
    const wxDCImpl* impl = dc.GetImpl();
    wxMemoryDCImpl *mem_impl = wxDynamicCast( impl, wxMemoryDCImpl );
    if (mem_impl)
    {
        int w, h;
        mem_impl->GetSize( &w, &h );
        wxMacCoreGraphicsContext* context = new wxMacCoreGraphicsContext( this,
            (CGContextRef)(mem_impl->GetGraphicsContext()->GetNativeContext()), (wxDouble) w, (wxDouble) h );
        context->EnableOffset(dc.GetContentScaleFactor() < 2);
        return context;
    }
#endif
    return NULL;
}

#if wxUSE_PRINTING_ARCHITECTURE
wxGraphicsContext * wxMacCoreGraphicsRenderer::CreateContext( const wxPrinterDC& dc )
{
#ifdef __WXMAC__
    const wxDCImpl* impl = dc.GetImpl();
    wxPrinterDCImpl *print_impl = wxDynamicCast( impl, wxPrinterDCImpl );
    if (print_impl)
    {
        int w, h;
        print_impl->GetSize( &w, &h );
        return new wxMacCoreGraphicsContext( this,
            (CGContextRef)(print_impl->GetGraphicsContext()->GetNativeContext()), (wxDouble) w, (wxDouble) h );
    }
#endif
    return NULL;
}
#endif

wxGraphicsContext * wxMacCoreGraphicsRenderer::CreateContextFromNativeContext( void * context )
{
    return new wxMacCoreGraphicsContext(this,(CGContextRef)context);
}

wxGraphicsContext * wxMacCoreGraphicsRenderer::CreateContextFromNativeWindow( void * window )
{
    wxUnusedVar(window);
    return NULL;
}

wxGraphicsContext * wxMacCoreGraphicsRenderer::CreateContext( wxWindow* window )
{
    return new wxMacCoreGraphicsContext(this, window );
}

wxGraphicsContext * wxMacCoreGraphicsRenderer::CreateMeasuringContext()
{
    return new wxMacCoreGraphicsContext(this);
}

#if wxUSE_IMAGE

wxGraphicsContext*
wxMacCoreGraphicsRenderer::CreateContextFromImage(wxImage& image)
{
    return new wxMacCoreGraphicsImageContext(this, image);
}

#endif // wxUSE_IMAGE

// Path

wxGraphicsPath wxMacCoreGraphicsRenderer::CreatePath()
{
    wxGraphicsPath m;
    m.SetRefData( new wxMacCoreGraphicsPathData(this));
    return m;
}


// Matrix

wxGraphicsMatrix wxMacCoreGraphicsRenderer::CreateMatrix( wxDouble a, wxDouble b, wxDouble c, wxDouble d,
    wxDouble tx, wxDouble ty)
{
    wxGraphicsMatrix m;
    wxMacCoreGraphicsMatrixData* data = new wxMacCoreGraphicsMatrixData( this );
    data->Set( a,b,c,d,tx,ty ) ;
    m.SetRefData(data);
    return m;
}

wxGraphicsPen wxMacCoreGraphicsRenderer::CreatePen(const wxPen& pen)
{
    if ( !pen.IsOk() || pen.GetStyle() == wxPENSTYLE_TRANSPARENT )
        return wxNullGraphicsPen;
    else
    {
        wxGraphicsPen p;
        p.SetRefData(new wxMacCoreGraphicsPenData( this, pen ));
        return p;
    }
}

wxGraphicsBrush wxMacCoreGraphicsRenderer::CreateBrush(const wxBrush& brush )
{
    if ( !brush.IsOk() || brush.GetStyle() == wxBRUSHSTYLE_TRANSPARENT )
        return wxNullGraphicsBrush;
    else
    {
        wxGraphicsBrush p;
        p.SetRefData(new wxMacCoreGraphicsBrushData( this, brush ));
        return p;
    }
}

wxGraphicsBitmap wxMacCoreGraphicsRenderer::CreateBitmap( const wxBitmap& bmp )
{
    if ( bmp.IsOk() )
    {
        wxGraphicsBitmap p;
        p.SetRefData(new wxMacCoreGraphicsBitmapData( this , bmp.CreateCGImage(), bmp.GetDepth() == 1 ) );
        return p;
    }
    else
        return wxNullGraphicsBitmap;
}

#if wxUSE_IMAGE

wxGraphicsBitmap
wxMacCoreGraphicsRenderer::CreateBitmapFromImage(const wxImage& image)
{
    // We don't have any direct way to convert wxImage to CGImage so pass by
    // wxBitmap. This makes this function pretty useless in this implementation
    // but it allows to have the same API as with Cairo backend where we can
    // convert wxImage to a Cairo surface directly, bypassing wxBitmap.
    return CreateBitmap(wxBitmap(image));
}

wxImage wxMacCoreGraphicsRenderer::CreateImageFromBitmap(const wxGraphicsBitmap& bmp)
{
    wxMacCoreGraphicsBitmapData* const
        data = static_cast<wxMacCoreGraphicsBitmapData*>(bmp.GetRefData());

    return data ? data->ConvertToImage() : wxNullImage;
}

#endif // wxUSE_IMAGE

wxGraphicsBitmap wxMacCoreGraphicsRenderer::CreateBitmapFromNativeBitmap( void* bitmap )
{
    if ( bitmap != NULL )
    {
        wxGraphicsBitmap p;
        p.SetRefData(new wxMacCoreGraphicsBitmapData( this , (CGImageRef) bitmap, false ));
        return p;
    }
    else
        return wxNullGraphicsBitmap;
}

wxGraphicsBitmap wxMacCoreGraphicsRenderer::CreateSubBitmap( const wxGraphicsBitmap &bmp, wxDouble x, wxDouble y, wxDouble w, wxDouble h  )
{
    wxMacCoreGraphicsBitmapData* refdata  =static_cast<wxMacCoreGraphicsBitmapData*>(bmp.GetRefData());
    CGImageRef img = refdata->GetBitmap();
    if ( img )
    {
        wxGraphicsBitmap p;
        CGImageRef subimg = CGImageCreateWithImageInRect(img,CGRectMake( (CGFloat) x , (CGFloat) y , (CGFloat) w , (CGFloat) h ));
        p.SetRefData(new wxMacCoreGraphicsBitmapData( this , subimg, refdata->IsMonochrome() ) );
        return p;
    }
    else
        return wxNullGraphicsBitmap;
}

wxString wxMacCoreGraphicsRenderer::GetName() const
{
    return "cg";
}

void wxMacCoreGraphicsRenderer::GetVersion(int *major, int *minor, int *micro) const
{
    if ( major )
        *major = wxPlatformInfo::Get().GetOSMajorVersion();
    if ( minor )
        *minor = wxPlatformInfo::Get().GetOSMinorVersion();
    if ( micro )
        *micro = 0;
}

wxGraphicsBrush
wxMacCoreGraphicsRenderer::CreateLinearGradientBrush(wxDouble x1, wxDouble y1,
                                                     wxDouble x2, wxDouble y2,
                                                     const wxGraphicsGradientStops& stops)
{
    wxGraphicsBrush p;
    wxMacCoreGraphicsBrushData* d = new wxMacCoreGraphicsBrushData( this );
    d->CreateLinearGradientBrush(x1, y1, x2, y2, stops);
    p.SetRefData(d);
    return p;
}

wxGraphicsBrush
wxMacCoreGraphicsRenderer::CreateRadialGradientBrush(wxDouble xo, wxDouble yo,
                                                     wxDouble xc, wxDouble yc,
                                                     wxDouble radius,
                                                     const wxGraphicsGradientStops& stops)
{
    wxGraphicsBrush p;
    wxMacCoreGraphicsBrushData* d = new wxMacCoreGraphicsBrushData( this );
    d->CreateRadialGradientBrush(xo, yo, xc, yc, radius, stops);
    p.SetRefData(d);
    return p;
}

wxGraphicsFont wxMacCoreGraphicsRenderer::CreateFont( const wxFont &font , const wxColour &col )
{
    if ( font.IsOk() )
    {
        wxGraphicsFont p;
        p.SetRefData(new wxMacCoreGraphicsFontData( this , font, col ));
        return p;
    }
    else
        return wxNullGraphicsFont;
}

wxGraphicsFont
wxMacCoreGraphicsRenderer::CreateFont(double sizeInPixels,
                                      const wxString& facename,
                                      int flags,
                                      const wxColour& col)
{
    // This implementation is not ideal as we don't support fractional font
    // sizes right now, but it's the simplest one.
    //
    // Notice that under Mac we always use 72 DPI so the font size in pixels is
    // the same as the font size in points and we can pass it directly to wxFont
    // ctor.
    wxFont font(wxRound(sizeInPixels),
                wxFONTFAMILY_DEFAULT,
                flags & wxFONTFLAG_ITALIC ? wxFONTSTYLE_ITALIC
                                          : wxFONTSTYLE_NORMAL,
                flags & wxFONTFLAG_BOLD ? wxFONTWEIGHT_BOLD
                                        : wxFONTWEIGHT_NORMAL,
                (flags & wxFONTFLAG_UNDERLINED) != 0,
                facename);

    if ( flags & wxFONTFLAG_STRIKETHROUGH )
        font.MakeStrikethrough();

    wxGraphicsFont f;
    f.SetRefData(new wxMacCoreGraphicsFontData(this, font, col));
    return f;
}

//
// CoreGraphics Helper Methods
//

// Data Providers and Consumers

size_t UMAPutBytesCFRefCallback( void *info, const void *bytes, size_t count )
{
    CFMutableDataRef data = (CFMutableDataRef) info;
    if ( data )
    {
        CFDataAppendBytes( data, (const UInt8*) bytes, count );
    }
    return count;
}

void wxMacReleaseCFDataProviderCallback(void *info,
                                      const void *WXUNUSED(data),
                                      size_t WXUNUSED(count))
{
    if ( info )
        CFRelease( (CFDataRef) info );
}

void wxMacReleaseCFDataConsumerCallback( void *info )
{
    if ( info )
        CFRelease( (CFDataRef) info );
}

CGDataProviderRef wxMacCGDataProviderCreateWithCFData( CFDataRef data )
{
    if ( data == NULL )
        return NULL;

    return CGDataProviderCreateWithCFData( data );
}

CGDataConsumerRef wxMacCGDataConsumerCreateWithCFData( CFMutableDataRef data )
{
    if ( data == NULL )
        return NULL;

    return CGDataConsumerCreateWithCFData( data );
}

void
wxMacReleaseMemoryBufferProviderCallback(void *info,
                                         const void * WXUNUSED_UNLESS_DEBUG(data),
                                         size_t WXUNUSED(size))
{
    wxMemoryBuffer* membuf = (wxMemoryBuffer*) info ;

    wxASSERT( data == membuf->GetData() ) ;

    delete membuf ;
}

CGDataProviderRef wxMacCGDataProviderCreateWithMemoryBuffer( const wxMemoryBuffer& buf )
{
    wxMemoryBuffer* b = new wxMemoryBuffer( buf );
    if ( b->GetDataLen() == 0 )
    {
        delete b;
        return NULL;
    }

    return CGDataProviderCreateWithData( b , (const void *) b->GetData() , b->GetDataLen() ,
                                                 wxMacReleaseMemoryBufferProviderCallback );
}
