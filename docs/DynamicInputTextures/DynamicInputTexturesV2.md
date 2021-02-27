# Dolphin Dynamic Input Textures Specification (v2)

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

|Identifier               |Required |
|-------------------------|---------|
|``image``                | **Yes** |
|``emulated_controls``    | **Yes** |
|``host_controls``        | No      |

*image* - the image that has the input buttons you wish to replace, can be upscaled/redrawn if desired.

*emulated_controls* - a map of emulated devices (ex: ``Wiimote1``, ``GCPad2``) each with their own array of emulated keys.  These keys are described below.

*host_controls* - a map of devices (ex: ``DInput/0/Keyboard Mouse``, ``XInput/1/Gamepad``, or blank for wildcard) each with their own array of host keys.  Required if ``default_host_controls`` is not defined in the global section.

#### Emulated Keys

There are two emulated key types supported one that covers a single icon representing a single key in Dolphin and the other covers a single icon that represents multiple keys in Dolphin.

##### Single Key

A key is a json map that has various attributes.  These attributes depend on what the "bind_type" is.  By default the "bind_type" is ``single`` to denote this key matching a single key in Dolphin.

Here is an example of a single key:

```js
{
    "bind_type": "single",
    "key": "Buttons/A",
    "region": [0, 0, 30, 30]
}
```

Since ``single`` binding is the default, you can leave it off too:

```js
{
    "key": "Buttons/A",
    "region": [0, 0, 30, 30]
}
```

Single keys can also optionally have a "tag".  This can be used to denote various things, like for instance a game that has a pressed icon.

Here is an example of a single key with a tag:

```js
{
    "bind_type": "single",
    "key": "Buttons/A",
    "tag": "pressed",
    "region": [0, 0, 30, 30]
}
```

##### Multi Key

The single key case works well when an image maps directly to Dolphin's emulated controller bindings.  However, what about the case where there is a dpad icon?  Dolphin has four mappings for that.  This is where a "bind_type" of ``multi`` is helpful.

When binding a multi button icon, there are two scenarios.  1) The user has all the buttons mapped and there is a corresponding host icon for that.  2) The user hasn't mapped all the buttons, so we must map each to a portion of the image.

Here's an example of that:

```js
{
    "bind_type": "multi",
    "tag": "dpad",
    "region": [0, 0, 45, 45],
    "sub_entries": [
      {
          "bind_type": "single",
          "key": "D-Pad/Up",
          "region": [7.5, 0, 22.5, 15]
      },
      {
          "bind_type": "single",
          "key": "D-Pad/Left",
          "region": [0, 15, 15, 30]
      },
      {
          "bind_type": "single",
          "key": "D-Pad/Right",
          "region": [15, 15, 30, 30]
      },
      {
          "bind_type": "single",
          "key": "D-Pad/Down",
          "region": [7.5, 30, 22.5, 45]
      }
    ]
}
```

All the keys are first checked and if a match occurs, then the top ``region`` above is used.  If a match doesn't occur, then individual replacements happen for the individual keys like normal.


#### Host Keys

At the moment, there is only a single host key type. Keys must match all elements of the ``keys`` array in order for a ``multi`` bind type to match. For instance, in the above example, the following host controls would match assuming normal bindings, while other orderings would not.

```js
{
    "host_controls": {
        "DInput/0/Keyboard Mouse": [
            {
              "keys": ["UP", "LEFT", "DOWN", "RIGHT"],
              "tag": "dpad",
              "image": "keyboard/wasd.png"
            }
        ],
        "XInput/0/Gamepad": [
            {
              "keys": ["`Pad N`", "`Pad E`", "`Pad S`", "`Pad W`"],
              "tag": "dpad",
              "image": "gamepad/dpad.png"
            }
        ]
    }
}
```

#### Global fields in the JSON applied to all textures

The following fields apply to all textures in the json file:

|Identifier               |
|-------------------------|
|``generated_folder_name``|
|``preserve_aspect_ratio``|
|``default_host_controls``|

*generated_folder_name* - the folder name that the textures will be generated into.  Optional, defaults to '<gameid>_Generated'

*preserve_aspect_ratio* - will preserve the aspect ratio when replacing the colored boxes with the image.  Optional, defaults to on

*default_host_controls* - a default map of devices to a map of host controls (keyboard or gamepad values) that each maps to an image.

#### Full Examples

Here's an example of generating a single image with the "A" and "B" Wiimote1 buttons replaced to either keyboard buttons or gamepad buttons depending on the user device and the input mapped:

```js
{
    "specification": 2.0,
    "generated_folder_name": "MyDynamicTexturePack",
    "preserve_aspect_ratio": false,
    "output_textures":
    {
        "tex1_128x128_02870c3b015d8b40_5.png":
        {
            "image": "icons.png",
            "emulated_controls": {
                "Wiimote1": [
                    {
                      "key": "Buttons/A",
                      "region": [0, 0, 30, 30]
                    },
                    {
                      "key": "Buttons/A",
                      "region": [500, 550, 530, 580]
                    },
                    {
                      "key": "Buttons/B",
                      "region": [100, 342, 132, 374]
                    }
                ]
            },
            "host_controls": {
                "DInput/0/Keyboard Mouse": [
                    {
                      "keys": ["A"],
                      "image": "keyboard/a.png"
                    },
                    {
                      "keys": ["B"],
                      "image": "keyboard/b.png"
                    }
                ],
                "XInput/0/Gamepad": [
                    {
                      "keys": ["`Button A`"],
                      "image": "gamepad/a.png"
                    },
                    {
                      "keys": ["`Button B`"],
                      "image": "gamepad/b.png"
                    }
                ]
            }
        }
    }
}
```

Here's the same example but the "A" button has a "pressed" state for one of its icons:

```js
{
    "specification": 2.0,
    "generated_folder_name": "MyDynamicTexturePack",
    "preserve_aspect_ratio": false,
    "output_textures":
    {
        "tex1_128x128_02870c3b015d8b40_5.png":
        {
            "image": "icons.png",
            "emulated_controls": {
                "Wiimote1": [
                    {
                      "key": "Buttons/A",
                      "region": [0, 0, 30, 30]
                    },
                    {
                      "key": "Buttons/A",
                      "tag": "pressed",
                      "region": [500, 550, 530, 580]
                    },
                    {
                      "key": "Buttons/B",
                      "region": [100, 342, 132, 374]
                    }
                ]
            },
            "host_controls": {
                "DInput/0/Keyboard Mouse": [
                    {
                      "keys": ["A"],
                      "image": "keyboard/a.png"
                    },
                    {
                      "keys": ["A"],
                      "tag": "pressed",
                      "image": "keyboard/a_pressed.png"
                    },
                    {
                      "keys": ["B"],
                      "image": "keyboard/b.png"
                    }
                ],
                "XInput/0/Gamepad": [
                    {
                      "keys": ["`Button A`"],
                      "image": "gamepad/a.png"
                    },
                    {
                      "keys": ["`Button A`"],
                      "tag": "pressed",
                      "image": "gamepad/a_pressed.png"
                    },
                    {
                      "keys": ["`Button B`"],
                      "image": "gamepad/b.png"
                    }
                ]
            }
        }
    }
}
```

As an example, you are writing a pack for a single-player game.  You may want to provide DS4 controller icons but not care what device the user is using.  You can use a wildcard for that:

```js
{
    "specification": 2.0,
    "preserve_aspect_ratio": false,
    "output_textures":
    {
        "tex1_128x128_02870c3b015d8b40_5.png":
        {
            "image": "icons.png",
            "emulated_controls": {
                "Wiimote1": [
                  {
                    "key": "Buttons/A",
                    "region": [0, 0, 30, 30]
                  },
                  {
                    "key": "Buttons/A",
                    "region": [500, 550, 530, 580]
                  },
                  {
                    "key": "Buttons/B"
                    "region": [100, 342, 132, 374]
                  }
                ]
            },
            "host_controls": {
                "": [
                    {"keys": ["`Button X`"], "image": "ds4/x.png"},
                    {"keys": ["`Button Y`"], "image": "ds4/y.png"}
                ]
            }
        }
    }
}
```

Here's an example of generating multiple images but using defaults from the global section except for one texture:

```js
{
    "specification": 2.0,
    "default_host_controls": {
        "DInput/0/Keyboard Mouse": [
            {"keys": ["A"], "image": "keyboard/a.png"},
            {"keys": ["B"], "image": "keyboard/b.png"}
        ]
    },
    "default_device": "DInput/0/Keyboard Mouse",
    "output_textures":
    {
        "tex1_21x26_5cbc6943a74cb7ca_67a541879d5fe615_9.png":
        {
            "image": "icons1.png",
            "emulated_controls": {
                "Wiimote1": []
                  {
                      "key": "Buttons/A",
                      "region": [62, 0, 102, 40]
                  },
                  {
                      "key": "Buttons/B",
                      "region": [100, 342, 132, 374]
                  }
                ]
            }
        },
        "tex1_21x26_5e71c27dad9cda76_3d76bd5d1e73c3b1_9.png":
        {
            "image": "icons2.png",
            "emulated_controls": {
                "Wiimote1": [
                    {
                        "key": "Buttons/A",
                        "region": [857, 682, 907, 732]
                    },
                    {
                        "key" :"Buttons/B",
                        "region": [100, 342, 132, 374]
                    }
                ]
            }
        },
        "tex1_24x24_003f3a17f66f1704_82848f47946caa41_9.png":
        {
            "image": "icons3.png",
            "emulated_controls": {
                "Wiimote1": [
                    {
                        "key": "Buttons/A",
                        "region": [0, 0, 30, 30]
                    },
                    {
                        "key": "Buttons/A",
                        "region": [500, 550, 530, 580]
                    },
                    {
                        "key": "Buttons/B",
                        "region": [100, 342, 132, 374]
                    }
                ]
            },
            "host_controls":
            {
                "DInput/0/Keyboard Mouse": [
                  {"keys": ["A"], "image": "keyboard/a_special.png"},
                  {"keys": ["B"], "image": "keyboard/b.png"}
                ]
            }
        }
    }
}
```
