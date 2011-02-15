//dummy shader:
uniform samplerRECT samp0 : register(s0);

void main(out float4 ocol0 : COLOR0, in float2 uv0 : TEXCOORD0)
{
  float4 c0 = texRECT(samp0, uv0).rgba;
  ocol0 = float4(c0.r, c0.g, c0.b, c0.a);
}
/*
And now that's over with, the contents of this readme file!
For best results, turn Wordwrap formatting on...
The shaders shown in the dropdown box in the video plugin configuration window are kept in the directory named User/Data/Shaders. They are linked in to the dolphin source from the repository at <http://dolphin-shaders-database.googlecode.com/svn/trunk/>. See <http://code.google.com/p/dolphin-shaders-database/wiki/Documentation> for more details on the way shaders work.

This file will hopefully hold more content in future...
*/