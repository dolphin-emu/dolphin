/*
** wa_msgids.h (created 14/04/2004 12:23:19 PM)
** Created from wa_ipc.h and resource.h from the language sdk
** by Darren Owen aka DrO
**
** This a simple header file which defines the message ids to allow you to control
** Winamp in keeping with the old frontend.h (R.I.P.)
**
**
** Version History:
**
** v1.0  ::  intial version with ids for Winamp 5.02+
** v1.0a ::  fixed the file to work on compile
** v1.1  ::  added the msg id for 'Manual Playlist Advance'
** v1.2  ::  added in song rating menu items
**
**
** How to use:
**
** To send these, use the format:
**
** SendMessage(hwnd_winamp, WM_COMMAND,command_name,0);
**
** For other languages such as Visual Basic, Pascal, etc you will need to use
** the equivalent calling SendMessage(..) calling convention
**
**
** Notes:
**
** IDs 42000 to 45000 are reserved for gen_ff
** IDs from 45000 to 57000 are reserved for library 
**
*/

#ifndef _WA_MSGIDS_H_
#define _WA_MSGIDS_H_

#define WINAMP_FILE_QUIT                40001
#define WINAMP_OPTIONS_PREFS            40012 // pops up the preferences
#define WINAMP_OPTIONS_AOT              40019 // toggles always on top
#define WINAMP_FILE_REPEAT              40022
#define WINAMP_FILE_SHUFFLE             40023
#define WINAMP_HIGH_PRIORITY            40025
#define WINAMP_FILE_PLAY                40029 // pops up the load file(s) box
#define WINAMP_OPTIONS_EQ               40036 // toggles the EQ window
#define WINAMP_OPTIONS_ELAPSED          40037
#define WINAMP_OPTIONS_REMAINING        40038
#define WINAMP_OPTIONS_PLEDIT           40040 // toggles the playlist window
#define WINAMP_HELP_ABOUT               40041 // pops up the about box :)
#define WINAMP_MAINMENU                 40043


/* the following are the five main control buttons, with optionally shift 
** or control pressed
** (for the exact functions of each, just try it out)
*/
#define WINAMP_BUTTON1                  40044
#define WINAMP_BUTTON2                  40045
#define WINAMP_BUTTON3                  40046
#define WINAMP_BUTTON4                  40047
#define WINAMP_BUTTON5                  40048
#define WINAMP_BUTTON1_SHIFT            40144
#define WINAMP_BUTTON2_SHIFT            40145
#define WINAMP_BUTTON3_SHIFT            40146
#define WINAMP_BUTTON4_SHIFT            40147
#define WINAMP_BUTTON5_SHIFT            40148
#define WINAMP_BUTTON1_CTRL             40154
#define WINAMP_BUTTON2_CTRL             40155
#define WINAMP_BUTTON3_CTRL             40156
#define WINAMP_BUTTON4_CTRL             40157
#define WINAMP_BUTTON5_CTRL             40158

#define WINAMP_VOLUMEUP                 40058 // turns the volume up a little
#define WINAMP_VOLUMEDOWN               40059 // turns the volume down a little
#define WINAMP_FFWD5S                   40060 // fast forwards 5 seconds
#define WINAMP_REW5S                    40061 // rewinds 5 seconds
#define WINAMP_NEXT_WINDOW              40063
#define WINAMP_OPTIONS_WINDOWSHADE      40064
#define WINAMP_OPTIONS_DSIZE            40165
#define IDC_SORT_FILENAME               40166
#define IDC_SORT_FILETITLE              40167
#define IDC_SORT_ENTIREFILENAME         40168
#define IDC_SELECTALL                   40169
#define IDC_SELECTNONE                  40170
#define IDC_SELECTINV                   40171
#define IDM_EQ_LOADPRE                  40172
#define IDM_EQ_LOADMP3                  40173
#define IDM_EQ_LOADDEFAULT              40174
#define IDM_EQ_SAVEPRE                  40175
#define IDM_EQ_SAVEMP3                  40176
#define IDM_EQ_SAVEDEFAULT              40177
#define IDM_EQ_DELPRE                   40178
#define IDM_EQ_DELMP3                   40180
#define IDC_PLAYLIST_PLAY               40184
#define WINAMP_FILE_LOC                 40185
#define WINAMP_OPTIONS_EASYMOVE         40186
#define WINAMP_FILE_DIR                 40187 // pops up the load directory box
#define WINAMP_EDIT_ID3                 40188
#define WINAMP_TOGGLE_AUTOSCROLL        40189
#define WINAMP_VISSETUP                 40190
#define WINAMP_PLGSETUP                 40191
#define WINAMP_VISPLUGIN                40192
#define WINAMP_JUMP                     40193
#define WINAMP_JUMPFILE                 40194
#define WINAMP_JUMP10FWD                40195
#define WINAMP_JUMP10BACK               40197
#define WINAMP_PREVSONG                 40198
#define WINAMP_OPTIONS_EXTRAHQ          40200
#define ID_PE_NEW                       40201
#define ID_PE_OPEN                      40202
#define ID_PE_SAVE                      40203
#define ID_PE_SAVEAS                    40204
#define ID_PE_SELECTALL                 40205
#define ID_PE_INVERT                    40206
#define ID_PE_NONE                      40207
#define ID_PE_ID3                       40208
#define ID_PE_S_TITLE                   40209
#define ID_PE_S_FILENAME                40210
#define ID_PE_S_PATH                    40211
#define ID_PE_S_RANDOM                  40212
#define ID_PE_S_REV                     40213
#define ID_PE_CLEAR                     40214
#define ID_PE_MOVEUP                    40215
#define ID_PE_MOVEDOWN                  40216
#define WINAMP_SELSKIN                  40219
#define WINAMP_VISCONF                  40221
#define ID_PE_NONEXIST                  40222
#define ID_PE_DELETEFROMDISK            40223
#define ID_PE_CLOSE                     40224
#define WINAMP_VIS_SETOSC               40226
#define WINAMP_VIS_SETANA               40227
#define WINAMP_VIS_SETOFF               40228
#define WINAMP_VIS_DOTSCOPE             40229
#define WINAMP_VIS_LINESCOPE            40230
#define WINAMP_VIS_SOLIDSCOPE           40231
#define WINAMP_VIS_NORMANA              40233
#define WINAMP_VIS_FIREANA              40234
#define WINAMP_VIS_LINEANA              40235
#define WINAMP_VIS_NORMVU               40236
#define WINAMP_VIS_SMOOTHVU             40237
#define WINAMP_VIS_FULLREF              40238
#define WINAMP_VIS_FULLREF2             40239
#define WINAMP_VIS_FULLREF3             40240
#define WINAMP_VIS_FULLREF4             40241
#define WINAMP_OPTIONS_TOGTIME          40242
#define EQ_ENABLE                       40244
#define EQ_AUTO                         40245
#define EQ_PRESETS                      40246
#define WINAMP_VIS_FALLOFF0             40247
#define WINAMP_VIS_FALLOFF1             40248
#define WINAMP_VIS_FALLOFF2             40249
#define WINAMP_VIS_FALLOFF3             40250
#define WINAMP_VIS_FALLOFF4             40251
#define WINAMP_VIS_PEAKS                40252
#define ID_LOAD_EQF                     40253
#define ID_SAVE_EQF                     40254
#define ID_PE_ENTRY                     40255
#define ID_PE_SCROLLUP                  40256
#define ID_PE_SCROLLDOWN                40257
#define WINAMP_MAIN_WINDOW              40258
#define WINAMP_VIS_PFALLOFF0            40259
#define WINAMP_VIS_PFALLOFF1            40260
#define WINAMP_VIS_PFALLOFF2            40261
#define WINAMP_VIS_PFALLOFF3            40262
#define WINAMP_VIS_PFALLOFF4            40263
#define ID_PE_TOP                       40264
#define ID_PE_BOTTOM                    40265
#define WINAMP_OPTIONS_WINDOWSHADE_PL   40266
#define EQ_INC1                         40267
#define EQ_INC2                         40268
#define EQ_INC3                         40269
#define EQ_INC4                         40270
#define EQ_INC5                         40271
#define EQ_INC6                         40272
#define EQ_INC7                         40273
#define EQ_INC8                         40274
#define EQ_INC9                         40275
#define EQ_INC10                        40276
#define EQ_INCPRE                       40277
#define EQ_DECPRE                       40278
#define EQ_DEC1                         40279
#define EQ_DEC2                         40280
#define EQ_DEC3                         40281
#define EQ_DEC4                         40282
#define EQ_DEC5                         40283
#define EQ_DEC6                         40284
#define EQ_DEC7                         40285
#define EQ_DEC8                         40286
#define EQ_DEC9                         40287
#define EQ_DEC10                        40288
#define ID_PE_SCUP                      40289
#define ID_PE_SCDOWN                    40290
#define WINAMP_REFRESHSKIN              40291
#define ID_PE_PRINT                     40292
#define ID_PE_EXTINFO                   40293
#define WINAMP_PLAYLIST_ADVANCE         40294
#define WINAMP_VIS_LIN                  40295
#define WINAMP_VIS_BAR                  40296
#define WINAMP_OPTIONS_MINIBROWSER      40298
#define MB_FWD                          40299
#define MB_BACK                         40300
#define MB_RELOAD                       40301
#define MB_OPENMENU                     40302
#define MB_OPENLOC                      40303
#define WINAMP_NEW_INSTANCE             40305
#define MB_UPDATE                       40309
#define WINAMP_OPTIONS_WINDOWSHADE_EQ   40310
#define EQ_PANLEFT                      40313
#define EQ_PANRIGHT                     40314
#define WINAMP_GETMORESKINS             40316
#define WINAMP_VIS_OPTIONS              40317
#define WINAMP_PE_SEARCH                40318
#define ID_PE_BOOKMARK                  40319
#define WINAMP_EDIT_BOOKMARKS           40320
#define WINAMP_MAKECURBOOKMARK          40321
#define ID_MAIN_PLAY_BOOKMARK_NONE      40322
#define ID_MAIN_PLAY_AUDIOCD            40323 // starts playing the audio CD in the first CD reader
#define ID_MAIN_PLAY_AUDIOCD2           40324 // plays the 2nd
#define ID_MAIN_PLAY_AUDIOCD3           40325 // plays the 3rd
#define ID_MAIN_PLAY_AUDIOCD4           40326 // plays the 4th
#define WINAMP_OPTIONS_VIDEO            40328
#define ID_VIDEOWND_ZOOMFULLSCREEN      40329
#define ID_VIDEOWND_ZOOM100             40330
#define ID_VIDEOWND_ZOOM200             40331
#define ID_VIDEOWND_ZOOM50              40332
#define ID_VIDEOWND_VIDEOOPTIONS        40333
#define WINAMP_MINIMIZE                 40334
#define ID_PE_FONTBIGGER                40335
#define ID_PE_FONTSMALLER               40336
#define WINAMP_VIDEO_TOGGLE_FS          40337
#define WINAMP_VIDEO_TVBUTTON           40338
#define WINAMP_LIGHTNING_CLICK          40339
#define ID_FILE_ADDTOLIBRARY            40344
#define ID_HELP_HELPTOPICS              40347
#define ID_HELP_GETTINGSTARTED          40348
#define ID_HELP_WINAMPFORUMS            40349
#define ID_PLAY_VOLUMEUP                40351
#define ID_PLAY_VOLUMEDOWN              40352
#define ID_PEFILE_OPENPLAYLISTFROMLIBRARY_NOPLAYLISTSINLIBRARY 40355
#define ID_PEFILE_ADDFROMLIBRARY        40356
#define ID_PEFILE_CLOSEPLAYLISTEDITOR   40357
#define ID_PEPLAYLIST_PLAYLISTPREFERENCES 40358
#define ID_MLFILE_NEWPLAYLIST           40359
#define ID_MLFILE_LOADPLAYLIST          40360
#define ID_MLFILE_SAVEPLAYLIST          40361
#define ID_MLFILE_ADDMEDIATOLIBRARY     40362
#define ID_MLFILE_CLOSEMEDIALIBRARY     40363
#define ID_MLVIEW_NOWPLAYING            40364
#define ID_MLVIEW_LOCALMEDIA_ALLMEDIA   40366
#define ID_MLVIEW_LOCALMEDIA_AUDIO      40367
#define ID_MLVIEW_LOCALMEDIA_VIDEO      40368
#define ID_MLVIEW_PLAYLISTS_NOPLAYLISTINLIBRARY 40369
#define ID_MLVIEW_INTERNETRADIO         40370
#define ID_MLVIEW_INTERNETTV            40371
#define ID_MLVIEW_LIBRARYPREFERENCES    40372
#define ID_MLVIEW_DEVICES_NOAVAILABLEDEVICE 40373
#define ID_MLFILE_IMPORTCURRENTPLAYLIST 40374
#define ID_MLVIEW_MEDIA                 40376
#define ID_MLVIEW_PLAYLISTS             40377
#define ID_MLVIEW_MEDIA_ALLMEDIA        40377
#define ID_MLVIEW_DEVICES               40378
#define ID_FILE_SHOWLIBRARY             40379
#define ID_FILE_CLOSELIBRARY            40380
#define ID_POST_PLAY_PLAYLIST           40381
#define ID_VIS_NEXT                     40382
#define ID_VIS_PREV                     40383
#define ID_VIS_RANDOM                   40384
#define ID_MANAGEPLAYLISTS              40385
#define ID_PREFS_SKIN_SWITCHTOSKIN      40386
#define ID_PREFS_SKIN_DELETESKIN        40387
#define ID_PREFS_SKIN_RENAMESKIN        40388
#define ID_VIS_FS                       40389
#define ID_VIS_CFG                      40390
#define ID_VIS_MENU                     40391
#define ID_VIS_SET_FS_FLAG              40392
#define ID_PE_SHOWPLAYING               40393
#define ID_HELP_REGISTERWINAMPPRO       40394
#define ID_PE_MANUAL_ADVANCE            40395
#define WA_SONG_5_STAR_RATING           40396
#define WA_SONG_4_STAR_RATING           40397
#define WA_SONG_3_STAR_RATING           40398
#define WA_SONG_2_STAR_RATING           40399
#define WA_SONG_1_STAR_RATING           40400
#define WA_SONG_NO_RATING               40401
#define PL_SONG_5_STAR_RATING           40402
#define PL_SONG_4_STAR_RATING           40403
#define PL_SONG_3_STAR_RATING           40404
#define PL_SONG_2_STAR_RATING           40405
#define PL_SONG_1_STAR_RATING           40406
#define PL_SONG_NO_RATING               40407
#define AUDIO_TRACK_ONE                 40408
#define VIDEO_TRACK_ONE                 40424

#define ID_SWITCH_COLOURTHEME			44500
#define ID_GENFF_LIMIT					45000

#endif
