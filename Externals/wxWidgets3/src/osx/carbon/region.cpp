/////////////////////////////////////////////////////////////////////////////
// File:      src/osx/carbon/region.cpp
// Purpose:   Region class
// Author:    Stefan Csomor
// Created:   Fri Oct 24 10:46:34 MET 1997
// Copyright: (c) 1997 Stefan Csomor
// Licence:   wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#include "wx/wxprec.h"

#if wxOSX_USE_COCOA_OR_CARBON

#include "wx/region.h"

#ifndef WX_PRECOMP
    #include "wx/gdicmn.h"
    #include "wx/dcmemory.h"
#endif

#include "wx/osx/private.h"

wxIMPLEMENT_DYNAMIC_CLASS(wxRegion, wxGDIObject);
wxIMPLEMENT_DYNAMIC_CLASS(wxRegionIterator, wxObject);

#define OSX_USE_SCANLINES 1

//-----------------------------------------------------------------------------
// wxRegionRefData implementation
//-----------------------------------------------------------------------------

class WXDLLEXPORT wxRegionRefData : public wxGDIRefData
{
public:
    wxRegionRefData()
    {
        m_macRgn.reset( HIShapeCreateMutable() );
    }

    wxRegionRefData(wxCFRef<HIShapeRef> &region)
    {
        m_macRgn.reset( HIShapeCreateMutableCopy(region) );
    }

    wxRegionRefData(long x, long y, long w, long h)
    {
        CGRect r = CGRectMake(x,y,w,h);
        wxCFRef<HIShapeRef> rect(HIShapeCreateWithRect(&r));
        m_macRgn.reset( HIShapeCreateMutableCopy(rect) );
    }

    wxRegionRefData(const wxRegionRefData& data)
        : wxGDIRefData()
    {
        m_macRgn.reset( HIShapeCreateMutableCopy(data.m_macRgn) );
    }

    virtual ~wxRegionRefData()
    {
    }

    wxCFRef<HIMutableShapeRef> m_macRgn;
};

#define M_REGION (((wxRegionRefData*)m_refData)->m_macRgn)
#define OTHER_M_REGION(a) (((wxRegionRefData*)(a.m_refData))->m_macRgn)

//-----------------------------------------------------------------------------
// wxRegion
//-----------------------------------------------------------------------------

wxRegion::wxRegion(WXHRGN hRegion )
{
    wxCFRef< HIShapeRef > shape( (HIShapeRef) hRegion );
    m_refData = new wxRegionRefData(shape);
}

wxRegion::wxRegion(long x, long y, long w, long h)
{
    m_refData = new wxRegionRefData(x , y , w , h );
}

wxRegion::wxRegion(const wxPoint& topLeft, const wxPoint& bottomRight)
{
    m_refData = new wxRegionRefData(topLeft.x , topLeft.y ,
                                    bottomRight.x - topLeft.x,
                                    bottomRight.y - topLeft.y);
}

wxRegion::wxRegion(const wxRect& rect)
{
    m_refData = new wxRegionRefData(rect.x , rect.y , rect.width , rect.height);
}

#if OSX_USE_SCANLINES

/*
 
 Copyright 1987, 1998  The Open Group
 
 Permission to use, copy, modify, distribute, and sell this software and its
 documentation for any purpose is hereby granted without fee, provided that
 the above copyright notice appear in all copies and that both that
 copyright notice and this permission notice appear in supporting
 documentation.
 
 The above copyright notice and this permission notice shall be included
 in all copies or substantial portions of the Software.
 
 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 IN NO EVENT SHALL THE OPEN GROUP BE LIABLE FOR ANY CLAIM, DAMAGES OR
 OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 OTHER DEALINGS IN THE SOFTWARE.
 
 Except as contained in this notice, the name of The Open Group shall
 not be used in advertising or otherwise to promote the sale, use or
 other dealings in this Software without prior written authorization
 from The Open Group.
 
 */

/* miscanfill.h */

/*
 *     scanfill.h
 *
 *     Written by Brian Kelleher; Jan 1985
 *
 *     This file contains a few macros to help track
 *     the edge of a filled object.  The object is assumed
 *     to be filled in scanline order, and thus the
 *     algorithm used is an extension of Bresenham's line
 *     drawing algorithm which assumes that y is always the
 *     major axis.
 *     Since these pieces of code are the same for any filled shape,
 *     it is more convenient to gather the library in one
 *     place, but since these pieces of code are also in
 *     the inner loops of output primitives, procedure call
 *     overhead is out of the question.
 *     See the author for a derivation if needed.
 */


/*
 *  In scan converting polygons, we want to choose those pixels
 *  which are inside the polygon.  Thus, we add .5 to the starting
 *  x coordinate for both left and right edges.  Now we choose the
 *  first pixel which is inside the pgon for the left edge and the
 *  first pixel which is outside the pgon for the right edge.
 *  Draw the left pixel, but not the right.
 *
 *  How to add .5 to the starting x coordinate:
 *      If the edge is moving to the right, then subtract dy from the
 *  error term from the general form of the algorithm.
 *      If the edge is moving to the left, then add dy to the error term.
 *
 *  The reason for the difference between edges moving to the left
 *  and edges moving to the right is simple:  If an edge is moving
 *  to the right, then we want the algorithm to flip immediately.
 *  If it is moving to the left, then we don't want it to flip until
 *  we traverse an entire pixel.
 */
#define BRESINITPGON(dy, x1, x2, xStart, d, m, m1, incr1, incr2) { \
int dx;      /* local storage */ \
\
/* \
*  if the edge is horizontal, then it is ignored \
*  and assumed not to be processed.  Otherwise, do this stuff. \
*/ \
if ((dy) != 0) { \
xStart = (x1); \
dx = (x2) - xStart; \
if (dx < 0) { \
m = dx / (dy); \
m1 = m - 1; \
incr1 = -2 * dx + 2 * (dy) * m1; \
incr2 = -2 * dx + 2 * (dy) * m; \
d = 2 * m * (dy) - 2 * dx - 2 * (dy); \
} else { \
m = dx / (dy); \
m1 = m + 1; \
incr1 = 2 * dx - 2 * (dy) * m1; \
incr2 = 2 * dx - 2 * (dy) * m; \
d = -2 * m * (dy) + 2 * dx; \
} \
} \
}

#define BRESINCRPGON(d, minval, m, m1, incr1, incr2) { \
if (m1 > 0) { \
if (d > 0) { \
minval += m1; \
d += incr1; \
} \
else { \
minval += m; \
d += incr2; \
} \
} else {\
if (d >= 0) { \
minval += m1; \
d += incr1; \
} \
else { \
minval += m; \
d += incr2; \
} \
} \
}


/*
 *     This structure contains all of the information needed
 *     to run the bresenham algorithm.
 *     The variables may be hardcoded into the declarations
 *     instead of using this structure to make use of
 *     register declarations.
 */
typedef struct {
    int minor;         /* minor axis        */
    int d;           /* decision variable */
    int m, m1;       /* slope and slope+1 */
    int incr1, incr2; /* error increments */
} BRESINFO;


#define BRESINITPGONSTRUCT(dmaj, min1, min2, bres) \
BRESINITPGON(dmaj, min1, min2, bres.minor, bres.d, \
bres.m, bres.m1, bres.incr1, bres.incr2)

#define BRESINCRPGONSTRUCT(bres) \
BRESINCRPGON(bres.d, bres.minor, bres.m, bres.m1, bres.incr1, bres.incr2)


/* mipoly.h */

/*
 *     fill.h
 *
 *     Created by Brian Kelleher; Oct 1985
 *
 *     Include file for filled polygon routines.
 *
 *     These are the data structures needed to scan
 *     convert regions.  Two different scan conversion
 *     methods are available -- the even-odd method, and
 *     the winding number method.
 *     The even-odd rule states that a point is inside
 *     the polygon if a ray drawn from that point in any
 *     direction will pass through an odd number of
 *     path segments.
 *     By the winding number rule, a point is decided
 *     to be inside the polygon if a ray drawn from that
 *     point in any direction passes through a different
 *     number of clockwise and counter-clockwise path
 *     segments.
 *
 *     These data structures are adapted somewhat from
 *     the algorithm in (Foley/Van Dam) for scan converting
 *     polygons.
 *     The basic algorithm is to start at the top (smallest y)
 *     of the polygon, stepping down to the bottom of
 *     the polygon by incrementing the y coordinate.  We
 *     keep a list of edges which the current scanline crosses,
 *     sorted by x.  This list is called the Active Edge Table (AET)
 *     As we change the y-coordinate, we update each entry in
 *     in the active edge table to reflect the edges new xcoord.
 *     This list must be sorted at each scanline in case
 *     two edges intersect.
 *     We also keep a data structure known as the Edge Table (ET),
 *     which keeps track of all the edges which the current
 *     scanline has not yet reached.  The ET is basically a
 *     list of ScanLineList structures containing a list of
 *     edges which are entered at a given scanline.  There is one
 *     ScanLineList per scanline at which an edge is entered.
 *     When we enter a new edge, we move it from the ET to the AET.
 *
 *     From the AET, we can implement the even-odd rule as in
 *     (Foley/Van Dam).
 *     The winding number rule is a little trickier.  We also
 *     keep the EdgeTableEntries in the AET linked by the
 *     nextWETE (winding EdgeTableEntry) link.  This allows
 *     the edges to be linked just as before for updating
 *     purposes, but only uses the edges linked by the nextWETE
 *     link as edges representing spans of the polygon to
 *     drawn (as with the even-odd rule).
 */

/*
 * for the winding number rule
 */
#define CLOCKWISE          1
#define COUNTERCLOCKWISE  -1

typedef struct _EdgeTableEntry {
    int ymax;             /* ycoord at which we exit this edge. */
    BRESINFO bres;        /* Bresenham info to run the edge     */
    struct _EdgeTableEntry *next;       /* next in the list     */
    struct _EdgeTableEntry *back;       /* for insertion sort   */
    struct _EdgeTableEntry *nextWETE;   /* for winding num rule */
    int ClockWise;        /* flag for winding number rule       */
} EdgeTableEntry;


typedef struct _ScanLineList{
    int scanline;              /* the scanline represented */
    EdgeTableEntry *edgelist;  /* header node              */
    struct _ScanLineList *next;  /* next in the list       */
} ScanLineList;


typedef struct {
    int ymax;                 /* ymax for the polygon     */
    int ymin;                 /* ymin for the polygon     */
    ScanLineList scanlines;   /* header node              */
} EdgeTable;


/*
 * Here is a struct to help with storage allocation
 * so we can allocate a big chunk at a time, and then take
 * pieces from this heap when we need to.
 */
#define SLLSPERBLOCK 25

typedef struct _ScanLineListBlock {
    ScanLineList SLLs[SLLSPERBLOCK];
    struct _ScanLineListBlock *next;
} ScanLineListBlock;

/*
 * number of points to buffer before sending them off
 * to scanlines() :  Must be an even number
 */
#define NUMPTSTOBUFFER 200


/*
 *
 *     a few macros for the inner loops of the fill code where
 *     performance considerations don't allow a procedure call.
 *
 *     Evaluate the given edge at the given scanline.
 *     If the edge has expired, then we leave it and fix up
 *     the active edge table; otherwise, we increment the
 *     x value to be ready for the next scanline.
 *     The winding number rule is in effect, so we must notify
 *     the caller when the edge has been removed so he
 *     can reorder the Winding Active Edge Table.
 */
#define EVALUATEEDGEWINDING(pAET, pPrevAET, y, fixWAET) { \
if (pAET->ymax == y) {          /* leaving this edge */ \
pPrevAET->next = pAET->next; \
pAET = pPrevAET->next; \
fixWAET = 1; \
if (pAET) \
pAET->back = pPrevAET; \
} \
else { \
BRESINCRPGONSTRUCT(pAET->bres); \
pPrevAET = pAET; \
pAET = pAET->next; \
} \
}


/*
 *     Evaluate the given edge at the given scanline.
 *     If the edge has expired, then we leave it and fix up
 *     the active edge table; otherwise, we increment the
 *     x value to be ready for the next scanline.
 *     The even-odd rule is in effect.
 */
#define EVALUATEEDGEEVENODD(pAET, pPrevAET, y) { \
if (pAET->ymax == y) {          /* leaving this edge */ \
pPrevAET->next = pAET->next; \
pAET = pPrevAET->next; \
if (pAET) \
pAET->back = pPrevAET; \
} \
else { \
BRESINCRPGONSTRUCT(pAET->bres); \
pPrevAET = pAET; \
pAET = pAET->next; \
} \
}

/* mipolyutil.c */

static bool miCreateETandAET(
                             int /*count*/,
                             const wxPoint * /*pts*/,
                             EdgeTable * /*ET*/,
                             EdgeTableEntry * /*AET*/,
                             EdgeTableEntry * /*pETEs*/,
                             ScanLineListBlock * /*pSLLBlock*/
                             );

static void miloadAET(
                      EdgeTableEntry * /*AET*/,
                      EdgeTableEntry * /*ETEs*/
                      );

static void micomputeWAET(
                          EdgeTableEntry * /*AET*/
                          );

static int miInsertionSort(
                           EdgeTableEntry * /*AET*/
                           );

static void miFreeStorage(
                          ScanLineListBlock * /*pSLLBlock*/
                          );

/*
 *     fillUtils.c
 *
 *     Written by Brian Kelleher;  Oct. 1985
 *
 *     This module contains all of the utility functions
 *     needed to scan convert a polygon.
 *
 */

/*
 *     InsertEdgeInET
 *
 *     Insert the given edge into the edge table.
 *     First we must find the correct bucket in the
 *     Edge table, then find the right slot in the
 *     bucket.  Finally, we can insert it.
 *
 */
static bool
miInsertEdgeInET(EdgeTable *ET, EdgeTableEntry *ETE,  int scanline,
                 ScanLineListBlock **SLLBlock, int *iSLLBlock)
{
    EdgeTableEntry *start, *prev;
    ScanLineList *pSLL, *pPrevSLL;
    ScanLineListBlock *tmpSLLBlock;
    
    /*
     * find the right bucket to put the edge into
     */
    pPrevSLL = &ET->scanlines;
    pSLL = pPrevSLL->next;
    while (pSLL && (pSLL->scanline < scanline))
    {
        pPrevSLL = pSLL;
        pSLL = pSLL->next;
    }
    
    /*
     * reassign pSLL (pointer to ScanLineList) if necessary
     */
    if ((!pSLL) || (pSLL->scanline > scanline))
    {
        if (*iSLLBlock > SLLSPERBLOCK-1)
        {
            tmpSLLBlock =
            (ScanLineListBlock *)malloc(sizeof(ScanLineListBlock));
            if (!tmpSLLBlock)
                return FALSE;
            (*SLLBlock)->next = tmpSLLBlock;
            tmpSLLBlock->next = (ScanLineListBlock *)NULL;
            *SLLBlock = tmpSLLBlock;
            *iSLLBlock = 0;
        }
        pSLL = &((*SLLBlock)->SLLs[(*iSLLBlock)++]);
        
        pSLL->next = pPrevSLL->next;
        pSLL->edgelist = (EdgeTableEntry *)NULL;
        pPrevSLL->next = pSLL;
    }
    pSLL->scanline = scanline;
    
    /*
     * now insert the edge in the right bucket
     */
    prev = (EdgeTableEntry *)NULL;
    start = pSLL->edgelist;
    while (start && (start->bres.minor < ETE->bres.minor))
    {
        prev = start;
        start = start->next;
    }
    ETE->next = start;
    
    if (prev)
        prev->next = ETE;
    else
        pSLL->edgelist = ETE;
    return TRUE;
}

/*
 *     CreateEdgeTable
 *
 *     This routine creates the edge table for
 *     scan converting polygons.
 *     The Edge Table (ET) looks like:
 *
 *    EdgeTable
 *     --------
 *    |  ymax  |        ScanLineLists
 *    |scanline|-->------------>-------------->...
 *     --------   |scanline|   |scanline|
 *                |edgelist|   |edgelist|
 *                ---------    ---------
 *                    |             |
 *                    |             |
 *                    V             V
 *              list of ETEs   list of ETEs
 *
 *     where ETE is an EdgeTableEntry data structure,
 *     and there is one ScanLineList per scanline at
 *     which an edge is initially entered.
 *
 */

static bool
miCreateETandAET(int count, const wxPoint * pts, EdgeTable *ET, EdgeTableEntry *AET,
                 EdgeTableEntry *pETEs, ScanLineListBlock *pSLLBlock)
{
    const wxPoint* top, *bottom;
    const wxPoint* PrevPt, *CurrPt;
    int iSLLBlock = 0;
    
    int dy;
    
    if (count < 2)  return TRUE;
    
    /*
     *  initialize the Active Edge Table
     */
    AET->next = (EdgeTableEntry *)NULL;
    AET->back = (EdgeTableEntry *)NULL;
    AET->nextWETE = (EdgeTableEntry *)NULL;
    AET->bres.minor = INT_MIN;
    
    /*
     *  initialize the Edge Table.
     */
    ET->scanlines.next = (ScanLineList *)NULL;
    ET->ymax = INT_MIN;
    ET->ymin = INT_MAX;
    pSLLBlock->next = (ScanLineListBlock *)NULL;
    
    PrevPt = &pts[count-1];
    
    /*
     *  for each vertex in the array of points.
     *  In this loop we are dealing with two vertices at
     *  a time -- these make up one edge of the polygon.
     */
    while (count--)
    {
        CurrPt = pts++;
        
        /*
         *  find out which point is above and which is below.
         */
        if (PrevPt->y > CurrPt->y)
        {
            bottom = PrevPt, top = CurrPt;
            pETEs->ClockWise = 0;
        }
        else
        {
            bottom = CurrPt, top = PrevPt;
            pETEs->ClockWise = 1;
        }
        
        /*
         * don't add horizontal edges to the Edge table.
         */
        if (bottom->y != top->y)
        {
            pETEs->ymax = bottom->y-1;  /* -1 so we don't get last scanline */
            
            /*
             *  initialize integer edge algorithm
             */
            dy = bottom->y - top->y;
            BRESINITPGONSTRUCT(dy, top->x, bottom->x, pETEs->bres);
            
            if (!miInsertEdgeInET(ET, pETEs, top->y, &pSLLBlock, &iSLLBlock))
            {
                miFreeStorage(pSLLBlock->next);
                return FALSE;
            }
            
            ET->ymax = wxMax(ET->ymax, PrevPt->y);
            ET->ymin = wxMin(ET->ymin, PrevPt->y);
            pETEs++;
        }
        
        PrevPt = CurrPt;
    }
    return TRUE;
}

/*
 *     loadAET
 *
 *     This routine moves EdgeTableEntries from the
 *     EdgeTable into the Active Edge Table,
 *     leaving them sorted by smaller x coordinate.
 *
 */

static void
miloadAET(EdgeTableEntry *AET, EdgeTableEntry *ETEs)
{
    EdgeTableEntry *pPrevAET;
    EdgeTableEntry *tmp;
    
    pPrevAET = AET;
    AET = AET->next;
    while (ETEs)
    {
        while (AET && (AET->bres.minor < ETEs->bres.minor))
        {
            pPrevAET = AET;
            AET = AET->next;
        }
        tmp = ETEs->next;
        ETEs->next = AET;
        if (AET)
            AET->back = ETEs;
        ETEs->back = pPrevAET;
        pPrevAET->next = ETEs;
        pPrevAET = ETEs;
        
        ETEs = tmp;
    }
}

/*
 *     computeWAET
 *
 *     This routine links the AET by the
 *     nextWETE (winding EdgeTableEntry) link for
 *     use by the winding number rule.  The final
 *     Active Edge Table (AET) might look something
 *     like:
 *
 *     AET
 *     ----------  ---------   ---------
 *     |ymax    |  |ymax    |  |ymax    |
 *     | ...    |  |...     |  |...     |
 *     |next    |->|next    |->|next    |->...
 *     |nextWETE|  |nextWETE|  |nextWETE|
 *     ---------   ---------   ^--------
 *         |                   |       |
 *         V------------------->       V---> ...
 *
 */
static void
micomputeWAET(EdgeTableEntry *AET)
{
    EdgeTableEntry *pWETE;
    int inside = 1;
    int isInside = 0;
    
    AET->nextWETE = (EdgeTableEntry *)NULL;
    pWETE = AET;
    AET = AET->next;
    while (AET)
    {
        if (AET->ClockWise)
            isInside++;
        else
            isInside--;
        
        if ((!inside && !isInside) ||
            ( inside &&  isInside))
        {
            pWETE->nextWETE = AET;
            pWETE = AET;
            inside = !inside;
        }
        AET = AET->next;
    }
    pWETE->nextWETE = (EdgeTableEntry *)NULL;
}

/*
 *     InsertionSort
 *
 *     Just a simple insertion sort using
 *     pointers and back pointers to sort the Active
 *     Edge Table.
 *
 */

static int
miInsertionSort(EdgeTableEntry *AET)
{
    EdgeTableEntry *pETEchase;
    EdgeTableEntry *pETEinsert;
    EdgeTableEntry *pETEchaseBackTMP;
    int changed = 0;
    
    AET = AET->next;
    while (AET)
    {
        pETEinsert = AET;
        pETEchase = AET;
        while (pETEchase->back->bres.minor > AET->bres.minor)
            pETEchase = pETEchase->back;
        
        AET = AET->next;
        if (pETEchase != pETEinsert)
        {
            pETEchaseBackTMP = pETEchase->back;
            pETEinsert->back->next = AET;
            if (AET)
                AET->back = pETEinsert->back;
            pETEinsert->next = pETEchase;
            pETEchase->back->next = pETEinsert;
            pETEchase->back = pETEinsert;
            pETEinsert->back = pETEchaseBackTMP;
            changed = 1;
        }
    }
    return(changed);
}

/*
 *     Clean up our act.
 */
static void
miFreeStorage(ScanLineListBlock *pSLLBlock)
{
    ScanLineListBlock   *tmpSLLBlock;
    
    while (pSLLBlock) 
    {
        tmpSLLBlock = pSLLBlock->next;
        free(pSLLBlock);
        pSLLBlock = tmpSLLBlock;
    }
}

/* mipolygen.c */

static bool
scanFillGeneralPoly( wxRegion* rgn,
                  int   count,              /* number of points        */
                  const wxPoint *ptsIn,               /* the points              */
                  wxPolygonFillMode fillStyle
                  )
{
    EdgeTableEntry *pAET;  /* the Active Edge Table   */
    int y;                 /* the current scanline    */
    int nPts = 0;          /* number of pts in buffer */
    EdgeTableEntry *pWETE; /* Winding Edge Table      */
    ScanLineList *pSLL;    /* Current ScanLineList    */
    wxPoint * ptsOut;      /* ptr to output buffers   */
    int *width;
    wxPoint FirstPoint[NUMPTSTOBUFFER]; /* the output buffers */
    int FirstWidth[NUMPTSTOBUFFER];
    EdgeTableEntry *pPrevAET;       /* previous AET entry      */
    EdgeTable ET;                   /* Edge Table header node  */
    EdgeTableEntry AET;             /* Active ET header node   */
    EdgeTableEntry *pETEs;          /* Edge Table Entries buff */
    ScanLineListBlock SLLBlock;     /* header for ScanLineList */
    int fixWAET = 0;
    
    if (count < 3)
        return(TRUE);
    
    if(!(pETEs = (EdgeTableEntry *)
         malloc(sizeof(EdgeTableEntry) * count)))
        return(FALSE);
    ptsOut = FirstPoint;
    width = FirstWidth;
    if (!miCreateETandAET(count, ptsIn, &ET, &AET, pETEs, &SLLBlock))
    {
        free(pETEs);
        return(FALSE);
    }
    pSLL = ET.scanlines.next;
    
    if (fillStyle == wxODDEVEN_RULE)
    {
        /*
         *  for each scanline
         */
        for (y = ET.ymin; y < ET.ymax; y++)
        {
            /*
             *  Add a new edge to the active edge table when we
             *  get to the next edge.
             */
            if (pSLL && y == pSLL->scanline)
            {
                miloadAET(&AET, pSLL->edgelist);
                pSLL = pSLL->next;
            }
            pPrevAET = &AET;
            pAET = AET.next;
            
            /*
             *  for each active edge
             */
            while (pAET)
            {
                ptsOut->x = pAET->bres.minor;
                ptsOut++->y = y;
                *width++ = pAET->next->bres.minor - pAET->bres.minor;
                nPts++;
                
                /*
                 *  send out the buffer when its full
                 */
                if (nPts == NUMPTSTOBUFFER)
                {
                    // (*pgc->ops->FillSpans)(dst, pgc,
                    //     nPts, FirstPoint, FirstWidth,1);
                    
                    for ( int i = 0 ; i < nPts; ++i)
                    {
                        wxRect rect;
                        rect.y = FirstPoint[i].y;
                        rect.x = FirstPoint[i].x;
                        rect.height = 1;
                        rect.width = FirstWidth[i];
                        rgn->Union(rect);
                    }
                    ptsOut = FirstPoint;
                    width = FirstWidth;
                    nPts = 0;
                }
                EVALUATEEDGEEVENODD(pAET, pPrevAET, y)
                EVALUATEEDGEEVENODD(pAET, pPrevAET, y);
            }
            miInsertionSort(&AET);
        }
    }
    else      /* default to WindingNumber */
    {
        /*
         *  for each scanline
         */
        for (y = ET.ymin; y < ET.ymax; y++)
        {
            /*
             *  Add a new edge to the active edge table when we
             *  get to the next edge.
             */
            if (pSLL && y == pSLL->scanline)
            {
                miloadAET(&AET, pSLL->edgelist);
                micomputeWAET(&AET);
                pSLL = pSLL->next;
            }
            pPrevAET = &AET;
            pAET = AET.next;
            pWETE = pAET;
            
            /*
             *  for each active edge
             */
            while (pAET)
            {
                /*
                 *  if the next edge in the active edge table is
                 *  also the next edge in the winding active edge
                 *  table.
                 */
                if (pWETE == pAET)
                {
                    ptsOut->x = pAET->bres.minor;
                    ptsOut++->y = y;
                    *width++ = pAET->nextWETE->bres.minor - pAET->bres.minor;
                    nPts++;
                    
                    /*
                     *  send out the buffer
                     */
                    if (nPts == NUMPTSTOBUFFER)
                    {
                        // (*pgc->ops->FillSpans)(dst, pgc,
                        //     nPts, FirstPoint, FirstWidth,1);
                        for ( int i = 0 ; i < nPts ; ++i)
                        {
                            wxRect rect;
                            rect.y = FirstPoint[i].y;
                            rect.x = FirstPoint[i].x;
                            rect.height = 1;
                            rect.width = FirstWidth[i];
                            rgn->Union(rect);
                        }
                        ptsOut = FirstPoint;
                        width  = FirstWidth;
                        nPts = 0;
                    }
                    
                    pWETE = pWETE->nextWETE;
                    while (pWETE != pAET)
                        EVALUATEEDGEWINDING(pAET, pPrevAET, y, fixWAET);
                    pWETE = pWETE->nextWETE;
                }
                EVALUATEEDGEWINDING(pAET, pPrevAET, y, fixWAET);
            }
            
            /*
             *  reevaluate the Winding active edge table if we
             *  just had to resort it or if we just exited an edge.
             */
            if (miInsertionSort(&AET) || fixWAET)
            {
                micomputeWAET(&AET);
                fixWAET = 0;
            }
        }
    }
    
    /*
     *     Get any spans that we missed by buffering
     */
    // (*pgc->ops->FillSpans)(dst, pgc,
    //     nPts, FirstPoint, FirstWidth,1);
    for ( int i = 0 ; i < nPts; ++i)
    {
        wxRect rect;
        rect.y = FirstPoint[i].y;
        rect.x = FirstPoint[i].x;
        rect.height = 1;
        rect.width = FirstWidth[i];
        rgn->Union(rect);
    }

    free(pETEs);
    miFreeStorage(SLLBlock.next);
    return(TRUE);
}

#endif

wxRegion::wxRegion(size_t n, const wxPoint *points, wxPolygonFillMode fillStyle)
{
    // Set the region to a polygon shape generically using a bitmap with the 
    // polygon drawn on it. 
 
    m_refData = new wxRegionRefData(); 
    
#if OSX_USE_SCANLINES
    scanFillGeneralPoly(this,n,points,fillStyle);
#else
    wxCoord mx = 0; 
    wxCoord my = 0; 
    wxPoint p; 
    size_t idx;     
     
    // Find the max size needed to draw the polygon 
    for (idx=0; idx<n; idx++) 
    { 
        wxPoint pt = points[idx]; 
        if (pt.x > mx) 
            mx = pt.x; 
        if (pt.y > my) 
            my = pt.y; 
    } 
 
    // Make the bitmap 
    wxBitmap bmp(mx, my); 
    wxMemoryDC dc(bmp); 
    dc.SetBackground(*wxBLACK_BRUSH); 
    dc.Clear(); 
    dc.SetPen(*wxWHITE_PEN); 
    dc.SetBrush(*wxWHITE_BRUSH); 
    dc.DrawPolygon(n, (wxPoint*)points, 0, 0, fillStyle); 
    dc.SelectObject(wxNullBitmap); 
    bmp.SetMask(new wxMask(bmp, *wxBLACK)); 
 
    // Use it to set this region 
    Union(bmp);
#endif
}

wxRegion::~wxRegion()
{
    // m_refData unrefed in ~wxObject
}

wxGDIRefData *wxRegion::CreateGDIRefData() const
{
    return new wxRegionRefData;
}

wxGDIRefData *wxRegion::CloneGDIRefData(const wxGDIRefData *data) const
{
    return new wxRegionRefData(*static_cast<const wxRegionRefData *>(data));
}

//-----------------------------------------------------------------------------
//# Modify region
//-----------------------------------------------------------------------------

//! Clear current region
void wxRegion::Clear()
{
    UnRef();
}

// Move the region
bool wxRegion::DoOffset(wxCoord x, wxCoord y)
{
    wxCHECK_MSG( m_refData, false, wxT("invalid wxRegion") );

    if ( !x && !y )
        // nothing to do
        return true;

    AllocExclusive();

    verify_noerr( HIShapeOffset( M_REGION , x , y ) ) ;

    return true ;
}

bool wxRegion::DoUnionWithRect(const wxRect& rect)
{
    if ( !m_refData )
    {
        m_refData = new wxRegionRefData(rect.x , rect.y , rect.width , rect.height);
        return true;
    }
    
    AllocExclusive();
    
    CGRect r = CGRectMake(rect.x , rect.y , rect.width , rect.height);
    HIShapeUnionWithRect(M_REGION , &r);
    
    return true;
}

//! Union /e region with this.
bool wxRegion::DoCombine(const wxRegion& region, wxRegionOp op)
{
    wxCHECK_MSG( region.IsOk(), false, wxT("invalid wxRegion") );

    // Handle the special case of not initialized (e.g. default constructed)
    // region as we can't use HIShape functions if we don't have any shape.
    if ( !m_refData )
    {
        switch ( op )
        {
            case wxRGN_COPY:
            case wxRGN_OR:
            case wxRGN_XOR:
                // These operations make sense with a null region.
                *this = region;
                return true;

            case wxRGN_AND:
            case wxRGN_DIFF:
                // Those ones don't really make sense so just leave this region
                // empty/invalid.
                return false;
        }

        wxFAIL_MSG( wxT("Unknown region operation") );
        return false;
    }

    AllocExclusive();

    switch (op)
    {
        case wxRGN_AND:
            verify_noerr( HIShapeIntersect( M_REGION , OTHER_M_REGION(region) , M_REGION ) );
            break ;

        case wxRGN_OR:
            verify_noerr( HIShapeUnion( M_REGION , OTHER_M_REGION(region) , M_REGION ) );
            break ;

        case wxRGN_XOR:
            {
                // XOR is defined as the difference between union and intersection
                wxCFRef< HIShapeRef > unionshape( HIShapeCreateUnion( M_REGION , OTHER_M_REGION(region) ) );
                wxCFRef< HIShapeRef > intersectionshape( HIShapeCreateIntersection( M_REGION , OTHER_M_REGION(region) ) );
                verify_noerr( HIShapeDifference( unionshape, intersectionshape, M_REGION ) );
            }
            break ;

        case wxRGN_DIFF:
            verify_noerr( HIShapeDifference( M_REGION , OTHER_M_REGION(region) , M_REGION ) ) ;
            break ;

        case wxRGN_COPY:
        default:
            M_REGION.reset( HIShapeCreateMutableCopy( OTHER_M_REGION(region) ) );
            break ;
    }

    return true;
}

//-----------------------------------------------------------------------------
//# Information on region
//-----------------------------------------------------------------------------

bool wxRegion::DoIsEqual(const wxRegion& region) const
{
    // There doesn't seem to be any native function for checking the equality
    // of HIShapes so we compute their differences to determine if they are
    // equal.
    wxRegion r(*this);
    r.Subtract(region);

    if ( !r.IsEmpty() )
        return false;

    wxRegion r2(region);
    r2.Subtract(*this);

    return r2.IsEmpty();
}

// Outer bounds of region
bool wxRegion::DoGetBox(wxCoord& x, wxCoord& y, wxCoord& w, wxCoord& h) const
{
    if (m_refData)
    {
        CGRect box ;
        HIShapeGetBounds( M_REGION , &box ) ;
        x = static_cast<int>(box.origin.x);
        y = static_cast<int>(box.origin.y);
        w = static_cast<int>(box.size.width);
        h = static_cast<int>(box.size.height);

        return true;
    }
    else
    {
        x = y = w = h = 0;

        return false;
    }
}

// Is region empty?
bool wxRegion::IsEmpty() const
{
    if ( m_refData )
        return HIShapeIsEmpty( M_REGION ) ;
    else
        return true ;
}

WXHRGN wxRegion::GetWXHRGN() const
{
    if ( !m_refData )
        return NULL;

    return M_REGION ;
}

//-----------------------------------------------------------------------------
//# Tests
//-----------------------------------------------------------------------------

// Does the region contain the point?
wxRegionContain wxRegion::DoContainsPoint(wxCoord x, wxCoord y) const
{
    if (!m_refData)
        return wxOutRegion;

    CGPoint p = CGPointMake( x, y ) ;
    if (HIShapeContainsPoint( M_REGION , &p ) )
        return wxInRegion;

    return wxOutRegion;
}

// Does the region contain the rectangle (x, y, w, h)?
wxRegionContain wxRegion::DoContainsRect(const wxRect& r) const
{
    if (!m_refData)
        return wxOutRegion;

    CGRect rect = CGRectMake(r.x,r.y,r.width,r.height);
    wxCFRef<HIShapeRef> rectshape(HIShapeCreateWithRect(&rect));
    wxCFRef<HIShapeRef> intersect(HIShapeCreateIntersection(rectshape,M_REGION));
    CGRect bounds;
    HIShapeGetBounds(intersect, &bounds);

    if ( HIShapeIsRectangular(intersect) && CGRectEqualToRect(rect,bounds) )
        return wxInRegion;
    else if ( HIShapeIsEmpty( intersect ) )
        return wxOutRegion;
    else
        return wxPartRegion;
}

///////////////////////////////////////////////////////////////////////////////
//                                                                           //
//                               wxRegionIterator                            //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

/*!
 * Initialize empty iterator
 */
wxRegionIterator::wxRegionIterator()
    : m_current(0), m_numRects(0), m_rects(NULL)
{
}

wxRegionIterator::~wxRegionIterator()
{
    wxDELETEA(m_rects);
}

wxRegionIterator::wxRegionIterator(const wxRegionIterator& iterator)
    : wxObject()
    , m_current(iterator.m_current)
    , m_numRects(0)
    , m_rects(NULL)
{
    SetRects(iterator.m_numRects, iterator.m_rects);
}

wxRegionIterator& wxRegionIterator::operator=(const wxRegionIterator& iterator)
{
    m_current  = iterator.m_current;
    SetRects(iterator.m_numRects, iterator.m_rects);

    return *this;
}

/*!
 * Set iterator rects for region
 */
void wxRegionIterator::SetRects(long numRects, wxRect *rects)
{
    wxDELETEA(m_rects);

    if (rects && (numRects > 0))
    {
        int i;

        m_rects = new wxRect[numRects];
        for (i = 0; i < numRects; i++)
            m_rects[i] = rects[i];
    }

    m_numRects = numRects;
}

/*!
 * Initialize iterator for region
 */
wxRegionIterator::wxRegionIterator(const wxRegion& region)
{
    m_rects = NULL;

    Reset(region);
}

/*!
 * Reset iterator for a new /e region.
 */

class RegionToRectsCallbackData
{
public :
    wxRect* m_rects ;
    long m_current ;
};

OSStatus wxOSXRegionToRectsCounterCallback(
    int message, HIShapeRef WXUNUSED(region), const CGRect *WXUNUSED(rect), void *data )
{
    long *m_numRects = (long*) data ;
    if ( message == kHIShapeEnumerateInit )
    {
        (*m_numRects) = 0 ;
    }
    else if (message == kHIShapeEnumerateRect)
    {
        (*m_numRects) += 1 ;
    }

    return noErr;
}

OSStatus wxOSXRegionToRectsSetterCallback(
    int message, HIShapeRef WXUNUSED(region), const CGRect *rect, void *data )
{
    if (message == kHIShapeEnumerateRect)
    {
        RegionToRectsCallbackData *cb = (RegionToRectsCallbackData*) data ;
        cb->m_rects[cb->m_current++] = wxRect( rect->origin.x , rect->origin.y , rect->size.width , rect->size.height ) ;
    }

    return noErr;
}

void wxRegionIterator::Reset(const wxRegion& region)
{
    m_current = 0;
    m_region = region;

    wxDELETEA(m_rects);

    if (m_region.IsEmpty())
    {
        m_numRects = 0;
    }
    else
    {
#if 0
        // fallback code in case we ever need it again
        // copying this to a path and dissecting the path would be an option
        m_numRects = 1;
        m_rects = new wxRect[m_numRects];
        m_rects[0] = m_region.GetBox();
#endif
        OSStatus err = HIShapeEnumerate (OTHER_M_REGION(region), kHIShapeParseFromTopLeft, wxOSXRegionToRectsCounterCallback,
            (void*)&m_numRects);
        if (err == noErr)
        {
            m_rects = new wxRect[m_numRects];
            RegionToRectsCallbackData data ;
            data.m_rects = m_rects ;
            data.m_current = 0 ;
            HIShapeEnumerate( OTHER_M_REGION(region), kHIShapeParseFromTopLeft, wxOSXRegionToRectsSetterCallback,
                (void*)&data );
        }
        else
        {
            m_numRects = 0;
        }
    }
}

/*!
 * Increment iterator. The rectangle returned is the one after the
 * incrementation.
 */
wxRegionIterator& wxRegionIterator::operator ++ ()
{
    if (m_current < m_numRects)
        ++m_current;

    return *this;
}

/*!
 * Increment iterator. The rectangle returned is the one before the
 * incrementation.
 */
wxRegionIterator wxRegionIterator::operator ++ (int)
{
    wxRegionIterator previous(*this);

    if (m_current < m_numRects)
        ++m_current;

    return previous;
}

long wxRegionIterator::GetX() const
{
    if (m_current < m_numRects)
        return m_rects[m_current].x;

    return 0;
}

long wxRegionIterator::GetY() const
{
    if (m_current < m_numRects)
        return m_rects[m_current].y;

    return 0;
}

long wxRegionIterator::GetW() const
{
    if (m_current < m_numRects)
        return m_rects[m_current].width ;

    return 0;
}

long wxRegionIterator::GetH() const
{
    if (m_current < m_numRects)
        return m_rects[m_current].height;

    return 0;
}

#endif
