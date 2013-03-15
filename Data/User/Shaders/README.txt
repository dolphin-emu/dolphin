//dummy shader:
uniform sampler2D samp9;

out vec4 ocol0;
in vec2 uv0;

void main()
{
  ocol0 = texture(samp9, uv0);
}

/*
And now that's over with, the contents of this readme file!
For best results, turn Wordwrap formatting on...
The shaders shown in the dropdown box in the video plugin configuration window are kept in the directory named User/Data/Shaders. They are linked in to the dolphin source from the repository at <http://dolphin-shaders-database.googlecode.com/svn/trunk/>. See <http://code.google.com/p/dolphin-shaders-database/wiki/Documentation> for more details on the way shaders work.

This file will hopefully hold more content in future...
*/