# Dolphin Resource Pack Specification v2

This document is to be used as a reference when creating resource packs or writing programs that are to be capable of handling them.

## Format
Dolphin Resource Packs are ``zip`` archives with the following file structure:

```
resource-pack.zip
\__ manifest.json
\__ logo.png (optional)
\__ textures (Directory)
    \__ GAMEID (Directory)
        \___ TEXTURES GO HERE
```

**If you intend to use v1:** Your zip file may not be compressed or else **your pack will not load** (You can create uncompressed zips with software like 7-Zip).  
(This is to ensure that loading textures *directly* from resource packs remains a viable option in the future)  

**v2:** Supports compressed texture packs if ``compressed`` is ``true``.

``GAMEID`` can be one or multiple directories which are named after:
* a complete Game ID (e.g. ``SMNE01`` for "New Super Mario Bros. Wii (NTSC)")
* one without a region (e.g. ``SMN`` for "New Super Mario Bros. Wii (All regions)").


### logo.png

The logo you provide has to be in ``PNG`` format and should not exceed dimensions of ``256x256`` pixels.
The alpha channel is supported so your logo can be partially transparent.

### manifest.json

The ``manifest.json`` file contains metadata that helps users identify your resource pack and is therefore mandatory.

#### Available fields

|Identifier     |Function                                 |Required | Since |
|---------------|-----------------------------------------|---------|-------|
|``name``       | Name of your texture pack               | **Yes** | v1    |
|``id``         | Alphanumeric identifier for your pack   | **Yes** | v1    |
|``version``    | Version (any string)                    | **Yes** | v1    |
|``description``| Description                             | No      | v1    |
|``authors``    | List of authors                         | No      | v1    |
|``website``    | Link to your website(with protocol!)    | No      | v1    |
|``compressed`` | Allows you to compress your textures (1)| No      | v2    |

* (1) This will have the side effect that if Dolphin implements loading textures from the texture pack directly, your pack *might* not be supported

If your manifest doesn't contain all required fields, **it won't load**.

#### Example

Here's an example ``manifest.json`` that contains all supported fields:

```js
{
"name": "My Texture Pack!",
"id" : "my-texture-pack-by-doe",
"version": "2",
"authors": ["John Doe", "Jane Doe"],
"website": "https://example.com/my-texture-pack",
"description": "A cool texture pack made by me!"
}
```

## Conflicts

If there are conflicts (two texture packs providing the same file), they are resolved using the user-assigned order in which the higher entry in the list always overrides the ones that come afterwards.

## Installation

Resource packs are loaded from ``$DOLPHIN_USER_FOLDER/ResourcePacks``.

## Removal

Please remove the resource packs via the resource pack manager instead of just deleting them so they don't remain permanently activated.

## Activation

The actual installation process (*Activation*) just copies the contents of the pack's ``textures`` directory into ``$DOLPHIN_USER_FOLDER/Load/Textures``.

## Deactivation

Texture packs will be removed (*Deactivated*) by removing their files from ``$DOLPHIN_USER_FOLDER/Load/Textures``.

If there's an *active higher priority texture pack* that provides the same file as a lower priority one, Dolphin won't remove this file as it has already been overriden.

## Migration

If a user performs a migration, *pre-resource-pack files* that cannot be assigned to any particular pack *are combined into a ``legacy.zip``* so they can be managed like any other resource pack.
