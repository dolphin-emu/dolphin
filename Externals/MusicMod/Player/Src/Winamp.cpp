// =======================================================================================
// WndprocWinamp is called repeatedly when the cursor is moved over the main window
// =======================================================================================

////////////////////////////////////////////////////////////////////////////////
// Plainamp, Open source Winamp core
// 
// Copyright © 2005  Sebastian Pipping <webmaster@hartwork.org>
// 
// -->  http://www.hartwork.org
// 
// This source code is released under the GNU General Public License (GPL).
// See GPL.txt for details. Any non-GPL usage is strictly forbidden.
////////////////////////////////////////////////////////////////////////////////


#include "Winamp.h"
#include "Playback.h"
#include "Playlist.h"
#include "Console.h"
#include "Main.h"
#include "Winamp/wa_ipc.h"
#include "Winamp/wa_msgids.h"
#include "AddDirectory.h"
#include "AddFiles.h"
#include "Prefs.h"
#include "PluginManager.h"
#include "Embed.h"
#include "Unicode.h"
#include "zlib/zlib.h"
#include "Rebar.h"



int IPC_GENHOTKEYS_ADD = 0;
int ID_DYNAMICLIBRARY = 0;
int IPC_GETPLAINBARTARGET = 0;



////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////
LRESULT CALLBACK WndprocWinamp( HWND hwnd, UINT message, WPARAM wp, LPARAM lp )
{
	//Console::Append( TEXT( "Winamp.cc:WndprocWinamp was called" ) );

	switch( message )
	{
	case WM_COMMAND:
/*
		{
			TCHAR szBuffer[ 5000 ];
			_stprintf( szBuffer, TEXT( "WM_COMMAND <%i> <%i>" ), wp, lp );
			Console::Append( szBuffer );
		}
*/		
		switch( LOWORD( wp ) )
		{
		case WINAMP_FILE_QUIT:
			PostMessage( WindowMain, WM_SYSCOMMAND, SC_CLOSE, 0 );
			break;

		case WINAMP_OPTIONS_PREFS:
			Prefs::Show();
			break;
/*
		case WINAMP_OPTIONS_AOT: break;
		case WINAMP_FILE_REPEAT: break;
		case WINAMP_FILE_SHUFFLE: break;
		case WINAMP_HIGH_PRIORITY: break;
*/
		case WINAMP_FILE_PLAY:
			AddFiles();
			break;
/*
		case WINAMP_OPTIONS_EQ: break;
		case WINAMP_OPTIONS_ELAPSED: break;
		case WINAMP_OPTIONS_REMAINING: break;
		case WINAMP_OPTIONS_PLEDIT: break;
*/
		case WINAMP_HELP_ABOUT:
			About( hwnd );
			break;

		case WINAMP_MAINMENU:
			{
				POINT p;
				GetCursorPos( &p );

				CheckMenuItem(
					main_context_menu,
					PLAINAMP_TOGGLE_CONSOLE,
					IsWindowVisible( WindowConsole ) ? MF_CHECKED : MF_UNCHECKED
				);
				CheckMenuItem(
					main_context_menu,
					PLAINAMP_TOGGLE_MANAGER,
					IsWindowVisible( WindowManager ) ? MF_CHECKED : MF_UNCHECKED
				);
				
				TrackPopupMenu(
					main_context_menu,
					TPM_LEFTALIGN |
						TPM_TOPALIGN |
						TPM_RIGHTBUTTON,
					p.x,
					p.y,
					0,
					hwnd,
					NULL
				);

				break;
			}

		case WINAMP_BUTTON1:
			Playback::Prev();
			Playback::UpdateSeek();
			break;
	
		case WINAMP_BUTTON2:
			Playback::Play();
			Playback::UpdateSeek();
			break;
	
		case WINAMP_BUTTON3:
			Playback::Pause();
			Playback::UpdateSeek();
			break;
	
		case WINAMP_BUTTON4:
			Playback::Stop();
			Playback::UpdateSeek();
			break;
	
		case WINAMP_BUTTON5:
			Playback::Next();
			Playback::UpdateSeek();
			break;

/*	
		case WINAMP_BUTTON1_SHIFT: break;
		case WINAMP_BUTTON2_SHIFT: break;
		case WINAMP_BUTTON3_SHIFT: break;
		case WINAMP_BUTTON4_SHIFT: break;
		case WINAMP_BUTTON5_SHIFT: break;
		case WINAMP_BUTTON1_CTRL: break;
		case WINAMP_BUTTON2_CTRL: break;
		case WINAMP_BUTTON3_CTRL: break;
		case WINAMP_BUTTON4_CTRL: break;
		case WINAMP_BUTTON5_CTRL: break;
*/	
	
		case WINAMP_VOLUMEUP:
			Playback::Volume::Up();
			// TODO Update slider
			break;

		case WINAMP_VOLUMEDOWN:
			Playback::Volume::Down();
			// TODO Update slider
			break;

		case WINAMP_FFWD5S:
			Playback::Forward();
			Playback::UpdateSeek();
			break;
			
		case WINAMP_REW5S:
			Playback::Rewind();
			Playback::UpdateSeek();
			break;
/*
		case WINAMP_NEXT_WINDOW: break;
		case WINAMP_OPTIONS_WINDOWSHADE: break;
		case WINAMP_OPTIONS_DSIZE: break;
		case IDC_SORT_FILENAME: break;
		case IDC_SORT_FILETITLE: break;
		case IDC_SORT_ENTIREFILENAME: break;
		case IDC_SELECTALL: break;
		case IDC_SELECTNONE: break;
		case IDC_SELECTINV: break;
		case IDM_EQ_LOADPRE: break;
		case IDM_EQ_LOADMP3: break;
		case IDM_EQ_LOADDEFAULT: break;
		case IDM_EQ_SAVEPRE: break;
		case IDM_EQ_SAVEMP3: break;
		case IDM_EQ_SAVEDEFAULT: break;
		case IDM_EQ_DELPRE: break;
		case IDM_EQ_DELMP3: break;
		case IDC_PLAYLIST_PLAY: break;
		case WINAMP_FILE_LOC: break;
		case WINAMP_OPTIONS_EASYMOVE: break;
*/
		case WINAMP_FILE_DIR:
			AddDirectory();
			break;
/*
		case WINAMP_EDIT_ID3: break;
		case WINAMP_TOGGLE_AUTOSCROLL: break;
		case WINAMP_VISSETUP: break;
		case WINAMP_PLGSETUP: break;
		case WINAMP_VISPLUGIN: break;
		case WINAMP_JUMP: break;
		case WINAMP_JUMPFILE: break;
		case WINAMP_JUMP10FWD: break;
		case WINAMP_JUMP10BACK: break;
		case WINAMP_PREVSONG: break;
		case WINAMP_OPTIONS_EXTRAHQ: break;
*/
		case ID_PE_NEW:
			playlist->RemoveAll();
			break;

		case ID_PE_OPEN:
			Playlist::DialogOpen();
			break;
/*
		case ID_PE_SAVE: break;
*/
		case ID_PE_SAVEAS:
			Playlist::DialogSaveAs();
			break;

		case ID_PE_SELECTALL:
			playlist->SelectAll( true );
			break;

		case ID_PE_INVERT:
			playlist->SelectInvert();
			break;

		case ID_PE_NONE:
			playlist->SelectAll( false );
			break;
/*
		case ID_PE_ID3: break;
		case ID_PE_S_TITLE: break;
		case ID_PE_S_FILENAME: break;
		case ID_PE_S_PATH: break;
		case ID_PE_S_RANDOM: break;
		case ID_PE_S_REV: break;
*/
		case ID_PE_CLEAR:
			playlist->RemoveAll();
			break;
/*
		case ID_PE_MOVEUP: break;
		case ID_PE_MOVEDOWN: break;
		case WINAMP_SELSKIN: break;
		case WINAMP_VISCONF: break;
		case ID_PE_NONEXIST: break;
		case ID_PE_DELETEFROMDISK: break;
		case ID_PE_CLOSE: break;
		case WINAMP_VIS_SETOSC: break;
		case WINAMP_VIS_SETANA: break;
		case WINAMP_VIS_SETOFF: break;
		case WINAMP_VIS_DOTSCOPE: break;
		case WINAMP_VIS_LINESCOPE: break;
		case WINAMP_VIS_SOLIDSCOPE: break;
		case WINAMP_VIS_NORMANA: break;
		case WINAMP_VIS_FIREANA: break;
		case WINAMP_VIS_LINEANA: break;
		case WINAMP_VIS_NORMVU: break;
		case WINAMP_VIS_SMOOTHVU: break;
		case WINAMP_VIS_FULLREF: break;
		case WINAMP_VIS_FULLREF2: break;
		case WINAMP_VIS_FULLREF3: break;
		case WINAMP_VIS_FULLREF4: break;
		case WINAMP_OPTIONS_TOGTIME: break;
		case EQ_ENABLE: break;
		case EQ_AUTO: break;
		case EQ_PRESETS: break;
		case WINAMP_VIS_FALLOFF0: break;
		case WINAMP_VIS_FALLOFF1: break;
		case WINAMP_VIS_FALLOFF2: break;
		case WINAMP_VIS_FALLOFF3: break;
		case WINAMP_VIS_FALLOFF4: break;
		case WINAMP_VIS_PEAKS: break;
		case ID_LOAD_EQF: break;
		case ID_SAVE_EQF: break;
		case ID_PE_ENTRY: break;
		case ID_PE_SCROLLUP: break;
		case ID_PE_SCROLLDOWN: break;
		case WINAMP_MAIN_WINDOW: break;
		case WINAMP_VIS_PFALLOFF0: break;
		case WINAMP_VIS_PFALLOFF1: break;
		case WINAMP_VIS_PFALLOFF2: break;
		case WINAMP_VIS_PFALLOFF3: break;
		case WINAMP_VIS_PFALLOFF4: break;
		case ID_PE_TOP: break;
		case ID_PE_BOTTOM: break;
		case WINAMP_OPTIONS_WINDOWSHADE_PL: break;
		case EQ_INC1: break;
		case EQ_INC2: break;
		case EQ_INC3: break;
		case EQ_INC4: break;
		case EQ_INC5: break;
		case EQ_INC6: break;
		case EQ_INC7: break;
		case EQ_INC8: break;
		case EQ_INC9: break;
		case EQ_INC10: break;
		case EQ_INCPRE: break;
		case EQ_DECPRE: break;
		case EQ_DEC1: break;
		case EQ_DEC2: break;
		case EQ_DEC3: break;
		case EQ_DEC4: break;
		case EQ_DEC5: break;
		case EQ_DEC6: break;
		case EQ_DEC7: break;
		case EQ_DEC8: break;
		case EQ_DEC9: break;
		case EQ_DEC10: break;
		case ID_PE_SCUP: break;
		case ID_PE_SCDOWN: break;
		case WINAMP_REFRESHSKIN: break;
		case ID_PE_PRINT: break;
		case ID_PE_EXTINFO: break;
		case WINAMP_PLAYLIST_ADVANCE: break;
		case WINAMP_VIS_LIN: break;
		case WINAMP_VIS_BAR: break;
		case WINAMP_OPTIONS_MINIBROWSER: break;
		case MB_FWD: break;
		case MB_BACK: break;
		case MB_RELOAD: break;
		case MB_OPENMENU: break;
		case MB_OPENLOC: break;
		case WINAMP_NEW_INSTANCE: break;
		case MB_UPDATE: break;
		case WINAMP_OPTIONS_WINDOWSHADE_EQ: break;
		case EQ_PANLEFT: break;
		case EQ_PANRIGHT: break;
		case WINAMP_GETMORESKINS: break;
		case WINAMP_VIS_OPTIONS: break;
		case WINAMP_PE_SEARCH: break;
		case ID_PE_BOOKMARK: break;
		case WINAMP_EDIT_BOOKMARKS: break;
		case WINAMP_MAKECURBOOKMARK: break;
		case ID_MAIN_PLAY_BOOKMARK_NONE: break;
		case ID_MAIN_PLAY_AUDIOCD: break;
		case ID_MAIN_PLAY_AUDIOCD2: break;
		case ID_MAIN_PLAY_AUDIOCD3: break;
		case ID_MAIN_PLAY_AUDIOCD4: break;
		case WINAMP_OPTIONS_VIDEO: break;
		case ID_VIDEOWND_ZOOMFULLSCREEN: break;
		case ID_VIDEOWND_ZOOM100: break;
		case ID_VIDEOWND_ZOOM200: break;
		case ID_VIDEOWND_ZOOM50: break;
		case ID_VIDEOWND_VIDEOOPTIONS: break;
		case WINAMP_MINIMIZE: break;
		case ID_PE_FONTBIGGER: break;
		case ID_PE_FONTSMALLER: break;
		case WINAMP_VIDEO_TOGGLE_FS: break;
		case WINAMP_VIDEO_TVBUTTON: break;
		case WINAMP_LIGHTNING_CLICK: break;
		case ID_FILE_ADDTOLIBRARY: break;
		case ID_HELP_HELPTOPICS: break;
		case ID_HELP_GETTINGSTARTED: break;
		case ID_HELP_WINAMPFORUMS: break;
*/
		case ID_PLAY_VOLUMEUP:
			Playback::Volume::Up();
			// TODO Update slider
			break;

		case ID_PLAY_VOLUMEDOWN:
			Playback::Volume::Down();
			// TODO Update slider
			break;
/*			
		case ID_PEFILE_OPENPLAYLISTFROMLIBRARY_NOPLAYLISTSINLIBRARY: break;
		case ID_PEFILE_ADDFROMLIBRARY: break;
		case ID_PEFILE_CLOSEPLAYLISTEDITOR: break;
		case ID_PEPLAYLIST_PLAYLISTPREFERENCES: break;
		case ID_MLFILE_NEWPLAYLIST: break;
		case ID_MLFILE_LOADPLAYLIST: break;
		case ID_MLFILE_SAVEPLAYLIST: break;
		case ID_MLFILE_ADDMEDIATOLIBRARY: break;
		case ID_MLFILE_CLOSEMEDIALIBRARY: break;
		case ID_MLVIEW_NOWPLAYING: break;
		case ID_MLVIEW_LOCALMEDIA_ALLMEDIA: break;
		case ID_MLVIEW_LOCALMEDIA_AUDIO: break;
		case ID_MLVIEW_LOCALMEDIA_VIDEO: break;
		case ID_MLVIEW_PLAYLISTS_NOPLAYLISTINLIBRARY: break;
		case ID_MLVIEW_INTERNETRADIO: break;
		case ID_MLVIEW_INTERNETTV: break;
		case ID_MLVIEW_LIBRARYPREFERENCES: break;
		case ID_MLVIEW_DEVICES_NOAVAILABLEDEVICE: break;
		case ID_MLFILE_IMPORTCURRENTPLAYLIST: break;
		case ID_MLVIEW_MEDIA: break;
		case ID_MLVIEW_PLAYLISTS: break;
		case ID_MLVIEW_MEDIA_ALLMEDIA: break;
		case ID_MLVIEW_DEVICES: break;
		case ID_FILE_SHOWLIBRARY: break;
		case ID_FILE_CLOSELIBRARY: break;
		case ID_POST_PLAY_PLAYLIST: break;
		case ID_VIS_NEXT: break;
		case ID_VIS_PREV: break;
		case ID_VIS_RANDOM: break;
		case ID_MANAGEPLAYLISTS: break;
		case ID_PREFS_SKIN_SWITCHTOSKIN: break;
		case ID_PREFS_SKIN_DELETESKIN: break;
		case ID_PREFS_SKIN_RENAMESKIN: break;
		case ID_VIS_FS: break;
		case ID_VIS_CFG: break;
		case ID_VIS_MENU: break;
		case ID_VIS_SET_FS_FLAG: break;
		case ID_PE_SHOWPLAYING: break;
		case ID_HELP_REGISTERWINAMPPRO: break;
		case ID_PE_MANUAL_ADVANCE: break;
		case WA_SONG_5_STAR_RATING: break;
		case WA_SONG_4_STAR_RATING: break;
		case WA_SONG_3_STAR_RATING: break;
		case WA_SONG_2_STAR_RATING: break;
		case WA_SONG_1_STAR_RATING: break;
		case WA_SONG_NO_RATING: break;
		case PL_SONG_5_STAR_RATING: break;
		case PL_SONG_4_STAR_RATING: break;
		case PL_SONG_3_STAR_RATING: break;
		case PL_SONG_2_STAR_RATING: break;
		case PL_SONG_1_STAR_RATING: break;
		case PL_SONG_NO_RATING: break;
		case AUDIO_TRACK_ONE: break;
		case VIDEO_TRACK_ONE: break;
		case ID_SWITCH_COLOURTHEME: break;
		case ID_GENFF_LIMIT: break;

*/
		case PLAINAMP_TOGGLE_CONSOLE:
			ShowWindow( WindowConsole, IsWindowVisible( WindowConsole ) ? SW_HIDE : SW_SHOW );
			break;
			
		case PLAINAMP_TOGGLE_MANAGER:
			ShowWindow( WindowManager, IsWindowVisible( WindowManager ) ? SW_HIDE : SW_SHOW );
			break;

		case PLAINAMP_PL_REM_SEL:
			playlist->RemoveSelected( true );			
			break;
			
		case PLAINAMP_PL_REM_CROP:
			playlist->RemoveSelected( false );
			break;

		default:
			{
				/*
				if( wp == ID_DYNAMICLIBRARY )
				{
					// Stupid dnylib workaround
					PostMessage( hwnd, WM_COMMAND, ID_DYNAMICLIBRARY | ( 1 << 16 ), 0 );
				}
				*/
				
				if( LOWORD( wp ) < 40001 ) break;
				
				TCHAR szBuffer[ 5000 ];
				_stprintf( szBuffer, TEXT( "WM_COMMAND <%i> <%i>" ), wp, lp );
				Console::Append( szBuffer );
				Console::Append( TEXT( "NOT handled" ) );
				Console::Append( " " );
			}
			
		}
		break;
	
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
	
	case WM_WA_IPC:
/*
		{
			TCHAR szBuffer[ 5000 ];
			if( lp != 2006 )
			{
				_stprintf( szBuffer, TEXT( "WM_WA_IPC <%i> <%i>" ), wp, lp );
			}
			else
			{
				// Hotkey sent as strings!
				// Idea: make stl map
				_stprintf( szBuffer, TEXT( "WM_WA_IPC <%s> <%i>" ), ( char * )wp, lp );
			}
			Console::Append( szBuffer );
		}
*/		
		switch( lp )
		{
		case IPC_GETVERSION:
			return 0x5010; // 5.10
/*
		case IPC_GETREGISTEREDVERSION: break;
		case IPC_PLAYFILE: break;
		case IPC_ENQUEUEFILE: break;
*/
		case IPC_DELETE:
			playlist->RemoveAll();
			break;
/*
		case IPC_DELETE_INT: break;
*/
		case IPC_STARTPLAY:
			Playback::Play();
			Playback::UpdateSeek();
			break;
/*
		case IPC_STARTPLAY_INT: break;
		case IPC_CHDIR: break;
*/
		case IPC_ISPLAYING: // untested
			return ( Playback::IsPlaying() ? ( Playback::IsPaused() ? 3 : 1 ) : 0 );
/*
		case IPC_GETOUTPUTTIME: break;
		case IPC_JUMPTOTIME: break;
		case IPC_GETMODULENAME: break;
		case IPC_EX_ISRIGHTEXE: break;
		case IPC_WRITEPLAYLIST: break;
*/
		case IPC_SETPLAYLISTPOS:
			playlist->SetCurIndex( ( int )wp );
			break;

		case IPC_SETVOLUME: break;
			Playback::Volume::Set( ( int )wp );
			// TODO Update slider
			break;
			
		case IPC_SETPANNING: break;
			Playback::Pan::Set( ( int )wp );
			// TODO Update slider
			break;

		case IPC_GETLISTLENGTH:
			return playlist->GetSize();
	
		case IPC_GETLISTPOS:
			return playlist->GetCurIndex();
/*
		case IPC_GETINFO: break;
		case IPC_GETEQDATA: break;
			// TODO
		case IPC_SETEQDATA: break;
			// TODO
		case IPC_ADDBOOKMARK: break;
		case IPC_INSTALLPLUGIN: break;
		case IPC_RESTARTWINAMP: break;
		case IPC_ISFULLSTOP: break;
		case IPC_INETAVAILABLE: break;
		case IPC_UPDTITLE: break;
		case IPC_REFRESHPLCACHE: break;
		case IPC_GET_SHUFFLE: break;
		case IPC_GET_REPEAT: break;
		case IPC_SET_SHUFFLE: break;
		case IPC_SET_REPEAT: break;
		case IPC_ENABLEDISABLE_ALL_WINDOWS: break;
*/
		case IPC_GETWND:
			{
				TCHAR szBuffer[ 5000 ];
				_stprintf( szBuffer, TEXT( "IPC_GETWND <%i>" ), wp );
				Console::Append( szBuffer );
				Console::Append( " " );
			}

			switch( wp )
			{
			case IPC_GETWND_EQ:     break;
			case IPC_GETWND_PE:     return ( LRESULT )WindowMain;
			case IPC_GETWND_MB:     break;
			case IPC_GETWND_VIDEO:  break;
			}

			return ( LRESULT )NULL;
			
		case IPC_ISWNDVISIBLE:
			{
				TCHAR szBuffer[ 5000 ];
				_stprintf( szBuffer, TEXT( "IPC_ISWNDVISIBLE <%i>" ), wp );
				Console::Append( szBuffer );
				Console::Append( " " );
			}

			switch( wp )
			{
			case IPC_GETWND_EQ:     break;
			case IPC_GETWND_PE:     return 1;
			case IPC_GETWND_MB:     break;
			case IPC_GETWND_VIDEO:  break;
			}

			return 0;
/*
		case IPC_SETSKIN: break;
		case IPC_GETSKIN: break;
		case IPC_EXECPLUG: break;
*/
		case IPC_GETPLAYLISTFILE:
			{
				static char szAnsiFilename[ 2000 ] = "\0";
				Playlist::GetFilename( ( int )wp, szAnsiFilename, 1999 );
				return ( LRESULT )szAnsiFilename;
			}
			
		case IPC_GETPLAYLISTTITLE:
			{	
				static char szAnsiTitle[ 2000 ] = "\0";
				Playlist::GetTitle( ( int )wp, szAnsiTitle, 1999 );
				return ( LRESULT )szAnsiTitle;
			}
/*
		case IPC_GETHTTPGETTER: break;
		case IPC_MBOPEN: break;
		case IPC_CHANGECURRENTFILE: break;
		case IPC_GETMBURL: break;
		case IPC_MBBLOCK: break;
		case IPC_MBOPENREAL: break;
		case IPC_ADJUST_OPTIONSMENUPOS: break;
*/
		case IPC_GET_HMENU:
			{
				switch( wp )
				{
				case 0:
					return ( LRESULT )main_context_menu;
/*
				case 1: break;
				case 2: break;
				case 3: break;
				case 4: break;
*/
				}
				return ( LRESULT )NULL;
			}

		case IPC_GET_EXTENDED_FILE_INFO:
			Console::Append( "IPC_GET_EXTENDED_FILE_INFO" );
			Console::Append( TEXT( "NOT handled" ) );
			Console::Append( " " );
			break;
/*
		case IPC_GET_EXTENDED_FILE_INFO_HOOKABLE: break;
		case IPC_GET_BASIC_FILE_INFO: break;
*/
		case IPC_GET_EXTLIST:
			// TODO
			return ( LRESULT )GlobalAlloc( GMEM_ZEROINIT, 2 ); // "\0\0"
/*			
		case IPC_INFOBOX: break;
		case IPC_SET_EXTENDED_FILE_INFO: break;
		case IPC_WRITE_EXTENDED_FILE_INFO: break;
		case IPC_FORMAT_TITLE: break;
*/
// =======================================================================================
// Let's remove this
/*
		case IPC_GETUNCOMPRESSINTERFACE:
			if( wp == 0x10100000 )
			{
				Console::Append( "IPC_GETUNCOMPRESSINTERFACE @ wa_inflate_struct" );
				Console::Append( TEXT( "NOT handled" ) );
				Console::Append( " " );
			}
			else
			{
				Console::Append( "IPC_GETUNCOMPRESSINTERFACE @ zlib" );
				Console::Append( " " );
				return ( LRESULT )uncompress;
			}
			break;
*/
// =======================================================================================

		case IPC_ADD_PREFS_DLG:
			Prefs::AddPage( ( prefsDlgRec * )wp );
			break;
/*
		case IPC_REMOVE_PREFS_DLG: break;
*/
		case IPC_OPENPREFSTOPAGE:
			Prefs::Show( ( prefsDlgRec * )wp );
			break;

		case IPC_GETINIFILE:
			{
				static char szWinampInipath[ MAX_PATH ] = "";
				if( *szWinampInipath == '\0' )
				{
					GetModuleFileNameA( NULL, szWinampInipath, MAX_PATH - 1 );
					char * szWalk = szWinampInipath + strlen( szWinampInipath ) - 1;
					while( ( szWalk > szWinampInipath ) && ( *szWalk != '.' ) ) szWalk--;
					szWalk++;
					strcpy( szWalk, "ini" );
				}
				return ( LRESULT )szWinampInipath;
			}

		case IPC_GETINIDIRECTORY:
			{
				// TODO: trailing slash or not???
				static char szPluginInipath[ MAX_PATH ] = "";
				if( *szPluginInipath == '\0' )
				{
					GetModuleFileNameA( NULL, szPluginInipath, MAX_PATH - 1 );
					char * szWalk = szPluginInipath + strlen( szPluginInipath ) - 1;
					while( ( szWalk > szPluginInipath ) && ( *szWalk != '\\' ) ) szWalk--;
					szWalk++;
					strcpy( szWalk, TEXT( "Plugins" ) );
				}
				return ( LRESULT )szPluginInipath;
			}
/*
		case IPC_SPAWNBUTTONPOPUP: break;
		case IPC_OPENURLBOX: break;
*/
		case IPC_OPENFILEBOX:
			AddFiles();
			break;

		case IPC_OPENDIRBOX:
			AddDirectory();
			break;

		case IPC_GET_GENSKINBITMAP:
			{
				TCHAR szBuffer[ 5000 ];
				_stprintf( szBuffer, TEXT( "IPC_GET_GENSKINBITMAP <%i>" ), wp );
				Console::Append( szBuffer );

				switch( wp )
				{
				case 0:
					{
						// TODO: make it work
						
						// Gen.bmp?
						static HBITMAP hBitmap = NULL;
						static BYTE * image = NULL;
						if( !hBitmap )
						{
							const int iWidth   = 200; // 194;
							const int iHeight  = 300; // 109;
							const int bpp      = 24;
							int bytes_per_row = iWidth * ( bpp / 8 );
							const int diff = bytes_per_row % 4;
							if( diff ) bytes_per_row += ( 4 - diff );
							
							const int size_in_bytes = bytes_per_row * iHeight;
							image = new BYTE[ size_in_bytes ];

							hBitmap = CreateBitmap(
								iWidth,   // int nWidth
								iHeight,  // int nHeight
								1,        // UINT cPlanes
								bpp,      // UINT cBitsPerPel
								image     // CONST VOID * lpvBits
							);

							memset( image, 255, size_in_bytes );
						}
						
						return ( LRESULT )hBitmap;
					}
					
				default:
					{
						Console::Append( TEXT( "NOT handled" ) );
					}

				}
				Console::Append( TEXT( " " ) );
				
				break;
			}

		case IPC_GET_EMBEDIF:
			// TODO
			if( !wp )
			{
				return ( LRESULT )Embed::Embed;
			}
			else
			{
				return ( LRESULT )Embed::Embed( ( embedWindowState * )wp );
			}
			break;
/*		
		case IPC_EMBED_ENUM: break;
		case IPC_EMBED_ISVALID: break;
		case IPC_CONVERTFILE: break;
		case IPC_CONVERTFILE_END: break;
		case IPC_CONVERT_CONFIG: break;
		case IPC_CONVERT_CONFIG_END: break;
		case IPC_GETSADATAFUNC: break;
		case IPC_ISMAINWNDVISIBLE: break;
		case IPC_SETPLEDITCOLORS: break;
		case IPC_SPAWNEQPRESETMENU: break;
		case IPC_SPAWNFILEMENU: break;
		case IPC_SPAWNOPTIONSMENU: break;
		case IPC_SPAWNWINDOWSMENU: break;
		case IPC_SPAWNHELPMENU: break;
		case IPC_SPAWNPLAYMENU: break;
		case IPC_SPAWNPEFILEMENU: break;
		case IPC_SPAWNPEPLAYLISTMENU: break;
		case IPC_SPAWNPESORTMENU: break;
		case IPC_SPAWNPEHELPMENU: break;
		case IPC_SPAWNMLFILEMENU: break;
		case IPC_SPAWNMLVIEWMENU: break;
		case IPC_SPAWNMLHELPMENU: break;
		case IPC_SPAWNPELISTOFPLAYLISTS: break;
		case WM_WA_SYSTRAY: break;
		case IPC_IS_PLAYING_VIDEO: break;
		case IPC_GET_IVIDEOOUTPUT: break;
*/
		case IPC_CB_ONSHOWWND:
		case IPC_CB_ONHIDEWND:
		case IPC_CB_GETTOOLTIP:
		case IPC_CB_MISC:
		case IPC_CB_CONVERT_STATUS:
		case IPC_CB_CONVERT_DONE:
			break;
/*
		case IPC_ADJUST_FFWINDOWSMENUPOS: break;
		case IPC_ISDOUBLESIZE: break;
		case IPC_ADJUST_FFOPTIONSMENUPOS: break;
*/
		case IPC_GETTIMEDISPLAYMODE:
			return 0; // == elapsed time
/*
		case IPC_SETVISWND: break;
		case IPC_GETVISWND: break;
		case IPC_ISVISRUNNING: break;
*/
		case IPC_CB_VISRANDOM: break;
/*
		case IPC_SETIDEALVIDEOSIZE: break;
		case IPC_GETSTOPONVIDEOCLOSE: break;
		case IPC_SETSTOPONVIDEOCLOSE: break;
*/
		case IPC_TRANSLATEACCELERATOR:
			Console::Append( TEXT( "IPC_TRANSLATEACCELERATOR" ) );
			Console::Append( TEXT( "NOT handled" ) );
			Console::Append( TEXT( " " ) );
			break;

		case IPC_CB_ONTOGGLEAOT: break;
/*
		case IPC_GETPREFSWND: break;
		case IPC_SET_PE_WIDTHHEIGHT: break;
		case IPC_GETLANGUAGEPACKINSTANCE: break;
*/
		case IPC_CB_PEINFOTEXT:
		case IPC_CB_OUTPUTCHANGED:
			break;
/*
		case IPC_GETOUTPUTPLUGIN: break;
		case IPC_SETDRAWBORDERS: break;
		case IPC_DISABLESKINCURSORS: break;
*/
		case IPC_CB_RESETFONT: break;
/*
		case IPC_IS_FULLSCREEN: break;
		case IPC_SET_VIS_FS_FLAG: break;
		case IPC_SHOW_NOTIFICATION: break;
		case IPC_GETSKININFO: break;
		case IPC_GET_MANUALPLADVANCE: break;
		case IPC_SET_MANUALPLADVANCE: break;
		case IPC_GET_NEXT_PLITEM: break;
		case IPC_GET_PREVIOUS_PLITEM: break;
		case IPC_IS_WNDSHADE: break;
		case IPC_SETRATING: break;
		case IPC_GETRATING: break;
		case IPC_GETNUMAUDIOTRACKS: break;
		case IPC_GETNUMVIDEOTRACKS: break;
		case IPC_GETAUDIOTRACK: break;
		case IPC_GETVIDEOTRACK: break;
		case IPC_SETAUDIOTRACK: break;
		case IPC_SETVIDEOTRACK: break;
		case IPC_PUSH_DISABLE_EXIT: break;
		case IPC_POP_DISABLE_EXIT: break;
		case IPC_IS_EXIT_ENABLED: break;
		case IPC_IS_AOT: break;
		case IPC_PLCMD: break;
		case IPC_MBCMD: break;
		case IPC_VIDCMD: break;
		case IPC_MBURL: break;
		case IPC_MBGETCURURL: break;
		case IPC_MBGETDESC: break;
		case IPC_MBCHECKLOCFILE: break;
		case IPC_MBREFRESH: break;
		case IPC_MBGETDEFURL: break;
		case IPC_STATS_LIBRARY_ITEMCNT: break;
		case IPC_FF_FIRST: break;
		case IPC_FF_LAST: break;
		case IPC_GETDROPTARGET: break;
		case IPC_PLAYLIST_MODIFIED: break;
		case IPC_PLAYING_FILE: break;
		case IPC_FILE_TAG_MAY_HAVE_UPDATED: break;
		case IPC_ALLOW_PLAYTRACKING: break;
*/
		case IPC_HOOK_OKTOQUIT:
			return 1; // Okay
/*
		case IPC_WRITECONFIG: break;
*/
		case IPC_REGISTER_WINAMP_IPCMESSAGE:
			{
				// TODO convert to TCHAR????
				UINT res = RegisterWindowMessage( ( LPCTSTR )wp );
				
				TCHAR szBuffer[ 5000 ];
				_stprintf( szBuffer, TEXT( "Message \"%s\" registered as #%i" ), wp, res );
				Console::Append( szBuffer );
				Console::Append( " " );
				
				if( !stricmp( ( char * )wp, "GenHotkeysAdd" ) )
				{
					IPC_GENHOTKEYS_ADD = res;
				}
				else if( !stricmp( ( char * )wp, "Dynamic Library" ) )
				{
					ID_DYNAMICLIBRARY = res;
				}
				else if( !stricmp( ( char * )wp, "IPC_GETPLAINBARTARGET" ) )
				{
					IPC_GETPLAINBARTARGET = res;
				}
				
				return res;
			}
			
		case 2006: // undocumented, name IPC_CB_HOTKEY or so later and ask to add this to sdk
			{
				// Hotkey sent as strings!
				// Idea: make stl map
				TCHAR szBuffer[ 5000 ];
				_stprintf( szBuffer, TEXT( "Hotkey \"%s\" detected" ), wp );
				Console::Append( szBuffer );
				Console::Append( " " );
				
				return 1; // Accept???
			}

		default:
			{
				if( lp == IPC_GENHOTKEYS_ADD )
				{
					break;
				}
				else if( lp == IPC_GETPLAINBARTARGET )
				{
					return ( LRESULT )( IsWindowVisible( WindowVis ) ? WindowVis : NULL );
				}

				TCHAR szBuffer[ 5000 ];
				_stprintf( szBuffer, TEXT( "WM_WA_IPC <%i> <%i>" ), wp, lp );
				Console::Append( szBuffer );
				Console::Append( TEXT( "NOT handled" ) );
				Console::Append( " " );
			}
		}
		break;

	case WM_WA_MPEG_EOF:
		Playback::NotifyTrackEnd();
		Playback::Next();
		break;
		
	case WM_COPYDATA:
		{
			if( !lp ) return FALSE;

			COPYDATASTRUCT * cds = ( COPYDATASTRUCT * )lp;
			switch( cds->dwData )
			{
			case IPC_PLAYFILE:
				{
					const int iLen = cds->cbData;
					if( !iLen ) return FALSE;
					TCHAR * szKeep = new TCHAR[ iLen + 1 ];
					ToTchar( szKeep, ( char * )cds->lpData, iLen );
					szKeep[ iLen ] = TEXT( '\0' );
					
					playlist->PushBack( szKeep );
					return TRUE;
				}	
			}
			return FALSE;
		}
	
	}
	return DefWindowProc( hwnd, message, wp, lp );
}
