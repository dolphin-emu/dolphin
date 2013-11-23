/////////////////////////////////////////////////////////////////////////////
// Name:        src/osx/carbon/uma.cpp
// Purpose:     UMA support
// Author:      Stefan Csomor
// Modified by:
// Created:     04/01/98
// Copyright:   (c) Stefan Csomor
// Licence:     The wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#include "wx/wxprec.h"

#include "wx/osx/uma.h"

#if wxUSE_GUI

#include "wx/toplevel.h"
#include "wx/dc.h"

#include "wx/osx/uma.h"

// menu manager

#if wxOSX_USE_CARBON

MenuRef UMANewMenu( SInt16 id , const wxString& title , wxFontEncoding encoding )
{
    wxString str = wxStripMenuCodes( title ) ;
    MenuRef menu ;

    CreateNewMenu( id , 0 , &menu ) ;
    SetMenuTitleWithCFString( menu , wxCFStringRef(str , encoding ) ) ;

    return menu ;
}

void UMASetMenuTitle( MenuRef menu , const wxString& title , wxFontEncoding encoding )
{
    wxString str = wxStripMenuCodes( title ) ;

    SetMenuTitleWithCFString( menu , wxCFStringRef(str , encoding) ) ;
}

void UMASetMenuItemText( MenuRef menu,  MenuItemIndex item, const wxString& title, wxFontEncoding encoding )
{
    // we don't strip the accels here anymore, must be done before
    wxString str = title ;

    SetMenuItemTextWithCFString( menu , item , wxCFStringRef(str , encoding) ) ;
}

void UMAEnableMenuItem( MenuRef inMenu , MenuItemIndex inItem , bool enable)
{
    if ( enable )
        EnableMenuItem( inMenu , inItem ) ;
    else
        DisableMenuItem( inMenu , inItem ) ;
}

void UMAAppendSubMenuItem( MenuRef menu , const wxString& title, wxFontEncoding encoding , MenuRef submenu )
{
    AppendMenuItemTextWithCFString( menu,
                                CFSTR("A"), 0, 0,NULL);
    UMASetMenuItemText( menu, (SInt16) ::CountMenuItems(menu), title , encoding );
    SetMenuItemHierarchicalMenu( menu , CountMenuItems( menu ) , submenu ) ;
    SetMenuTitleWithCFString(submenu , wxCFStringRef(title , encoding) );
}

void UMAInsertSubMenuItem( MenuRef menu , const wxString& title, wxFontEncoding encoding , MenuItemIndex item , SInt16 id  )
{
    InsertMenuItemTextWithCFString( menu,
                CFSTR("A"), item, 0, 0);

    UMASetMenuItemText( menu, item+1, title , encoding );
    SetMenuItemHierarchicalID( menu , item+1 , id ) ;
}

void UMASetMenuItemShortcut( MenuRef menu , MenuItemIndex item , wxAcceleratorEntry *entry )
{
    if ( !entry )
    {
        SetMenuItemCommandKey(menu, item, false, 0); 
        return ;
    }

    UInt8 modifiers = 0 ;
    SInt16 key = entry->GetKeyCode() ;
    if ( key )
    {
        bool explicitCommandKey = (entry->GetFlags() & wxACCEL_CTRL);

        if (entry->GetFlags() & wxACCEL_ALT)
            modifiers |= kMenuOptionModifier ;

        if (entry->GetFlags() & wxACCEL_SHIFT)
            modifiers |= kMenuShiftModifier ;

        if (entry->GetFlags() & wxACCEL_RAW_CTRL)
            modifiers |= kMenuControlModifier ;
        
        SInt16 glyph = 0 ;
        SInt16 macKey = key ;
        if ( key >= WXK_F1 && key <= WXK_F15 )
        {
            if ( !explicitCommandKey )
                modifiers |= kMenuNoCommandModifier ;

            // for some reasons this must be 0 right now
            // everything else leads to just the first function key item
            // to be selected. Thanks to Ryan Wilcox for finding out.
            macKey = 0 ;
            glyph = kMenuF1Glyph + ( key - WXK_F1 ) ;
            if ( key >= WXK_F13 )
                glyph += 12 ;
        }
        else
        {
            switch ( key )
            {
                case WXK_BACK :
                    macKey = kBackspaceCharCode ;
                    glyph = kMenuDeleteLeftGlyph ;
                    break ;

                case WXK_TAB :
                    macKey = kTabCharCode ;
                    glyph = kMenuTabRightGlyph ;
                    break ;

                case kEnterCharCode :
                    macKey = kEnterCharCode ;
                    glyph = kMenuEnterGlyph ;
                    break ;

                case WXK_RETURN :
                    macKey = kReturnCharCode ;
                    glyph = kMenuReturnGlyph ;
                    break ;

                case WXK_ESCAPE :
                    macKey = kEscapeCharCode ;
                    glyph = kMenuEscapeGlyph ;
                    break ;

                case WXK_SPACE :
                    macKey = ' ' ;
                    glyph = kMenuSpaceGlyph ;
                    break ;

                case WXK_DELETE :
                    macKey = kDeleteCharCode ;
                    glyph = kMenuDeleteRightGlyph ;
                    break ;

                case WXK_CLEAR :
                    macKey = kClearCharCode ;
                    glyph = kMenuClearGlyph ;
                    break ;

                case WXK_PAGEUP :
                    macKey = kPageUpCharCode ;
                    glyph = kMenuPageUpGlyph ;
                    break ;

                case WXK_PAGEDOWN :
                    macKey = kPageDownCharCode ;
                    glyph = kMenuPageDownGlyph ;
                    break ;

                case WXK_LEFT :
                    macKey = kLeftArrowCharCode ;
                    glyph = kMenuLeftArrowGlyph ;
                    break ;

                case WXK_UP :
                    macKey = kUpArrowCharCode ;
                    glyph = kMenuUpArrowGlyph ;
                    break ;

                case WXK_RIGHT :
                    macKey = kRightArrowCharCode ;
                    glyph = kMenuRightArrowGlyph ;
                    break ;

                case WXK_DOWN :
                    macKey = kDownArrowCharCode ;
                    glyph = kMenuDownArrowGlyph ;
                    break ;

                case WXK_HOME :
                    macKey = kHomeCharCode ;
                    glyph = kMenuNorthwestArrowGlyph ;
                    break ;

                case WXK_END :
                    macKey = kEndCharCode ;
                    glyph = kMenuSoutheastArrowGlyph ;
                    break ;
                default :
                    macKey = toupper( key ) ;
                    break ;
            }

            // we now allow non command key shortcuts
            // remove in case this gives problems
            if ( !explicitCommandKey )
                modifiers |= kMenuNoCommandModifier ;
        }

        // 1d and 1e have special meaning to SetItemCmd, so
        // do not use for these character codes.
        if (key != WXK_UP && key != WXK_RIGHT && key != WXK_DOWN && key != WXK_LEFT)
            SetItemCmd( menu, item , macKey );

        SetMenuItemModifiers( menu, item , modifiers ) ;

        if ( glyph )
            SetMenuItemKeyGlyph( menu, item , glyph ) ;
    }
}

void UMAAppendMenuItem( MenuRef menu , const wxString& title, wxFontEncoding encoding , wxAcceleratorEntry *entry )
{
    AppendMenuItemTextWithCFString( menu,
                                CFSTR("A"), 0, 0,NULL);
    // don't attempt to interpret metacharacters like a '-' at the beginning (would become a separator otherwise)
    ChangeMenuItemAttributes( menu , ::CountMenuItems(menu), kMenuItemAttrIgnoreMeta , 0 ) ;
    UMASetMenuItemText(menu, (SInt16) ::CountMenuItems(menu), title , encoding );
    UMASetMenuItemShortcut( menu , (SInt16) ::CountMenuItems(menu), entry ) ;
}

void UMAInsertMenuItem( MenuRef menu , const wxString& title, wxFontEncoding encoding , MenuItemIndex item , wxAcceleratorEntry *entry )
{
    InsertMenuItemTextWithCFString( menu,
                CFSTR("A"), item, 0, 0);

    // don't attempt to interpret metacharacters like a '-' at the beginning (would become a separator otherwise)
    ChangeMenuItemAttributes( menu , item+1, kMenuItemAttrIgnoreMeta , 0 ) ;
    UMASetMenuItemText(menu, item+1 , title , encoding );
    UMASetMenuItemShortcut( menu , item+1 , entry ) ;
}

static OSStatus UMAGetHelpMenu(
                               MenuRef *        outHelpMenu,
                               MenuItemIndex *  outFirstCustomItemIndex,
                               bool             allowHelpMenuCreation);

static OSStatus UMAGetHelpMenu(
                               MenuRef *        outHelpMenu,
                               MenuItemIndex *  outFirstCustomItemIndex,
                               bool             allowHelpMenuCreation)
{
    static bool s_createdHelpMenu = false ;

    if ( !s_createdHelpMenu && !allowHelpMenuCreation )
    {
        return paramErr ;
    }

    OSStatus status = HMGetHelpMenu( outHelpMenu , outFirstCustomItemIndex ) ;
    s_createdHelpMenu = ( status == noErr ) ;
    return status ;
}

OSStatus UMAGetHelpMenu(
                        MenuRef *        outHelpMenu,
                        MenuItemIndex *  outFirstCustomItemIndex)
{
    return UMAGetHelpMenu( outHelpMenu , outFirstCustomItemIndex , true );
}

OSStatus UMAGetHelpMenuDontCreate(
                                  MenuRef *        outHelpMenu,
                                  MenuItemIndex *  outFirstCustomItemIndex)
{
    return UMAGetHelpMenu( outHelpMenu , outFirstCustomItemIndex , false );
}

#endif

#endif  // wxUSE_GUI
