uniform vec2 storm_position;
uniform float storm_total_radius;
uniform float storm_inner_radius;

void main()
{
    vec4 vertex = gl_ModelViewMatrix * gl_Vertex;
    vec2 offset = vertex.xy - storm_position;
    float len = length(offset);
    if (len < storm_total_radius)
    {
        float push_distance = storm_inner_radius + len / storm_total_radius * (storm_total_radius - storm_inner_radius);
        vertex.xy = storm_position + normalize(offset) * push_distance;
    }

    gl_Position = gl_ProjectionMatrix * vertex;
    gl_TexCoord[0] = gl_TextureMatrix[0] * gl_MultiTexCoord0;
    gl_FrontColor = gl_Color;
}
