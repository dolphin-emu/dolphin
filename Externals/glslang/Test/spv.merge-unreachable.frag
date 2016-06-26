#version 450
layout(location=1) in highp vec4 v;
void main (void)
{
  if (v == vec4(0.1,0.2,0.3,0.4)) discard;
  else return;
}
