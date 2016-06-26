#version 400 core

in double d;   // ERROR, no doubles
in dvec3 d3;   // ERROR, no doubles
in dmat4 dm4;  // ERROR, no doubles

void main()
{
}
