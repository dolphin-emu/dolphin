void main()
{
    // Transform the vertex position
    gl_Position = gl_ModelViewProjectionMatrix * gl_Vertex;
}
