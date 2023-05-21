================================================================

Copyright © 1996-2022, Valve Corporation, All rights reserved.

================================================================


Welcome to the Steamworks SDK.  For documentation please see our partner 
website at: http://partner.steamgames.com

----------------------------------------------------------------
v1.57 28th April 2022
----------------------------------------------------------------
User
* Updated ISteamUser::GetAuthTicketForWebApi(), To create a ticket for use by the AuthenticateUserTicket Web API
* Updated ISteamUser::GetAuthSessionTicket(), No longer to be used to create a ticket for use by the AuthenticateUserTicket Web API

----------------------------------------------------------------
v1.56 30th March 2023
----------------------------------------------------------------
User
* Updated ISteamUser::GetAuthSessionTicket(), Add parameter SteamNetworkingIdentity 

----------------------------------------------------------------
v1.55 29th July 2022
----------------------------------------------------------------

ISteamInput
* Added SetDualSenseTriggerEffect and corresponding header isteamdualsense.h for setting the adaptive trigger effect on DualSense controllers

Spacewar example:
* Added an example of using SetDualSenseTriggerEffect

----------------------------------------------------------------
v1.54 16th June 2022
----------------------------------------------------------------

ISteamFriends
* Added various functions to retrieve equipped Steam Community profile items and their properties
** RequestEquippedProfileItems – requests information on what Steam Community profile items a user has equipped.  Will send callback EquippedProfileItems_t.
** BHasEquippedProfileItem – after calling RequestEquippedProfileItems, returns true/false depending on whether a user has a ECommunityProfileItemType equipped or not
** GetProfileItemPropertyString – returns a string property given a ECommunityProfileItemType and ECommunityProfileItemProperty
** GetProfileItemPropertyUint – returns an unsigned integer property given a ECommunityProfileItemType and ECommunityProfileItemProperty
* Added callback EquippedProfileItemsChanged_t for when a user's equipped Steam Community profile items have changed.  This will be sent for the current user and for their friends.

Spacewar example:
* Added examples for how to interact with various overlay related functions (e.g. ActivateGameOverlay, ActivateGameOverlayToUser, ActivateGameOverlayToWebPage, ActivateGameOverlayToStore, ActivateGameOverlayInviteDialogConnectString)
* Fixed Steam Input example code not working on Linux

----------------------------------------------------------------
v1.53a 11th December 2021
----------------------------------------------------------------

macOS
* Fixed libsdkencryptedappticket.dylib to include arm64 support

----------------------------------------------------------------
v1.53 23th November 2021
----------------------------------------------------------------

SteamNetworkingSockets:
* Added support for connections to have multiple streams of messages, known as "lanes," with mechanisms to control bandwidth utilization and head-of-line blocking between lanes.
* Added the "FakeIP" system, which can be useful to add P2P networking or Steam Datagram Relay support to games while retaining the assumption that network hosts are identified by an IPv4 address.  Added steamnetworkingfakeip.h and ISteamNetworkingFakeUDPPort
* Simplified interface for iterating config values.
* Added SteamNetConnectionInfo_t::m_nFlags, which have misc info about a connection.

ISteamInput
* Added Steam Deck values to the EInputActionOrigin and ESteamInputType origins

ISteamUGC:
* Added SetTimeCreatedDateRange and SetTimeUpdatedDateRange

ISteamUtils:
* Added DismissFloatingGamepadTextInput

Flat Interface:
* For each interface accessor, there is now an inline, unversioned accessor that calls the versioned accessor exported by the .dll.  This reduces the number of changes that need to be made when updating the SDK and accessing the flat interface directly, while still retaining version safety.

General:
* Removed definitions for many internal callback IDs that are not needed by general users of the SDK.

Spacewar example:
* Added CItemStore, which demonstrates how to interact with an in-game store

----------------------------------------------------------------
v1.52 14th September 2021
----------------------------------------------------------------

ISteamInput
* Added support for bundling Steam Input API configurations w/ game depots. Allows developers to use the same configuration file across public/private AppIDs, check configurations into their revision control systems, more easily juggle changes between beta branches, and ensure game/config changes are done in-sync.
* Added new glyph API support for SVG glyphs and multiple sizes of PNG files. Note: these images will be added in a subsequent Steam Beta Client release.
* Added support for callbacks for action state changes, controller connect/disconnect, and controller mapping changes.
* Added BNewDataAvailable function to reduce need to manually compare action data between frames.
* Added BWaitForData helper function to wait on an event set when controller data is updated.
* Added functions for getting the localized string for action names (GetStringForDigitalActionName and GetStringForAnalogActionName).
* Added function to poll current Steam Input enable settings by controller type (GetSessionInputConfigurationSettings).

ISteamGameServer
* Renamed EnableHeartbeats to SetAdvertiseServerActive.
* Deprecated the following methods (they have been renamed to *_DEPRECATED and will be removed in a future SDK update):
** SendUserConnectAndAuthenticate
** SendUserDisconnect
** SetMasterServerHeartbeatInterval
** ForceMasterServerHeartbeat

ISteamRemoteStorage
* Added GetLocalFileChangeCount and GetLocalFileChange which allows for iterating over Steam Cloud files that have changed locally after the initial sync on app start, when supported by the app.  The callback notification is RemoteStorageLocalFileChange_t.
* Added BeginFileWriteBatch and EndFileWriteBatch to hint to Steam that a set of files should be written to Steam Cloud together (e.g. a game save that requires updating more than one file).
* Removed the following unused callbacks: RemoteStorageAppSyncedClient_t, RemoteStorageAppSyncedServer_t, RemoteStorageAppSyncProgress_t, and RemoteStorageAppSyncStatusCheck_t.

ISteamUGC
* Added ability to sort by "time last updated" (k_EUGCQuery_RankedByLastUpdatedDate).
* Added ShowWorkshopEULA and GetWorkshopEULAStatus, which allows a game to have a separate EULA for the Steam Workshop.
* Added UserSubscribedItemsListChanged_t callback.
* Added WorkshopEULAStatus_t callback, which will be sent asynchronously after calling GetWorkshopEULAStatus.

ISteamUser
* Deprecated InitiateGameConnection and TerminateGameConnection (renamed to *_DEPRECATED).  Please migrate to BeginAuthSession and EndAuthSession.

ISteamUtils
* Added IsSteamRunningOnSteamDeck - Can be used to optimize the experience of the game on Steam Deck, such as scaling the UI appropriately, applying performance related settings, etc.
* Added SetGameLauncherMode - In game launchers that don't have controller support you can call this to have Steam Input translate the controller input into mouse/kb to navigate the launcher.
* Added AppResumingFromSuspend_t callback - Sent after the device returns from sleep/suspend mode.
* Added ShowFloatingGamepadTextInput - Activates the modal gamepad input keyboard which pops up over game content and sends OS keyboard keys directly to the game. Note: Currently this is only implemented in the Steam Deck UI.
* Added FloatingGamepadTextInputDismissed_t callback - Sent after the floating gamepad input keyboard displayed via ShowFloatingGamepadTextInput has been dismissed.

macOS
* Added i386/x86_64/arm64 universal builds of libsdkencryptedappticket.dylib and libsteam_api.dylib

Steamworks Example Project
* Updated project to illustrate new Steam Input changes
* Updated to build properly with macOS 11 SDK for arm64
* Updated Windows project files to Visual Studio 2015
* Windows project files now target Windows 8.1
* Windows project files now set include and library path using DXSDK_DIR

Misc.
* ISteamAppList - Added m_iInstallFolderIndex to SteamAppInstalled_t and SteamAppUninstalled_t callbacks.
* ISteamApps - Removed unused SteamGameServerApps() accessor.
* CSteamGameServerAPIContext - removed SteamApps() accessor.
* Cleanup of types and enums that were unnecessarily in the SDK.


----------------------------------------------------------------
v1.51 8th January 2021
----------------------------------------------------------------
ISteamUGC
* Added GetQueryUGCNumTags(), GetQueryUGCTag(), and GetQueryUGCTagDisplayName() for access to an item's tags and the display names (e.g. localized versions) of those tags
* A previous SDK update added (but failed to call out) AddRequiredTagGroup() which allows for matching at least one tag from the group (logical "or")

ISteamInput & ISteamController
* Added PS5 Action Origins

ISteamFriends
* Added ActivateGameOverlayInviteDialogConnectString - Activates the game overlay to open an invite dialog that will send the provided Rich Presence connect string to selected friends

Steamworks Example
* Updated to use latest SteamNetworkingSockets API

Content Builder
* Updated upload example to use a single script file to upload a simple depot

----------------------------------------------------------------
v1.50 29th August 2020
----------------------------------------------------------------
* Added ISteamUtils::InitFilterText() and ISteamUtils::FilterText() which allow a game to filter content and user-generated text to comply with China legal requirements, and reduce profanity and slurs based on user settings.
* Added ISteamNetworkingMessages, a new non-connection-oriented API, similar to UDP.  This interface is intended to make it easy to port existing UDP code while taking advantage of the features provided by ISteamNetworkingSockets, especially Steam Datagram Relay (SDR).
* Added poll groups to ISteamNetworkingSockets.  Poll groups are a way to receive messages from many different connections at a time.
* ISteamNetworkingSockets::ReceiveMessagesOnListenSocket has been removed.  (Use poll groups instead.)
* Added symmetric connect mode to ISteamNetworkingSockets.  This can be used to solve the coordination problem of establishing a single connection between two peers, when both peers may initiating the connection at the same time and neither peer is the “server” or “client”.
* ISteamNetworking is deprecated and may be removed in a future version of the SDK.  Please use ISteamNetworkingSockets or ISteamNetworkingMessages instead.


----------------------------------------------------------------
v1.49 12th June 2020
----------------------------------------------------------------
* Added ISteamApps::BIsTimedTrial() which allows a game to check if user only has limited playtime
* Added ISteamFriends::RegisterProtocolInOverlayBrowser() which will enable dispatching callbacks when the overlay web browser navigates to a registered custom protocol, such as “mygame://<callback data>”
* Added ISteamuserStats::GetAchievementProgressLimits() which lets the game query at run-time the progress-based achievement’s bounds as set by the developers in the Steamworks application settings
* Added tool to demonstrate processing the steam.signatures file that comes in the steam client package.


----------------------------------------------------------------
v1.48a 26th March 2020
----------------------------------------------------------------

macOS
* Fixed notarization issues caused by missing code signature of libsdkencryptedappticket.dylib


----------------------------------------------------------------
v1.48 12th February 2020
----------------------------------------------------------------

ISteamNetworkingSockets
* Added the concept of a "poll group", which is a way to receive messages from many connections at once, efficiently.
* ReceiveMessagesOnListenSocket was deleted.  To get the same functionality, create a poll group, and then add connections to this poll group when accepting the connection.

Flat interface redesign
* Fixed many missing interfaces and types.
* All versions of overloaded functions are now available, using distinct names.
* There are now simple, global versioned functions to fetch the interfaces.  No more need to mess with HSteamPipes or HSteamUsers directly.
* The json file now has much more detailed information and several errors have been fixed.
* steam_api_interop.cs has been removed and will no longer be supported.
* There is a new manual dispatch API for callbacks, which works similarly to a windows event loop.  This is a replacement for the existing callback registeration and dispatch mechanisms, which which are nice in C++ but awkward to use outside of C++.


----------------------------------------------------------------
v1.47 3rd December 2019
----------------------------------------------------------------

macOS
* Updated steamcmd binaries to be 64-bit

ISteamNetworkingSockets
* Added API to set configuration options atomically, at time of creation of the listen socket or connection
* Added API to send multiple messages efficiently, without copying the message payload
* Added API for relayed P2P connections where signaling/rendezvous goes through your own custom backend instead of the Steam servers

ISteamRemotePlay
* Added a function to invite friends to play via Remote Play Together


----------------------------------------------------------------
v1.46 26th July 2019
----------------------------------------------------------------

ISteamRemotePlay
* Added a new interface to get information about Steam Remote Play sessions

ISteamInput
* Added the GetRemotePlaySessionID function to find out whether a controller is associated with a Steam Remote Play session


----------------------------------------------------------------
v1.45 25th June 2019
----------------------------------------------------------------

Steam Input and Steam Controller Interfaces
* Added the GetDeviceBindingRevision function which allows developers of Steam Input API games to detect out of date user configurations. Configurations w/ out of date major revisions should be automatically updated by Steam to the latest official configuration, but configurations w/ out of date minor revisions will be left in-place.

ISteamUser
* Add duration control APIs to support anti-indulgence regulations in some territories. This includes callbacks when gameplay time thresholds have been passed, and an API to fetch the same data on the fly.

ISteamUtils
* Add basic text filtering API.

----------------------------------------------------------------
v1.44 13th March 2019
----------------------------------------------------------------

ISteamNetworkingSockets
* Socket-style API that relays traffic on the Valve network

ISteamNetworkingUtils
* Tools for instantly estimating ping time between two network hosts

----------------------------------------------------------------
v1.43 20th February 2019
----------------------------------------------------------------

ISteamParties 
* This API can be used to selectively advertise your multiplayer game session in a Steam chat room group. Tell Steam the number of player spots that are available for your party, and a join-game string, and it will show a beacon in the selected group and allow that many users to “follow” the beacon to your party. Adjust the number of open slots if other players join through alternate matchmaking methods.  

ISteamController
* This interface will be deprecated and replaced with ISteamInput. For ease in upgrading the SDK ISteamController currently has feature parity with ISteamInput, but future features may not be ported back. Please use ISteamInput for new projects.
* Added GetActionOriginFromXboxOrigin, GetStringForXboxOrigin and GetGlyphForXboxOrigin to allow Xinput games to easily query glyphs for devices coming in through Steam Input’s Xinput emulation, ex: “A button”->”Cross button” on a PS4 controller. This is a simple translation of the button and does not take user remapping into account – the full action based API is required for that.
* Added TranslateActionOrigin which allows Steam Input API games to which are using look up tables to translate action origins from an recognized device released after the game was last built into origins they recognize.
* Added count and max_possible fields to current enums to make using lookup tables easier

ISteamInput
* This new interface replaces ISteamController to better reflect the fact this API supports not just the Steam Controller but every controller connected to Steam – including Xbox Controllers, Playstation Controllers and Nintendo Switch controllers. ISteamController currently has feature parity with the new features added in ISteamInput but new feature may not be ported back. Please use this interface instead of ISteamController for any new projects.
* Migrating to ISteamInput from ISteamController should mostly be a search-replace operation but any action origin look up tables will need to be adjusted as some of the enum orders have changed.
* Added GetActionOriginFromXboxOrigin, GetStringForXboxOrigin and GetGlyphForXboxOrigin to allow Xinput games to easily query glyphs for devices coming in through Steam Input’s Xinput emulation, ex: “A button”->”Cross button” on a PS4 controller. This is a simple translation of the button and does not take user remapping into account – the full action based API is required for that.
* Added TranslateActionOrigin which allows Steam Input API games to which are using look up tables to translate action origins from an recognized device released after the game was last built into origins they recognize.
* Added count and max_possible fields to current enums to make using lookup tables easier

ISteamFriends
* ActivateGameOverlayToWebPage – Added a new parameter to control how the created web browser window is displayed within the Steam Overlay. The default mode will create a new browser tab next to all other overlay windows that the user already has open. The new modal mode will create a new browser window and activate the Steam Overlay, showing only that window. When the browser window is closed, the Steam Overlay is automatically closed as well.

ISteamInventory
* GetItemsWithPrices and GetItemPrice - Added the ability to get the “base price” for a set of items, which you can use to markup in your own UI that items are “on sale” 

ISteamUGC
* SetAllowLegacyUpload - Call to force the use of Steam Cloud for back-end storage (instead of Steam Pipe), which is faster and more efficient for uploading and downloading small files (less than 100MB).
* CreateQueryAllUGCRequest - Added ability to page through query results using a “cursor” instead of a page number.  This is more efficient and supports “deep paging” beyond page 1000.  The old version of CreateQueryAllUGCRequest() that takes a page parameter is deprecated and cannot query beyond page 1000.  Note that you will need to keep track of the “previous” cursor in order to go to a previous page.

ISteamApps
* GetLaunchCommandLine - Get command line if game was launched via Steam URL, e.g. steam://run/<appid>//<command line>/. If you get NewUrlLaunchParameters_t callback while running, call again to get new command line
* BIsSubscribedFromFamilySharing - Check if subscribed app is temporarily borrowed via Steam Family Sharing

Steam API
* Refactored headers to minimize the number of headers that need to be included to use a single ISteam interface.
* Renamed some macros with STEAM_ prefix to minimize conflicts in the global namespace



----------------------------------------------------------------
v1.42 3rd January 2018
----------------------------------------------------------------

ISteamInventory
* Added ability to start a purchase process through the Steam Client via the StartPurchase call and a given set of item definition ids and quantities.  Users will be prompted in the Steam Client overlay to complete the purchase, including funding their Steam Wallet if necessary.  Returns a SteamInventoryStartPurchaseResult_t call result if the user authorizes the purchase.
* Added ability to retrieve item definition prices via the RequestPrices call. Once the call result SteamInventoryRequestPricesResult_t is returned, GetNumItemsWithPrices, GetItemsWithPrices, and GetItemPrice can be called to retrieve the item definition prices in the user's local currency.
* Added ability to modify whitelisted per item dynamic properties. The usage pattern is to call StartUpdateProperties, SetProperty or RemoveProperty, and finally SubmitUpdateProperties.  The SteamInventoryCallback_t will be fired with the appropriate result handle on success or failure.
* Deprecated TradeItems

ISteamController
* Added Action Set Layers – Action Set Layers are optional sets of action bindings which can be overlaid upon an existing set of controls.  In contrast to Action Sets, layers draw their actions from the Action Set they exist within and do not wholesale replace what is already active when applied, but apply small modifications.  These can consist of setting changes as well as adding or removing bindings from the base action set.  More than one layer can be applied at a time and will be applied consecutively, so an example might be the Sniper Class layer which includes tweaks or bindings specific to snipers in addition to the Scoped-In layer which alters look sensitivity.
* Added ActivateActionSetLayer – Activates the specified Layer.
* Added DeactivateActionSetLayer – Deactivates the specified Layer.
* Added DeactivateAllActionSetLayers – Deactivates all layers, resetting the mapping to the action base Action Set.
* Added GetActiveActionSetLayers – Returns all currently active Action Set Layers.
* Added GetInputTypeForHandle - Returns the input type for a particular handle, such as Steam Controller, PS4 Controller, Xbox One or 360.

ISteamHTMLSurface
* Added HTML_BrowserRestarted_t callback which is fired when the browser has restarted due to an internal failure

ISteamFriends
* Added IsClanPublic
* Added IsClanOfficialGameGroup

Steam API
* Removed the ISteamUnifiedMessages interface.  It is no longer intended for public usage.


----------------------------------------------------------------
v1.41 13th July 2017
----------------------------------------------------------------

ISteamClient
* Exposed ISteamParentalSettings interface. You can use this to determine if the user has parental settings turned on and for what high-level Steam features. 

* ISteamHTMLSurface
* Added SetDPIScalingFactor - Scale the output display space by this factor, this is useful when displaying content on high dpi devices.

ISteamUGC
* Added ability to mark a piece of UGC as requiring a set of DLC (AppID). These relationships are managed via new AddAppDependency, RemoveAppDependency, and GetAppDependencies calls.
* Ported over ability to delete UGC from ISteamRemoteStorage and called it DeleteItem. Note that this does *not* prompt the user in any way.
* Added m_nPublishedFileId to SubmitItemUpdateResult_t so that it is easier to keep track of what item was updated.


----------------------------------------------------------------
v1.40 25th April 2017
----------------------------------------------------------------

ISteamInventory
* Update API documentation 
* GetResultItemProperty - Retrieve dynamic properties for a given item returned in the result set.

ISteamUtils
* IsVRHeadsetStreamingEnabled - Returns true if the HMD content will be streamed via Steam In-Home Streaming
* SetVRHeadsetStreamingEnabled - Set whether the HMD content will be streamed via Steam In-Home Streaming

ISteamUser
* GetAvailableVoice and GetVoice - Some parameters have become deprecated and now have default values.

ISteamUGC 
* SetReturnPlaytimeStats - Set the number of days of playtime stats to return for a piece of UGC.
* AddDependency and RemoveDependency - Useful for parent-child relationship or dependency management

ISteamVideo
* Added GetOPFSettings and GetOPFStringForApp for retrieving Open Projection Format data used in Steam 360 Video playback. 
* GetOPFSettings - Handle the GetOPFSettingsResult_t callback which is called when the OPF related data for the passed in AppID is ready for retrieval. 
* GetOPFStringForApp - Using the AppID returned in GetOPFSettingsResult_t pass in an allocated string buffer to get the OPF data.

SteamPipe GUI Tool
* A simple GUI wrapper for Steamcmd/SteamPipe has been added to the SDK in the tools\ContentBuilder folder. More details can be found here: http://steamcommunity.com/groups/steamworks/discussions/0/412449508292646864


----------------------------------------------------------------
v1.39 6th January 2017
----------------------------------------------------------------

ISteamController

The two new Origin helper functions in this interface allow you to query a description and a glyph for types of controllers and inputs that are in the current SDK header, but also any type of controller that might be supported by the Steam client in the future. To achieve this, pass origin values directly returned from Get*ActionOrigin() functions into GetStringForActionOrigin() and GetGlyphForActionOrigin() and display the results programmatically without checking against the range of the Origin enumerations.

* TriggerVibration - Trigger a vibration event on supported controllers
* SetLEDColor - Set the controller LED color on supported controllers
* GetStringForActionOrigin - Returns a localized string (from Steam's language setting) for the specified origin
* GetGlyphForActionOrigin - Get a local path to art for on-screen glyph for a particular origin
* Updated Spacewar example to include example usage

ISteamFriends
* Removed k_EFriendFlagSuggested, since it was unused

ISteamInventory
* Updated and corrected documentation in the API
* RequestEligiblePromoItemDefinitionsIDs - Request the list of "eligible" promo items that can be manually granted to the given user.  These are promo items of type "manual" that won't be granted automatically. An example usage of this is an item that becomes available every week.
* GetEligiblePromoItemDefinitionIDs - After handling a SteamInventoryEligiblePromoItemDefIDs_t call result, use this function to pull out the list of item definition ids that the user can be manually granted via the AddPromoItems() call.


----------------------------------------------------------------
v1.38 14th October 2016
----------------------------------------------------------------

ISteamUGC
* Added ability to track the playtime of Workshop items. Call StartPlaytimeTracking() and StopPlaytimeTracking() when appropriate. On application shutdown all playtime tracking will stop, but StopPlaytimeTrackingForAllItems() can also be used.
* Added ability to query Workshop items by total playtime in a given period, total lifetime playtime, average playtime in a given period, lifetime average playtime, number of play sessions in a given period, and number of lifetime play sessions.
* Added ability to retrieve item statistics for number of seconds played, number of play sessions, and number of comments.
* Added SetReturnOnlyIDs() for queries.  This is useful for retrieving the list of items a user has subscribed to or favorited without having to get all the details for those items.
* Modified GetQueryUGCStatistic() to take in a uint64 instead of a uint32 to support larger values

ISteamUser
* Added BIsPhoneIdentifying()
* Added BIsPhoneRequiringVerification()

ISteamScreenshots
* Added IsScreenshotsHooked() if the application has hooked the screenshot
* Added ability to add a VR screenshot that was saved to disk to the user's library

ISteamRemoteStorage
* Modified GetQuota() to take in uint64 from int32, since Steam Cloud can now support quotas above 2GB
* Removed RemoteStorageConflictResolution_t callback

ISteamApps
* Added GetFileDetails() which will return FileDetailsResult_t through a call result. The FileDetailsResult_t struct contains information on the original file's size, SHA1, etc.

ISteamFriends
* Deprecated k_EFriendRelationshipSuggested relationship type that was originally used by Facebook linking feature

----------------------------------------------------------------
v1.37 23rd May 2016
----------------------------------------------------------------

Starting with this release, SDK forward-compatibility has been improved. All executables and libraries built using the official C++ headers from this SDK will continue to work even when paired with runtime DLLs from future SDKs. This will eventually allow for the mixing of dynamic libraries (such as third-party plug-ins) built with different versions of Steamworks.

The VERSION_SAFE_STEAM_API_INTERFACES compile-time flag is no longer necessary for cross-version compatibility, and the SteamAPI_InitSafe and SteamGameServer_InitSafe functions have been removed. Applications which currently use these InitSafe functions should be changed to use the normal Init functions instead.


ISteamRemoteStorage
* Removed unsed UGCHandle_t m_hFile from RemoteStoragePublishedFileUpdated_t

ISteamUGC
* Added ability to add additional preview types to UGC such as standard images, YouTube videos, Sketchfab models, etc.

ISteamUser
* Added BIsPhoneVerified()
* Added BIsTwoFactorEnabled()

ISteamUtils
* Added IsSteamInBigPictureMode()
* Added StartVRDashboard(), which asks Steam to create and render the OpenVR Dashboard

ISteamApps
* Added RequestAllProofOfPurchaseKeys


----------------------------------------------------------------
v1.36 9th February 2016
----------------------------------------------------------------

ISteamController:
* added new function TriggerRepeatedHapticPulse()


Revision History:

----------------------------------------------------------------
v1.35 21st September 2015
----------------------------------------------------------------

ISteamController:
 * The controller API has been redesigned to work with production Steam Controllers and take advantage of the configuration panel inside of Steam. The documentation on the partner site has a full overview of the new API.

ISteamRemoteStorage:
 * Added asynchronous file read and write methods. These methods will not block your calling thread for the duration of the disk IO. Additionally, the IO is performed in a worker thread in the Steam engine, so they will not impact other Steam API calls.
  - FileWriteAsync: Similar in use to FileWrite, however it returns a SteamAPICall_t handle. Use the RemoteStorageFileWriteAsyncComplete_t structure with your asynchronous Steam API handler, and that will indicate the results of the write. The data buffer passed in to FileWriteAsync is immediately copied, so you do not have to ensure it is valid throughout the entire asynchronous process.
  - FileReadAsync: This function queues an asynchronous read on the file specified, and also returns a SteamAPICall_t handle. The completion event uses the new RemoteStorageFileReadAsyncComplete_t structure. Upon successful completion, you can use the new FileReadAsyncComplete function to read the data -- passing in the original call handle, a pointer to a buffer for the data, and the amount to read (which generally should be equal to the amount read as specified by the callback structure, which generally will be equal to the amount requested). Additionally, the FileReadAsync function lets you specify an offset to read at, so it is no longer necessary to read the entire file in one call.


----------------------------------------------------------------
v1.34 28th July 2015
----------------------------------------------------------------
ISteamUGC:

* Added ability to set and retrieve key-value tags on an item. There can be multiple values for each key.
* Added ability to query all UGC that have matching key-value tags.
* Added ability to specify a title and description on an item for a specific language (defaults to English).
* Added ability to query for items and return the title and description in a preferred language.
* Added ability to vote on an item and retrieve the current user's vote on a given item (duplicated from ISteamRemoteStorage).


----------------------------------------------------------------
v1.33 6th May 2015
----------------------------------------------------------------

UGC:
* Added DownloadItem(), which will force download a piece of UGC (it will be cached based on usage). This can be used by stand-alone game servers.
* Renamed GetItemUpdateInfo() => GetItemDownloadInfo() and added GetItemState() which can be used to determine whether an item is currently being downloaded, has already been downloaded, etc.
* Added ability to set and retrieve developer metadata for an item
* Added ability to modify a user's favorites list
* Added ability to retrieve preview image & video URLs
* Added ability to retrieve "children" for an item (e.g. for collections)
* Added ability to retrieve stats, such as current number of subscribers, lifetime unique subscribers, etc.

SteamVR
* steamvr.h has been removed. You can use the OpenVR SDK to access those interfaces: https://github.com/ValveSoftware/openvr

SteamVideo
* Added ability to check if a user is currently broadcasting


----------------------------------------------------------------
v1.32 5th February 2015
----------------------------------------------------------------

General:
* Added an auto-generated "flat" C-style API for common Steamworks features (steam_api_flat.h)
* Added an auto-generated C# binding for common Steamworks features (steam_api_interop.cs)
* Expanded the ISteamFriends interface to include steam levels and friends groups
* Expanded the ISteamHTTP interface to include cookie handling, SSL certificate verification, and network timeouts
* Fixed typos in ISteamHTMLSurface interface constants

Inventory:
* Added the initial version of ISteamInventory, a developer-preview release of our new Steam Inventory Service for managing and tracking a Steam-compatible inventory of in-game items. Please see the documentation for the Inventory Service on the partner website for more details.



----------------------------------------------------------------
v1.31 8th September 2014
----------------------------------------------------------------

UGC:
* The Workshop item content API in ISteamUGC now supports legacy workshop items uploaded via the ISteamRemoteStorage interface. ISteamUGC::GetItemInstallInfo(). This will return whether the item was a legacy item or a new item. If it is a legacy item, then the pchFolder variable will be the full path to the file.

HTML:
* Added initial version of ISteamHTMLSurface API, which allows games to get textures for html pages and interact with them. There is also a sample implementation in the SteamworksExample.

Virtual Reality:
* Added VR_IsHmdPresent, which returns true if an HMD appears to be present but does not initialize the VR API. This is useful when enabling/disabling UI elements to offer VR mode to a user.
* Added VR_GetStringForHmdError which turns an HmdError enum value into a string.

SteamPipe
* The example Steampipe batch file (run_build.bat) now uses run_app_build_http instead of run_app_build by default.

ContentPrep.app
* Updated wxPython requirements for this app (version 2.7 and 2.8 supported). App will prompt with updated URL to download compatible packages if necessary.



----------------------------------------------------------------
v1.30 10th July 2014
----------------------------------------------------------------

General:
* Added a new Workshop item content API in ISteamUGC that is easy to use and allows multiple files per item without any size limits. It uses the same 
  content system that handles regular content depots, resulting in faster and smaller downloads due to delta patching. Subscribed workshop items will
  be placed in unique subfolders in the install folder, so the game doesn't need to fetch them using ISteamRemoteStorage anymore. The new API is not
  backwards compatible with old items created with ISteamRemoteStorage. Added Workshop feature to steamworksexample using ISteamUGC.


Steam VR:
* VR_Init now requires that you call SteamAPI_Init first.


----------------------------------------------------------------
v1.29 24th April 2014
----------------------------------------------------------------

General:
* Adjust game server login to use a token instead of username/password. Tokens are randomly generated at account creation time and can be reset.
* Added existing text param to ISteamUtils::ShowGamepadTextInput() so games can prepopulate control before displaying to user.
* Updated retail disc installer to use a single multi-language steamsetup.exe replacing all Steam install MSI packages.
* Removed redistributable Steam libraries for dedicated servers. Standalone dedicated server should use shared "Steamworks SDK Redist" depots.
* steamcmd is now included for Linux and OSX.

Music:
* Introducing API to control the Steam Music Player from external software. As an example this gives games the opportunity to pause the music or lower the volume, when an important cut scene is shown, and start playing afterwards.
* Added menu and code to the Steamworks Example to demonstrate this API.
* This feature is currently limited to users in the Steam Music Player Beta. It will have no effect on other users.

UGC:
* ISteamUGC - Add m_bCachedData to SteamUGCQueryCompleted_t and SteamUGCRequestUGCDetailsResult_t which can be used to determine if the data was retrieved from the cache.
* ISteamUGC - Allow clients to get cached responses for ISteamUGC queries. This is so client code doesn't have to build their own caching layer on top of ISteamUGC.
* ISteamRemoteStorage - add the name of the shared file to RemoteStorageFileShareResult_t so it can be matched up to the request if a game has multiple outstanding FileShare requests going on at the same time

Steam VR:
* Renamed GetEyeMatrix to GetHeadFromEyePose and made it return an HmdMatrix34t. This doesn't actually change the values it was returning, it just updates the name to match the values that were already being returned. Changed the driver interface too.
* Renamed GetWorldFromHeadPose to GetTrackerFromHeadPose to avoid confusion about the game's world space vs. the tracker's coordinate system.
* Also renamed GetLastWorldFromHeadPose to GetLastTrackerFromHeadPose.
* Added GetTrackerZeroPose method to get the tracker zero pose.
* Added VR support to the Linux/SDL version of the Steamworks Example.

----------------------------------------------------------------
v1.28 28th January 2014
----------------------------------------------------------------

* Added Steamworks Virtual Reality API via steamvr.h.
* Added ISteamUtils::IsSteamRunningInVRMode, which returns true if the Steam Client is running in VR mode.
* Deprecated ISteamGameserver::GetGameplayStats and ISteamGameserver::GetServerReputation. These calls already return no data and will be removed in a future SDK update.
* Added result code k_EResultRateLimitExceeded, which can now be returned if a user has too many outstanding friend requests.

----------------------------------------------------------------
v1.26a 14th November 2013
----------------------------------------------------------------

* Fix missing accessor function in steam_api.h for SteamUGC()

----------------------------------------------------------------
v1.26 6th November 2013
----------------------------------------------------------------
* Includes libsteam_api.so for 64-bit Linux.
* Callbacks ValidateAuthTicketResponse_t and GSClientApprove_t now contain the SteamID of the owner of current game. If the game is borrowed, this is different than the player's SteamID.
* Added ISteamFriends::GetPlayerNickname, which returns the nickname the current user has set for the specified player.
* Fix p2p networking apis on Linux so they work with dedicated servers
* Fix command line argument handling bug in SteamAPI_RestartAppIfNecessary on Linux and OSX.
* Added ISteamApps::GetLaunchQueryParam, which will get the value associated with the given key if a game is launched via a url with query paramaters, such as steam://run/<appid>//?param1=value1;param2=value2;param3=value3.  If the game is already running when such a url is executed, instead it will receive a NewLaunchQueryParameters_t callback.
* Added EUGCReadAction parameter to ISteamRemoteStorage:UGCRead that allows the game to keep the file open if it needs to seek around the file for arbitrary data, rather than always closing the file when the last byte is read.
* Added new ISteamUGC interface that is used for querying for lists of UGC details (e.g. Workshop items, screenshots, videos, artwork, guides, etc.). The ISteamUGC interface should be used instead of ISteamRemoteStorage, which contains similar, but less flexible and powerful functionality.
* Removed tools for deprecated content system


----------------------------------------------------------------
v1.25 1st October 2013
----------------------------------------------------------------
* Fixed a crash in the 1.24 SDK update when attempting to call ISteamRemoteStorage::GetPublishedFileDetails by adding a missing parameter unMaxSecondsOld, which allows a game to request potentially-cached details (passing a value of 0 retains the previous behavior).

----------------------------------------------------------------
v1.24	17th July 2013
----------------------------------------------------------------

User:
* Added ISteamUser::GetBadgeLevel and ISteamUser::GetPlayerSteamLevel functions

Friends:
* Games can now initiate Steam Friend requests, removals, request -accepts and request-ignores via ISteamFriends’ ActivateGameOverlayToUser API. This prompts the user for confirmation before action is taken.

Mac:
* Updated the OS X Content Prep tool and game wrapper for improved compatibility with OS X 10.8 (Mountain Lion).

Linux:
* Added install script for the Steam Linux Runtime tools (run "bash tools/linux/setup.sh" to install), see tools/linux/README.txt for details.
* SteamworksExample is now available on Linux

----------------------------------------------------------------
v1.23a	25th February 2013
----------------------------------------------------------------

Windows:
* Fix passing command-line parameters across SteamApi_RestartAppIfNeccessary()

----------------------------------------------------------------
v1.23	19th February 2013
----------------------------------------------------------------

Cloud:
* Added ISteamScreenshots::TagPublishedFile() which allows tagging workshop content that is visible or active when a screenshot is taken.
* Added ISteamRemoteStorage::UGCDownloadToLocation() which allows a developer to specify a location on disk to download workshop content.

Setup tool:
* Added Arabic to the supported languages for the PC Gold Master Setup Tool
* Fixed regression in localized EULA support in Mac OS X Gold Master Setup Tool

Windows:
* Fix SteamAPI_RestartAppIfNecessary() on 64 bit Windows
* When launching a game's development build from outside of Steam, fixed using steam_appid.txt in the Steam Overlay and for authorizing microtransactions (broken in the SDK 1.22)

Mac:
* Fixed many Steam callbacks not working for 64 bit OS X games due to mismatched structure alignment between the SDK and the Steam client
* Implemented SteamAPI_RestartAppIfNecessary()

Linux:
* Removed the need to redistribute libtier0_s.so and libvstdlib_s.so
* Fixed finding and loading steamclient.so, so LD_LIBRARY_PATH does not need to be set for game to talk with Steam
* Implemented SteamAPI_RestartAppIfNecessary()


----------------------------------------------------------------
v1.22	12th December 2012
----------------------------------------------------------------

Apps
* Added new API call ISteamApps::MarkContentCorrupt() so a game can hint Steam that some of it's local content seems corrupt. Steam will verify the content next time the game is started.
* Added new API call ISteamApps::GetCurrentBetaName() so a game can get the current content beta branch name if the user chose to opt-in to a content beta.

Cloud 
* Added an offset parameter to ISteamRemoteStorage::UGCRead() to allow reading files in chunks, and increased the limit from 100MB to 200MB when downloading files this way.

HTTP
* Added support for streaming HTTP requests with ISteamHTTP::SendHTTPRequestAndStreamResponse() and ISteamHTTP::GetHTTPStreamingResponseBodyData()

Linux
* Updated libsteam_api.so to find Steam in its new install location


----------------------------------------------------------------
v1.21	25th October 2012
----------------------------------------------------------------

Big Picture
* Added ISteamUtils::ShowGamepadTextInput() to enable usage of the Big Picture gamepad text input control in-game. UI is rendered by the Steam Overlay.
* Added ISteamUtils::GetEnteredGamepadTextLength() and ISteamUtils::GetEnteredGamepadTextInput() to retrieve entered gamepad text.
* Added GamepadTextInputDismissed_t callback to detect when the user has entered gamepad data.


----------------------------------------------------------------
v1.20	30th August 2012
----------------------------------------------------------------

SteamPipe
* Added local server and builder tools for new content system.

Mac
* OSX Supports 64 bit build targets.
* Spacewar has been updated to be buildable as a 64 bit OSX sample application.

Friends
* Added a callback for the result of ISteamFriends::SetPersonaName().
* Changed ISteamFriends::ActivateGameOverlayToStore() to take an additional parameter so app can be directly added to the cart.

Cloud
* Added ISteamRemoteStorage::FileWriteStreamOpen(), FileWriteStreamWriteChunk(), FileWriteStreamClose() and FileWriteStreamCancel() for streaming operations.
* Changed parameters to ISteamRemoteStorage::PublisheVideo().
* Added file type to ISteamRemoteStorage::GetPublishedFileDetails() callback result (RemoteStorageGetPublishedFileDetailsResult_t).
* Added a callback to indicate that a published file that a user owns was deleted (RemoteStoragePublishedFileDeleted_t).

ISteamUserStats
* Added ISteamUserStats::GetNumAchievements() and ISteamUserStats::GetAchievementName().


----------------------------------------------------------------
v1.19	22nd March 2012
----------------------------------------------------------------

Friends
* Added ISteamFriends::GetFollowerCount()
* Added ISteamFriends::IsFollowing()
* Added ISteamFriends::EnumerateFollowingList()

Cloud
* Added ISteamRemoteStorage::UpdatePublishedFileSetChangeDescription()
* Added ISteamRemoteStorage::GetPublishedItemVoteDetails()
* Added ISteamRemoteStorage::UpdateUserPublishedItemVote()
* Added ISteamRemoteStorage::GetUserPublishedItemVoteDetails()
* Added ISteamRemoteStorage::EnumerateUserSharedWorkshopFiles()
* Added ISteamRemoteStorage::PublishVideo()
* Added ISteamRemoteStorage::SetUserPublishedFileAction()
* Added ISteamRemoteStorage::EnumeratePublishedFilesByUserAction()
* Added ISteamRemoteStorage::EnumeratePublishedWorkshopFiles()

ISteamGameServer
* Updated callback for SteamGameServer::ComputeNewPlayerCompatibility to include the steam id the compatibility was calculated for


----------------------------------------------------------------
v1.18	7th February 2012
----------------------------------------------------------------

Cloud
* Removed ISteamRemoteStorage::PublishFile() and consolidated the API to PublishWorkshopFile()
* Updated ISteamRemoteStorage::PublishWorkshopFile() to better define the type of workshop file being published
* Replaced ISteamRemoteStorage::UpdatePublishedFile() with a new mechanism to update existing files through CreatePublishedFileUpdateRequest() UpdatePublishedFile[Property](), and CommitPublishedFileUpdate()
* Increased the description field for a workshop file from 256 -> 8000 characters
* Added ISteamRemoteStorage::GetUGCDownloadProgress()
* Added file size limit of 100MB to ISteamRemoteStorage::FileWrite()

Apps:
* Added ISteamApps::RequestAppProofOfPurchaseKey

----------------------------------------------------------------
v1.17	2nd November 2011
----------------------------------------------------------------

Cloud
* Added ISteamRemoteStorage::PublishFile(), PublishWorkshopFile(), UpdatePublishedFile(), DeletePublishedFile() which enables sharing, updating, and unsharing of cloud content with the Steam community
* Added ISteamRemoteStorage::EnumerateUserPublishedFiles to enumerate content that a user has shared with the Steam community
* Added ISteamRemoteStorage::GetPublishedFileDetails() which gets the metadata associated with a piece of community shared content
* Added ISteamRemoteStorage::SubscribePublishedFile(), EnumerateUserSubscribedFiles(), and UnsubscribePublishedFiles() which allow for management of community content that a user is interested in and marked as a favorite

User
* Updated ISteamUser::GetAuthSessionTicket(), When creating a ticket for use by the AuthenticateUserTicket Web API, the calling application should wait for the callback GetAuthSessionTicketResponse_t generated by the API call before attempting to use the ticket to ensure that the ticket has been communicated to the server. If this callback does not come in a timely fashion ( 10 - 20 seconds ), your client is not connected to Steam, and the AuthenticeUserTicket will fail because it can not authenticate the user.

Friends
* Added ISteamFriends::RequestFriendRichPresence, which allows requesting rich presence keys for any Steam user playing the same game as you
* Added a set of functions to ISteamFriends which allow games to integrate with Steam Chat. Games can both join group chats, as well as get friends chats and show them in-line in the game. This API isn’t currently used in a game, so there may be some rough edges around the user experience to work out, and some experimentation is required.

Game Servers
* Removed the ISteamMasterServerUpdater interface.  It has been merged into the ISteamGameServer interface, which is used to communicate all game server state changes.
* Significant changes to the game server init sequence.  (See the comments for SteamGameServer_Init  and the ISteamGameServer interface.)
* Removed interface to legacy master server mode
* Groundwork for implementing named steam accounts for game servers
* Old player auth system is deprecated.  It may be removed in a future version of the SDK.

Tools
* Added tool for automated DRM submissions in /sdk/tools/drm/

----------------------------------------------------------------
v1.16	29th July 2011
----------------------------------------------------------------

HTTP
* added ISteamHTTP::SetHTTPRequestRawPostBody() to set the raw body of a POST request
Screenshots
* added ISteamScreenshots interface, which enables adding screenshots to the user's screenshot library and tagging them with location data or relevant players that are visible in the screenshot.  A game can provide screenshots based on game events using WriteScreenshot, AddScreenshotToLibrary, or TriggerScreenshot.  A game can also choose to provide its own screenshots when the Steam screenshot hotkey is pressed by calling HookScreenshots() and listening for the ScreenshotRequested_t callback.

----------------------------------------------------------------
v1.15	1st June 2011
----------------------------------------------------------------

Bug fixes
* Fixed exposing HTTP interface
* Fixed setting AppID for game processes started outside of Steam or which require administrative privileges


----------------------------------------------------------------
v1.14	16th May 2011
----------------------------------------------------------------

Stats and Achievements
* Added a set of functions for accessing global achievement unlock percentages
** RequestGlobalAchievementPercentages() to request the completion percentages from the backend
** GetMostAchievedAchievementInfo() and GetNextMostAchievedAchievementInfo() to iterate achievement completion percentages
** GetAchievementAchievedPercent() to query the global unlock percentage for a specific achievement
* Added a set of functions for accessing global stats values. To enable a global stats set stats as "aggregated" from the Steamworks admin page.
** RequestGlobalStats() to request the global stats data from the backend
** GetGlobalStat() to get the global total for a stat
** GetGlobalStatHistory() to get per day totals for a stat

HTTP
* added ISteamHTTP::GetHTTPDownloadProgressPct() get the progress of an HTTP request


----------------------------------------------------------------
v1.13	26th April 2011
----------------------------------------------------------------

Rich Presence
* added a new Rich Presence system to allow for sharing game specific per user data between users
* ISteamFriends::SetRichPresense() can be used to set key/value presence data for the current user
* ISteamFriends::GetFriendRichPresence() and related functions can be used to retrieve presence data for a particular user
* Two special presence keys exist:
** the "connect" key can be set to specify a custom command line used by friends when joining that user
** the "status" key can be set to specify custom text that will show up in the 'view game info' dialog in the Steam friends list

HTTP
* added ISteamHTTP, which exposes methods for making HTTP requests

Downloadable Content
* added ISteamApps::GetDLCCount() and ISteamApps::BGetDLCDataByIndex() to allow for enumerating DLC content for the current title
* added ISteamApps::InstallDLC() and ISteamApps::UninstallDLC() to control installing optional content

P2P Networking
* added ISteamNetworking::CloseP2PChannelWithUser(), to allow for closing a single channel to a user. When all channels are closed, the connection to that user is automatically closed.
* added ISteamNetworking::AllowP2PPacketRelay(), which can be used to prevent allowing P2P connections from falling back to relay

Voice
* ISteamUser::GetAvailableVoice() & ISteamUser::GetVoice() now take the desired sample rate to determine the number of uncompressed bytes to return
* added ISteamUser::GetVoiceOptimalSampleRate() to return the frequency of the voice data as it's stored internally

Friends
* added ISteamFriends methods to retrieve the list of users the player has recently played with

Content Tool
* all files are now encrypted by default
* add command line option to app creation wizard
* add command line edit option by right clicking on app
* update cache size in CDDB after each build
* look for install scripts at build time and automatically add CDDB flag
* fix language names for chinese
* add menu button to easily rev version
* warn if rebuilding existing version
* allow specifying subfolder when ftp-ing depots to valve
* better error messaging if ftp fails
* clean up various small display bugs
* don't trash ValidOSList tag when updating CDDB

OSX DirectX to OpenGL
* added the graphics layer used to port Valve games to OSX which can now be used by all Steamworks developers
* included in the Steamworks Example application. Can be enabled by building with DX9MODE=1


----------------------------------------------------------------
v1.12	10th November 2010
----------------------------------------------------------------

Cloud
* added a set of function to handle publishing User Generated Content (UGC) files to the backend, and to download others users UGC files. This enables games to have users easily publish & share content with each other.
* Added ISteamRemoteStorage::FileForget() which tells a file to remain on disk but to be removed from the backend. This can be used to manage which files should be synchronized if you have more files to store than your quota allows.
* Added ISteamRemoteStorage::FilePersisted() to tell if the file is set to be synchronized with the backend.
* Added ISteamRemoteStorage::FileDelete() which tells a file to be deleted locally, from cloud, and from other clients that have the file. This can be used to properly delete a save file rather than writing a 1-byte file as a sentinel.
* Added ISteamRemoteStorage::SetSyncPlatforms(), GetSyncPlatforms() to tell steam which platforms a file should be synchronized to. This allows OSX not to download PC-specific files, or vice-versa.
* Added ISteamRemoteStorage::IsCloudEnabledForAccount(), IsCloudEnabledForApp(), and SetCloudEnabledForApp(). When cloud is disabled the APIs still work as normal and an alternate location on disk is not needed. It just means the files will not be synchronized with the backend.

Leaderboards
* added ISteamUserStats::DownloadLeaderboardEntriesForUsers(), which downloads scores for an arbitrary set of users
* added ISteamUserStats::AttachLeaderboardUGC(), to attach a clouded file to a leaderboard entry

Friends
* added ISteamFriends::RequestUserInformation(), to asynchronously request a users persona name & avatar by steamID
* added ISteamFriends::RequestClanOfficerList(), to asynchronously download the set of officers for a clan. GetClanOwner(), GetClanOfficerCount(), and GetClanOfficerByIndex() can then be used to access the data.

Matchmaking
* added k_ELobbyTypePrivate option to creating lobbies - this means that the lobby won't show up to friends or be returned in searches
* added LobbyDataUpdate_t::m_bSuccess, to easily check if a RequestLobbyData() call failed to find the specified lobby

Authentication
* added ISteamApps::GetEarliestPurchaseUnixTime(), for games that want to reward users who have played for a long time
* added ISteamApps::BIsSubscribedFromFreeWeekend(), so games can show different offers or information for users who currently only have rights to play the game due to a free weekend promotion
* added ISteamGameServer::GetAuthSessionTicket(), BeginAuthSession(), EndAuthSession(), and CancelAuthTicket(), matching what exists in ISteamUser. This allows game servers and clients to authenticate each other in a unified manner.

OSX
* The Steamworks Spacewar example now builds/runs on OS X
* The OSX retail install setup application is now contained in goldmaster\disk_assets\SteamRetailInstaller.dmg

PS3
* added several functions regarding PS3 support. This is still a work in progress, and no PS3 binaries are included.


----------------------------------------------------------------
v1.11	23rd August 2010
----------------------------------------------------------------

Networking
* added virtual ports to the P2P networking API to help with routing messages to different systems
* added ISteamUser::BIsBehindNAT() to detect when a user is behind a NAT

Friends / Matchmaking
* added support for retrieving large (184x184) avatars
* added ISteamUser::AdvertiseGame() which can be used send join game info to friends without using the game server APIs

64-bit support
* 64-bit windows binaries are included in the sdk/redistributable_bin/ folder
* VAC and CEG are not yet supported

Authentication
* added ticket based remote authentication library

Other
* added ISteamUser::CheckFileSignature which can be used in conjunction with the signing tab on the partner site to verify that an executable has not been modified


----------------------------------------------------------------
v1.10	20th July 2010
----------------------------------------------------------------

Friends / Matchmaking
* added function ISteamFriends::GetClanTag(), which returns the abbreviation set for a group
* added "stats" and "achievements" options to ISteamFriends::ActivateGameOverlayToUser()
* added function ISteamFriends::ActivateGameOverlayInviteDialog() to open the invite dialog for a specific lobby
* renamed ISteamMatchmaking::SetGameType() to the more correct SetGameTags()

Authentication
* added ISteamUtils::CheckFileSignature(), which can be used to verify that a binary has a valid signature

Other
* added #pragma pack() in several places around structures in headers


----------------------------------------------------------------
v1.09	12th May 2010
----------------------------------------------------------------

Mac Steamworks!
* new binaries in the sdk/redistributable_bin/osx/ folder

Other
* explicit pragma( pack, 8 ) added around all callbacks and structures, for devs who have use a different default packing
* renamed function ISteamGameServer::SetGameType() to the more accurate ISteamGameServer::SetGameTags()


----------------------------------------------------------------
v1.08	27st January 2010
----------------------------------------------------------------

Matchmaking
* added function ISteamMatching::AddRequestLobbyListDistanceFilter(), to specify how far geographically you want to search for other lobbies
* added function ISteamMatching::AddRequestLobbyListResultCountFilter(), to specify how the maximum number of lobby you results you need (less is faster)

Stats & Achievements
* added interface ISteamGameServerStats, which enables access to stats and achievements for users to the game server
* removed function ISteamGameServer::BGetUserAchievementStatus(), now handled by ISteamGameServerStats
* added ISteamUserStats::GetAchievementAndUnlockTime(), which returns if and when a user unlocked an achievement

Other
* added new constant k_cwchPersonaNameMax (32), which is the maximum number of unicode characters a users name can be
* removed ISteamRemoteStorage::FileDelete() - NOTE: it will be back, it's only removed since it hadn't been implemented on the back-end yet
* added function ISteamGameServer::GetServerReputation(), gives returns a game server reputation score based on how long users typically play on the server


----------------------------------------------------------------
v1.07	16th December 2009
----------------------------------------------------------------

* Replaced SteamAPI_RestartApp() with SteamAPI_RestartAppIfNecessary(). This new function detects if the process was started through Steam, and starts the current game through Steam if necessary.
* Added ISteamUtils::BOverlayNeedsPresent() so games with event driven rendering can determine when the Steam overlay needs to draw


----------------------------------------------------------------
v1.06	30th September 2009
----------------------------------------------------------------

Voice
* ISteamUser::GetCompressedVoice() has been replaced with ISteamUser::GetVoice which can be used to retrieve compressed and uncompressed voice data
* Added ISteamUser::GetAvailableVoice() to retrieve the amount of captured audio data that is available

Matchmaking
* Added a new callback LobbyKicked_t that is sent when a user has been disconnected from a lobby
* Through ISteamMatchmakingServers, multiple server list requests of the same type can now be outstanding at the same time

Steamworks Setup Application:
* Streamlined configuration process
* Now supports EULAs greater than 32k bytes

Content Tool
* Added DLC checkbox to depot creation wizard

Other
* Added SteamAPI_IsSteamRunning()
* Added SteamAPI_RestartApp() so CEG users can restart their game through Steam if launched through Windows Games Explorer



----------------------------------------------------------------
v1.05	11th June 2009
----------------------------------------------------------------

Matchmaking
* Added the SteamID of the gameserver to the gameserveritem_t structure (returned only by newer game servers)
* Added ISteamUserStats::GetNumberOfCurrentPlayers(), asyncronously returns the number users currently running this game
* Added k_ELobbyComparisonNotEqual comparision functions for filters
* Added option to use comparison functions for string filters
* Added ISteamMatchmaking::AddRequestLobbyListFilterSlotsAvailable( int nSlotsAvailable ) filter function, so you can find a lobby for a group of users to join
* Extended ISteamMatchmaking::CreateLobby() to take the max number of users in the lobby
* Added ISteamMatchmaking::GetLobbyDataCount(), ISteamMatchmaking::GetLobbyDataByIndex() so you can iterate all the data set on a lobby
* Added ISteamMatchmaking::DeleteLobbyData() so you can clear a key from a lobby
* Added ISteamMatchmaking::SetLobbyOwner() so that ownership of a lobby can be transferred
* Added ISteamMatchmaking::SetLobbyJoinable()
* Added ISteamGameServer::SetGameData(), so game server can set more information that can be filtered for in the server pinging API

Networking
* Added a set of connectionless networking functions for easy use for making peer-to-peer (NAT traversal) connections. Includes supports for windowed reliable sendsand fragementation/re-assembly of large packets. See ISteamNetworking.h for more details.

Leaderboards
* Added enum ELeaderboardUploadScoreMethod and changed ISteamUserStats::UploadLeaderboardScore() to take this - lets you force a score to be changed even if it's worse than the prior score

Callbacks
* Added CCallbackManual<> class to steam_api.h, a version of CCallback<> that doesn't register itself automatically in it's the constructor

Downloadable Content
* Added ISteamUser::UserHasLicenseForApp() and ISteamGameServer::UserHasLicenseForApp() to enable checking if a user owns DLC in multiplayer. See the DLC documentation for more info.

Game Overlay
* ISteamFriends::ActivateGameOverlay() now accepts "Stats" and "Achievements"



----------------------------------------------------------------
v1.04	9th Mar 2009
----------------------------------------------------------------

Added Peer To Peer Multi-Player Authentication/Authorization:
* Allows each peer to verify the unique identity of the peers ( by steam account id ) in their game and determine if that user is allowed access to the game.
* Added to the ISteamUser interface: GetAuthSessionTicket(), BeginAuthSession(), EndAuthSession() and CancelAuthTicket()
* Additional information can be found in the API Overview on the Steamworks site

Added support for purchasing downloadable content in game:
* Added ISteamApps::BIsDlcInstalled() and the DlcInstalled_t callback, which enable a game to check if downloadable content is owned and installed
* Added ISteamFriends::ActivateGameOverlayToStore(), which opens the Steam game overlay to the store page for an appID (can be a game or DLC)

Gold Master Creation:
* It is no longer optional to encrypt depots on a GM
* The GM configuration file now supports an included_depots key, which along with the excluded_depots key, allows you to specify exactly which depots are placed on a GM
* Simplified the configuration process for the setup application
* The documentation for creating a Gold Master has been rewritten and extended. See the Steamworks site for more information.

Added Leaderboards:
* 10k+ leaderboards can now be created programmatically per game, and queried globally or compared to friends
* Added to ISteamUserStats interface
* See SteamworksExample for a usage example

Other:
* Added SteamShutdown_t callback, which will alert the game when Steam wants to shut down
* Added ISteamUtils::IsOverlayEnabled(), which can be used to detect if the user has disabled the overlay in the Steam settings
* Added ISteamUserStats::ResetAllStats(), which can be used to reset all stats (and optionally achievements) for a user
* Moved SetWarningMessageHook() from ISteamClient to ISteamUtils
* Added SteamAPI_SetTryCatchCallbacks,  sets whether or not Steam_RunCallbacks() should do a try {} catch (...) {} around calls to issuing callbacks
* In CCallResult callback, CCallResult::IsActive() will return false and can now reset the CCallResult
* Added support for zero-size depots
* Properly strip illegal characters from depot names



----------------------------------------------------------------
v1.03	16th Jan 2009
----------------------------------------------------------------

Major changes:
* ISteamRemoteStorage interface has been added, which contains functions to store per-user data in the Steam Cloud back-end. 
** To use this, you must first use the partner web site to enable Cloud for your game. 
** The current setting is allowing 1MB of storage per-game per-user (we hope to increase this over time).

Lobby & Matchmaking related changes:
* ISteamFriends::GetFriendGamePlayed() now also return the steamID of the lobby the friend is in, if any. It now takes a pointer to a new FriendGameInfo_t struct, which it fills 
* Removed ISteamFriends::GetFriendsLobbies(), since this is now redundant to ISteamFriends::GetFriendGamePlayed()
* Added enum ELobbyComparison, to set the comparison operator in ISteamMatchmaking::AddRequestLobbyListNumericalFilter()
* Changed ISteamMatchmaking::CreateLobby(), JoinLobby() and RequestLobbyList() to now return SteamAPICall_t handles, so you can easily track if a particular call has completed (see below)
* Added ISteamMatchmaking::SetLobbyType(), which can switch a lobby between searchable (public) and friends-only
* Added ISteamMatchmaking::GetLobbyOwner(), which returns the steamID of the user who is currently the owner of the lobby. The back-end ensures that one and only one user is ever the owner. If that user leaves the lobby, another user will become the owner.

Steam game-overlay interaction:
* Added a new callback GameLobbyJoinRequested_t, which is sent to the game if the user selects 'Join friends game' from the Steam friends list, and that friend is in a lobby. The game should initiate connection to that lobby.
* Changed ISteamFriends::ActivateGameOverlay() can now go to "Friends", "Community", "Players", "Settings", "LobbyInvite", "OfficialGameGroup"
* Added ISteamFriends::ActivateGameOverlayToUser(), which can open a either a chat dialog or another users Steam community profile
* Added ISteamFriends::ActivateGameOverlayToWebPage(), which opens the Steam game-overlay web browser to the specified url

Stats system changes:
* Added ISteamUserStats::RequestUserStats(), to download the current game stats of another user
* Added ISteamUserStats::GetUserStat() and ISteamUserStats::GetUserAchievement() to access the other users stats, once they've been downloaded

Callback system changes:
* Added new method for handling asynchronous call results, currently used by CreateLobby(), JoinLobby(), RequestLobbyList(), and RequestUserStats(). Each of these functions returns a handle, SteamAPICall_t, that can be used to track the completion state of a call.
* Added new object CCallResult<>, which can map the completion of a SteamAPICall_t to a function, and include the right data. See SteamworksExample for a usage example.
* Added ISteamUtils::IsAPICallCompleted(), GetAPICallFailureReason(), and GetAPICallResult(), which can be used to track the state of a SteamAPICall_t (although it is recommended to use CCallResult<>, which wraps these up nicely)

Other:
* Added ISteamGameServer::GetPublicIP(), which is the IP address of a game server as seen by the Steam back-end
* Added "allow relay" parameter to ISteamNetworking::CreateP2PConnectionSocket() and CreateListenSocket(), which specified if being bounced through Steam relay servers is OK if a direct p2p connection fails (will have a much higher latency, but increases chance of making a connection)
* Added IPCFailure_t callback, which will be posted to the game if Steam itself has crashed, or if Steam_RunCallbacks() hasn't been called in a long time



----------------------------------------------------------------
v1.02	4th Sep 2008
----------------------------------------------------------------

The following interfaces have been updated:

ISteamUser

	// Starts voice recording. Once started, use GetCompressedVoice() to get the data
	virtual void StartVoiceRecording( ) = 0;

	// Stops voice recording. Because people often release push-to-talk keys early, the system will keep recording for
	// a little bit after this function is called. GetCompressedVoice() should continue to be called until it returns
	// k_eVoiceResultNotRecording
	virtual void StopVoiceRecording( ) = 0;

	// Gets the latest voice data. It should be called as often as possible once recording has started.
	// nBytesWritten is set to the number of bytes written to pDestBuffer. 
	virtual EVoiceResult GetCompressedVoice( void *pDestBuffer, uint32 cbDestBufferSize, uint32 *nBytesWritten ) = 0;

	// Decompresses a chunk of data produced by GetCompressedVoice(). nBytesWritten is set to the 
	// number of bytes written to pDestBuffer. The output format of the data is 16-bit signed at 
	// 11025 samples per second.
	virtual EVoiceResult DecompressVoice( void *pCompressed, uint32 cbCompressed, void *pDestBuffer, uint32 cbDestBufferSize, uint32 *nBytesWritten ) = 0;

virtual int InitiateGameConnection( void *pAuthBlob, int cbMaxAuthBlob, CSteamID steamIDGameServer, uint32 unIPServer, uint16 usPortServer, bool bSecure ) = 0;

This has been extended to be usable for games that don't use the other parts of Steamworks matchmaking. This allows any multiplayer game to easily notify the Steam client of the IP:Port of the game server the user is connected to, so that their friends can join them via the Steam friends list. Empty values are taken for auth blob.

            virtual bool GetUserDataFolder( char *pchBuffer, int cubBuffer ) = 0;

This function returns a hint as a good place to store per- user per-game data.



ISteamMatchmaking

Added a set of server-side lobby filters, as well as voice chat, lobby member limits, and a way of quickly accessing the list of lobbies a users friends are in.

      // filters for lobbies
      // this needs to be called before RequestLobbyList() to take effect
      // these are cleared on each call to RequestLobbyList()
      virtual void AddRequestLobbyListFilter( const char *pchKeyToMatch, const char *pchValueToMatch ) = 0;
      // numerical comparison - 0 is equal, -1 is the lobby value is less than nValueToMatch, 1 is the lobby value is greater than nValueToMatch
      virtual void AddRequestLobbyListNumericalFilter( const char *pchKeyToMatch, int nValueToMatch, int nComparisonType /* 0 is equal, -1 is less than, 1 is greater than */ ) = 0;
      // sets RequestLobbyList() to only returns lobbies which aren't yet full - needs SetLobbyMemberLimit() called on the lobby to set an initial limit
      virtual void AddRequestLobbyListSlotsAvailableFilter() = 0;

      // returns the details of a game server set in a lobby - returns false if there is no game server set, or that lobby doesn't exist
      virtual bool GetLobbyGameServer( CSteamID steamIDLobby, uint32 *punGameServerIP, uint16 *punGameServerPort, CSteamID *psteamIDGameServer ) = 0;

      // set the limit on the # of users who can join the lobby
      virtual bool SetLobbyMemberLimit( CSteamID steamIDLobby, int cMaxMembers ) = 0;
      // returns the current limit on the # of users who can join the lobby; returns 0 if no limit is defined
      virtual int GetLobbyMemberLimit( CSteamID steamIDLobby ) = 0;

      // asks the Steam servers for a list of lobbies that friends are in
      // returns results by posting one RequestFriendsLobbiesResponse_t callback per friend/lobby pair
      // if no friends are in lobbies, RequestFriendsLobbiesResponse_t will be posted but with 0 results
      // filters don't apply to lobbies (currently)
      virtual bool RequestFriendsLobbies() = 0;


ISteamUtils
      // Sets the position where the overlay instance for the currently calling game should show notifications.
      // This position is per-game and if this function is called from outside of a game context it will do nothing.
      virtual void SetOverlayNotificationPosition( ENotificationPosition eNotificationPosition ) = 0;


ISteamFriends
            virtual int GetFriendAvatar( CSteamID steamIDFriend, int eAvatarSize ) = 0;

This function now takes an eAvatarSize parameter, which can be k_EAvatarSize32x32 or k_EAvatarSize64x64 (previously it always just returned a handle to the 32x32 image)


----------------------------------------------------------------
v1.01  	8th Aug 2008
----------------------------------------------------------------

The Steamworks SDK has been updated to simplfy game server authentication and better expose application state


----------------------------------------------------------------
v1.0:
----------------------------------------------------------------

- Initial Steamworks SDK release
