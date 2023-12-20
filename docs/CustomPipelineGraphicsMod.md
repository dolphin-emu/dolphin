# Dolphin Custom Pipeline Specification

Dolphin provides content creators a way to overwrite its internal graphics pipeline data using graphics mods.  At the moment, this supports modifying only the pixel shader.  This document will describe the specification and give some examples.

## Graphics mod metadata format

This feature is powered by graphics mods.  This document assumes the user is familiar with them and will only detail the action specific data needed to trigger this capability.

The action type for this feature is `custom_pipeline`.  This action has the following data:

|Identifier               |Required | Since |
|-------------------------|---------|-------|
|``passes``               | **Yes** | v1    |

`passes` is an array of pass blobs.  Note that at the moment, Dolphin only supports a single pass.  Each pass can have the following data:

|Identifier               |Required | Since |
|-------------------------|---------|-------|
|``pixel_material_asset`` | **Yes** | v1    |

Here `pixel_material_asset` is the name of a material asset.

A full example is given below:

```json
{
    "assets": [
        {
            "name": "material_replace_normal",
            "data":
            {
                "": "normal.material.json"
            }
        },
        {
            "name": "shader_replace_normal",
            "data":
            {
                "metadata": "replace_normal.shader.json",
                "shader": "replace_normal.glsl"
            }
        },
        {
            "name": "normal_texture",
            "data":
            {
                "texture": "normal_texture.png"
            }
        }
    ],
    "features": [
        {
            "action": "custom_pipeline",
            "action_data": {
              "passes":  [
                {
                  "pixel_material_asset": "material_replace_normal"
                }
              ]
            },
            "group": "PipelineTarget"
        }
    ],
    "groups": [
       {
            "name": "PipelineTarget",
            "targets": [
                {
                    "texture_filename": "tex1_512x512_m_afdbe7efg332229e_14",
                    "type": "draw_started"
                },
                {
                    "texture_filename": "tex1_512x512_m_afdbe7efg332229e_14",
                    "type": "create_texture"
                }
            ]
       }
    ]
}
```

## The shader format

The shaders are written in GLSL and converted to the target shader that the backend uses internally.  The user is expected to provide an entrypoint with the following signature:

```
vec4 custom_main( in CustomShaderData data )
```

`CustomShaderData` encompasses all the data that Dolphin will pass to the user (in addition to the `samp` variable outlined above which is how textures are accessed).  It has the following structure:

|Name                         | Type                    | Since | Description                                                                                   |
|-----------------------------|-------------------------|-------|-----------------------------------------------------------------------------------------------|
|``position``                 | vec3                    | v1    | The position of this pixel in _view space_                                                    |
|``normal``                   | vec3                    | v1    | The normal of this pixel in _view space_                                                      |
|``texcoord``                 | vec3[]                  | v1    | An array of texture coordinates, the amount available is specified by ``texcoord_count``      |
|``texcoord_count``           | uint                    | v1    | The count of texture coordinates                                                              |
|``texmap_to_texcoord_index`` | uint[]                  | v1    | An array of texture units to texture coordinate values                                        |
|``lights_chan0_color``       | CustomShaderLightData[] | v1    | An array of color lights for channel 0, the amount is specified by ``light_chan0_color_count``|
|``lights_chan0_alpha``       | CustomShaderLightData[] | v1    | An array of alpha lights for channel 0, the amount is specified by ``light_chan0_alpha_count``|
|``lights_chan1_color``       | CustomShaderLightData[] | v1    | An array of color lights for channel 1, the amount is specified by ``light_chan1_color_count``|
|``lights_chan1_alpha``       | CustomShaderLightData[] | v1    | An array of alpha lights for channel 1, the amount is specified by ``light_chan1_alpha_count``|
|``ambient_lighting``         | vec4[]                  | v1    | An array of ambient lighting values.  Count is two, one for each color channel                |
|``base_material``            | vec4[]                  | v1    | An array of the base material values.  Count is two, one for each color channel               |
|``tev_stages``               | CustomShaderTevStage[]  | v1    | An array of TEV stages, the amount is specified by ``tev_stage_count``                        |
|``tev_stage_count``          | uint                    | v1    | The count of TEV stages                                                                       |
|``final_color``              | vec4                    | v1    | The final color generated by Dolphin after all TEV stages are executed                        |
|``time_ms``                  | uint                    | v1    | The time that has passed in milliseconds, since the game was started.  Useful for animating   |

`CustomShaderLightData` is used to denote lighting data the game is applying when rendering the specific draw call.  It has the following structure:

|Name                     | Type                    | Since | Description                                                                                     |
|-------------------------|-------------------------|-------|-------------------------------------------------------------------------------------------------|
|``position``             | vec3                    | v1    | The position of the light in _view space_                                                       |
|``direction``            | vec3                    | v1    | The direction in _view space_ the light is pointing (only applicable for point and spot lights) |
|``color``                | vec3                    | v1    | The color of the light                                                                          |
|``attenuation_type``     | uint                    | v1    | The attentuation type of the light.  See details below                                          |
|``cosatt``               | vec4                    | v1    | The cos attenuation values used                                                                 |
|``distatt``              | vec4                    | v1    | The distance attenuation values used                                                            |

The `attenuation_type` is defined as a `uint` but is effecitvely an enumeration.  It has the following values:

|Name                                              | Since | Description                                                             |
|--------------------------------------------------|-------|-------------------------------------------------------------------------|
|``CUSTOM_SHADER_LIGHTING_ATTENUATION_TYPE_POINT`` | v1    | This value denotes the lighting attentuation is for a point light       |
|``CUSTOM_SHADER_LIGHTING_ATTENUATION_TYPE_DIR``   | v1    | This value denotes the lighting attentuation is for a directional light |
|``CUSTOM_SHADER_LIGHTING_ATTENUATION_TYPE_SPOT``  | v1    | This value denotes the lighting attentuation is for a directional light |


`CustomShaderTevStage` is used to denote the various TEV operations.  Each operation describes a graphical operation that the game is applying when rendering the specific draw call.  It has the following structure:

|Name                     | Type                             | Since | Description                                                                   |
|-------------------------|----------------------------------|-------|-------------------------------------------------------------------------------|
|``input_color``          | CustomShaderTevStageInputColor[] | v1    | The four color inputs that are used to produce the final output of this stage |
|``input_alpha``          | CustomShaderTevStageInputAlpha[] | v1    | The four alpha inputs that are used to produce the final output of this stage |
|``texmap``               | uint                             | v1    | The texture unit for this stage                                               |
|``output_color``         | vec4                             | v1    | The final output color this stage produces                                    |


`CustomShaderTevStageInputColor` is a single input TEV operation for a color value.  It has the following structure:

|Name                     | Type | Since | Description                                     |
|-------------------------|------|-------|-------------------------------------------------|
|``input_type``           | uint | v1    | The input type of the input.  See details below |
|``value``                | vec3 | v1    | The value of input                              |

The `input_type` is defined as a `uint` but is effectively an enumeration.  it has the following values:

|Name                                              | Since | Description                                                               |
|--------------------------------------------------|-------|---------------------------------------------------------------------------|
|``CUSTOM_SHADER_TEV_STAGE_INPUT_TYPE_PREV``       | v1    | The value is provided by the last stage                                   |
|``CUSTOM_SHADER_TEV_STAGE_INPUT_TYPE_COLOR``      | v1    | The value is provided by the color data                                   |
|``CUSTOM_SHADER_TEV_STAGE_INPUT_TYPE_TEX``        | v1    | The value is provided by a texture                                        |
|``CUSTOM_SHADER_TEV_STAGE_INPUT_TYPE_RAS``        | v1    |                                                                           |
|``CUSTOM_SHADER_TEV_STAGE_INPUT_TYPE_KONST``      | v1    | The value is a constant value defined by the software                     |
|``CUSTOM_SHADER_TEV_STAGE_INPUT_TYPE_NUMERIC``    | v1    | The value is a constant numeric value like vec3(0, 0, 0) or vec3(1, 1, 1) |

`CustomShaderTevStageInputAlpha` is a single input TEV operation for an alpha value.  It has the following structure:

|Name                     | Type | Since | Description                                                                   |
|-------------------------|------|-------|-------------------------------------------------------------------------------|
|``input_type``           | uint | v1    | The input type of the input.  See `input_type` for color input stages         |
|``value``                | uint | v1    | The value of input                                                            |


## Examples

Below are a handful of examples.

### Single color

The following shader displays the color red on the screen:

```glsl
vec4 custom_main( in CustomShaderData data )
{
	return vec4(1.0, 0.0, 0.0, 1.0);
}
```

### Normal

The following shader displays the normal on the screen:

```glsl
vec4 custom_main( in CustomShaderData data )
{
	return vec4(data.normal * 0.5 + 0.5, 1);
}
```

### Reading a texture

The following shader displays the contents of the texture denoted in the shader asset as `MY_TEX` with the first texture coordinate data:

```glsl
vec4 custom_main( in CustomShaderData data )
{
	return texture(samp_MY_TEX, TEX_COORD0);
}
```

### Capturing the first texture the game renders with

The following shader would display the contents of the first texture the game uses, ignoring any other operations.  If no stages are available or none exist with a texture it would use the final color of all the staging operations:

```glsl
vec4 custom_main( in CustomShaderData data )
{
	vec4 final_color = data.final_color;
	uint texture_set = 0;
	for (uint i = 0; i < data.tev_stage_count; i++)
	{
		// There are 4 color inputs
		for (uint j = 0; j < 4; j++)
		{
			if (data.tev_stages[i].input_color[j].input_type == CUSTOM_SHADER_TEV_STAGE_INPUT_TYPE_TEX && texture_set == 0)
			{
				final_color = vec4(data.tev_stages[i].input_color[j].value, 1.0);
				texture_set = 1;
			}
		}
	}

	return final_color;
}
```

### Applying lighting with a point type attenuation

The following shader would apply the lighting for any point lights used during the draw for channel 0's color lights, using blue as a base color:

```glsl
vec4 custom_main( in CustomShaderData data )
{
	float total_diffuse = 0;
	for (int i = 0; i < data.light_chan0_color_count; i++)
	{
		if (data.lights_chan0_color[i].attenuation_type == CUSTOM_SHADER_LIGHTING_ATTENUATION_TYPE_POINT)
		{
			vec3 light_dir = normalize(data.lights_chan0_color[i].position - data.position.xyz);
			float attn = (dot(normal, light_dir) >= 0.0) ? max(0.0, dot(normal, data.lights_chan0_color[i].direction.xyz)) : 0.0;
			vec3 cosAttn = data.lights_chan0_color[i].cosatt.xyz;
			vec3 distAttn = data.lights_chan0_color[i].distatt.xyz;
			attn = max(0.0, dot(cosAttn, vec3(1.0, attn, attn*attn))) / dot(distAttn, vec3(1.0, attn, attn * attn));
			total_diffuse += attn * max(0.0, dot(normal, light_dir));
		}
	}
	return vec4(total_diffuse * vec3(0, 0, 1), 1);
}
```