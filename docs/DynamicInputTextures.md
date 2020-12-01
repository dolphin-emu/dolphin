# Dolphin Dynamic Input Textures Specification (v1)

## Format
Dynamic Input Textures are generated textures based on a user's input formed from a group of png files and json files.

```
\__ Dolphin User Directory
    \__ Load (Directory)
        \__ DynamicInputTextures (Directory)
            \__ FOLDER (Directory)
                \__ PNG and JSON GO HERE
```

``FOLDER`` can be one or multiple directories which are named after:
* a complete Game ID (e.g. ``SMNE01`` for "New Super Mario Bros. Wii (NTSC)")
* one without a region (e.g. ``SMN`` for "New Super Mario Bros. Wii (All regions)").
* Any folder name but with an empty ``<GAMEID>.txt`` underneath it

## How to enable

Place the files in the format above and ensure that "Load Custom Textures" is enabled under the advanced tab of the graphics settings.

### PNG files

At a minimum two images are required to support the generation and any number of 'button' images.  These need to be in PNG format.

### JSON files

You need at least a single json file that describes the generation parameters.  You may have multiple JSON files if you prefer that from an organizational standpoint.

#### Possible fields in the JSON for a texture

In each json, one or more generated textures can be specified.  Each of those textures can have the following fields:

|Identifier               |Required | Since |
|-------------------------|---------|-------|
|``image``                | **Yes** | v1    |
|``emulated_controls``    | **Yes** | v1    |
|``host_controls``        | No      | v1    |

*image* - the image that has the input buttons you wish to replace, can be upscaled/redrawn if desired.

*emulated_controls* - a map of emulated devices (ex: ``Wiimote1``, ``GCPad2``) each with their own section of emulated buttons that map to an array of "regions".  Each region is a rectangle defined as a json array of four entries.  The rectangle bounds are offsets into the image where the replacement occurs (left-coordinate, top-coordinate, right-coordinate, bottom-coordinate).

*host_controls* - a map of devices (ex: ``DInput/0/Keyboard Mouse``, ``XInput/1/Gamepad``, or blank for wildcard) each with their own section of host buttons (keyboard or gamepad values) that each map to an image. This image will act as a replacement in the original image if this key is mapped to one of the buttons under the ``emulated_controls`` section.  Required if ``default_host_controls`` is not defined in the global section.

#### Global fields in the JSON applied to all textures

The following fields apply to all textures in the json file:

|Identifier               | Since |
|-------------------------|-------|
|``generated_folder_name``| v1    |
|``preserve_aspect_ratio``| v1    |
|``default_host_controls``| v1    |

*generated_folder_name* - the folder name that the textures will be generated into.  Optional, defaults to '<gameid>_Generated'

*preserve_aspect_ratio* - will preserve the aspect ratio when replacing the colored boxes with the image.  Optional, defaults to on

*default_host_controls* - a default map of devices to a map of host controls (keyboard or gamepad values) that each maps to an image.

#### Examples

Here's an example of generating a single image with the "A" and "B" Wiimote1 buttons replaced to either keyboard buttons or gamepad buttons depending on the user device and the input mapped:

```js
{
    "generated_folder_name": "MyDynamicTexturePack",
    "preserve_aspect_ratio": false,
    "output_textures":
    {
        "tex1_128x128_02870c3b015d8b40_5.png":
        {
            "image": "icons.png",
            "emulated_controls": {
                "Wiimote1":
                {
                    "Buttons/A": [
                      [0, 0, 30, 30],
                      [500, 550, 530, 580],
                    ]
                    "Buttons/B": [
                      [100, 342, 132, 374]
                    ]
                }
            },
            "host_controls": {
                "DInput/0/Keyboard Mouse": {
                    "A": "keyboard/a.png",
                    "B": "keyboard/b.png"
                },
                "XInput/0/Gamepad": {
                    "`Button A`": "gamepad/a.png",
                    "`Button B`": "gamepad/b.png"
                }
            }
        }
    }
}
```

As an example, you are writing a pack for a single-player game.  You may want to provide DS4 controller icons but not care what device the user is using.  You can use a wildcard for that:

```js
{
    "preserve_aspect_ratio": false,
    "output_textures":
    {
        "tex1_128x128_02870c3b015d8b40_5.png":
        {
            "image": "icons.png",
            "emulated_controls": {
                "Wiimote1":
                {
                    "Buttons/A": [
                      [0, 0, 30, 30],
                      [500, 550, 530, 580]
                    ]
                    "Buttons/B": [
                      [100, 342, 132, 374]
                    ]
                }
            },
            "host_controls": {
                "": {
                    "`Button X`": "ds4/x.png",
                    "`Button Y`": "ds4/y.png"
                }
            }
        }
    }
}
```

Here's an example of generating multiple images but using defaults from the global section except for one texture:

```js
{
    "default_host_controls": {
        "DInput/0/Keyboard Mouse": {
            "A": "keyboard/a.png",
            "B": "keyboard/b.png"
        }
    },
    "default_device": "DInput/0/Keyboard Mouse",
    "output_textures":
    {
        "tex1_21x26_5cbc6943a74cb7ca_67a541879d5fe615_9.png":
        {
            "image": "icons1.png",
            "emulated_controls": {
                "Wiimote1":
                {
                    "Buttons/A": [
                      [62, 0, 102, 40]
                    ]
                    "Buttons/B": [
                      [100, 342, 132, 374]
                    ]
                }
            }
        },
        "tex1_21x26_5e71c27dad9cda76_3d76bd5d1e73c3b1_9.png":
        {
            "image": "icons2.png",
            "emulated_controls": {
                "Wiimote1":
                {
                    "Buttons/A": [
                      [857, 682, 907, 732]
                    ]
                    "Buttons/B": [
                      [100, 342, 132, 374]
                    ]
                }
            }
        },
        "tex1_24x24_003f3a17f66f1704_82848f47946caa41_9.png":
        {
            "image": "icons3.png",
            "emulated_controls": {
                "Wiimote1":
                {
                    "Buttons/A": [
                      [0, 0, 30, 30],
                      [500, 550, 530, 580]
                    ]
                    "Buttons/B": [
                      [100, 342, 132, 374]
                    ]
                }
            },
            "host_controls":
            {
                "DInput/0/Keyboard Mouse": {
                    "A": "keyboard/a_special.png",
                    "B": "keyboard/b.png"
                }
            }
        }
    }
}
```
