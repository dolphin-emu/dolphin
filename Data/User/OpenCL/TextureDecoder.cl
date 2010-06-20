// Copyright (C) 2003 Dolphin Project.

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, version 2.0.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License 2.0 for more details.

// A copy of the GPL 2.0 should have been included with the program.
// If not, see http://www.gnu.org/licenses/

// Official SVN repository and contact information can be found at
// http://code.google.com/p/dolphin-emu/

kernel void DecodeI4(global uchar *dst,
                     const global uchar *src, int width)
{
    int x = get_global_id(0) * 8, y = get_global_id(1) * 8;
    int srcOffset = x + y * width / 8;
    for (int iy = 0; iy < 8; iy++)
    {
        uchar4 val = vload4(srcOffset, src);
        uchar8 res;
        res.even = (val >> (uchar4)4) & (uchar4)0x0F;
        res.odd = val & (uchar4)0x0F;
        res |= res << (uchar8)4;
        vstore8(res, 0, dst + ((y + iy)*width + x));
        srcOffset++;
    }
}

kernel void DecodeI8(global uchar *dst,
                     const global uchar *src, int width)
{
    int x = get_global_id(0) * 8, y = get_global_id(1) * 4;
    int srcOffset = ((x * 4) + (y * width)) / 8;
    for (int iy = 0; iy < 4; iy++)
    {
        vstore8(vload8(srcOffset, src),
                0, dst + ((y + iy)*width + x));
        srcOffset++;
    }
}

kernel void DecodeIA8(global uchar *dst,
                      const global uchar *src, int width)
{
    int x = get_global_id(0) * 4, y = get_global_id(1) * 4;
    int srcOffset = ((x * 4) + (y * width)) / 4;
    for (int iy = 0; iy < 4; iy++)
    {
        uchar8 val = vload8(srcOffset++, src);
        uchar8 res;
        res.odd = val.even;
        res.even = val.odd;
        vstore8(res, 0, dst + ((y + iy)*width + x) * 2);
    }
}

kernel void DecodeIA4(global uchar *dst,
                     const global uchar *src, int width)
{
    int x = get_global_id(0) * 8, y = get_global_id(1) * 4;
    int srcOffset = ((x * 4) + (y * width)) / 8;
    for (int iy = 0; iy < 4; iy++)
    {
        uchar8 val = vload8(srcOffset++, src);
        uchar16 res;
        res.odd = (val >> (uchar8)4);
        res.even  = val & (uchar8)0x0F;
        res |= res << (uchar16)4;
        vstore16(res, 0, dst + ((y + iy)*width + x) * 2);
    }
}

kernel void DecodeRGBA8(global uchar *dst,
                      const global uchar *src, int width)
{
    int x = get_global_id(0) * 4, y = get_global_id(1) * 4;
    int srcOffset = (x * 2) + (y * width) / 2;
    for (int iy = 0; iy < 4; iy++)
    {
        uchar8 ar = vload8(srcOffset, src);
        uchar8 gb = vload8(srcOffset + 4, src);
        uchar16 res;
        res.even.even = gb.odd;
        res.even.odd = ar.odd;
        res.odd.even = gb.even;
        res.odd.odd = ar.even;
        vstore16(res, 0, dst + ((y + iy)*width + x) * 4);
        srcOffset++;
    }
}

kernel void DecodeRGB565(global ushort *dst,
                         const global uchar *src, int width)
{
    int x = get_global_id(0) * 4, y = get_global_id(1) * 4;
    int srcOffset = x + (y * width) / 4;
    for (int iy = 0; iy < 4; iy++)
    {
        uchar8 val = vload8(srcOffset++, src);
        vstore4(upsample(val.even, val.odd), 0, dst + ((y + iy)*width + x));
    }
}

kernel void DecodeRGB5A3(global uchar *dst,
                         const global uchar *src, int width)
{
    int x = get_global_id(0) * 4, y = get_global_id(1) * 4;
    int srcOffset = x + (y * width) / 4;
    uchar8 val;
    uchar16 resNoAlpha, resAlpha, res, choice;
    uchar4 gNoAlpha, aAlpha;
    #define iterateRGB5A3() \
    val = vload8(srcOffset++, src); \
    gNoAlpha = (val.even << (uchar4)6) | (val.odd >> (uchar4)2); \
    resNoAlpha.s26AE = bitselect(val.even >> (uchar4)4,     val.even << (uchar4)1, (uchar4)0xFFF); \
    resNoAlpha.s159D = bitselect(gNoAlpha >> (uchar4)5,     gNoAlpha,              (uchar4)0xFFF); \
    resNoAlpha.s048C = bitselect(val.odd >> (uchar4)2,      val.odd << (uchar4)3,  (uchar4)0xFFF); \
    resNoAlpha.s37BF = (uchar4)(0xFF); \
    resAlpha.s26AE = val.even; \
    resAlpha.s159D = val.odd >> (uchar4)4; \
    resAlpha.s048C = val.odd; \
    resAlpha &= (uchar16)0x0F; \
    resAlpha |= (resAlpha << (uchar16)4); \
    resAlpha.s37BF = val.even << (uchar4)1 & (uchar4)0xE0; \
    resAlpha.s37BF |= ((resAlpha.s37BF >> (uchar4)3) & (uchar4)0x1C) \
                   | ((resAlpha.s37BF >> (uchar4)6) & (uchar4)0x3); \
    choice = (uchar16)((uchar4)(val.even.s0), \
                       (uchar4)(val.even.s1), \
                       (uchar4)(val.even.s2), \
                       (uchar4)(val.even.s3)); \
    vstore16(select(resAlpha, resNoAlpha, choice), 0, dst + (y * width + x) * 4); \
    dst += width*4; // This may look ugly but unrolling loops is required for pre-DX11 hardware.
    iterateRGB5A3();
    iterateRGB5A3();
    iterateRGB5A3();
    iterateRGB5A3();
}

uint16 unpack(uchar b)
{
    return (uint16)((uint4)(b >> 6),
                    (uint4)(b >> 4 & 3),
                    (uint4)(b >> 2 & 3),
                    (uint4)(b      & 3));
}

kernel void decodeCMPRBlock(global uchar *dst,
                      const global uchar *src, int width)
{
    int x = get_global_id(0) * 4, y = get_global_id(1) * 4;
    uchar8 val = vload8(0, src);

    uchar2 colora565 = (uchar2)(val.s1, val.s3);
    uchar2 colorb565 = (uchar2)(val.s0, val.s2);
    uchar8 color32 = (uchar8)(bitselect(colora565 << (uchar2)3, colora565 >> (uchar2)2, (uchar2)0xFFFFF000),
                              colora565 >> (uchar2)3 | bitselect(colorb565 << (uchar2)5, colorb565 >> (uchar2)1, (uchar2)0xFFFFFF00),
                              bitselect(colorb565, colorb565 >> (uchar2)5, (uchar2)0xFFFFF000),
                              (uchar2)0xFF);
    uint4 colors;
    uint4 colorNoAlpha;
	ushort4 frac2 = (ushort4)(color32.even & (uchar4)0xFF) - (ushort4)(color32.odd & (uchar4)0xFF);
	uchar4 frac = convert_uchar4((frac2 * (ushort4)3) / (ushort4)8);
    colorNoAlpha = convert_uint4(color32.odd + frac);
    colorNoAlpha = (colorNoAlpha << (uint4)8) | convert_uint4(color32.even - frac);
    colorNoAlpha = (colorNoAlpha << (uint4)8) | convert_uint4(color32.odd);
    colorNoAlpha = (colorNoAlpha << (uint4)8) | convert_uint4(color32.even);

    uint4 colorAlpha;
    uchar4 midpoint = rhadd(color32.odd, color32.even);
    midpoint.s3 = 0xFF;
    colorAlpha = convert_uint4(midpoint);    
    colorAlpha = (colorAlpha << (uint4)8) | convert_uint4(color32.odd);
    colorAlpha = (colorAlpha << (uint4)8) | convert_uint4(color32.even);

    uint4 choice = isgreater(upsample(val.s0,val.s1),upsample(val.s2, val.s3));
    colors = bitselect(colorNoAlpha, colorAlpha, choice);

    uint16 colorsFull = (uint16)(colors, colors, colors, colors);

    uint16 shifts = (((unpack(val.s7) << (uint16)8
                  |   unpack(val.s6)) << (uint16)8
                  |   unpack(val.s5)) << (uint16)8
                  |   unpack(val.s4)) << (uint16)3;

    vstore16(convert_uchar16(colorsFull >> (shifts & (uint16)0xFF)), 0, dst);
    shifts = shifts >> (uint16)8;
    vstore16(convert_uchar16(colorsFull >> (shifts & (uint16)0xFF)), 0, dst+=width*4);
    shifts = shifts >> (uint16)8;
    vstore16(convert_uchar16(colorsFull >> (shifts & (uint16)0xFF)), 0, dst+=width*4);
    shifts = shifts >> (uint16)8;
    vstore16(convert_uchar16(colorsFull >> (shifts & (uint16)0xFF)), 0, dst+=width*4);
}

kernel void DecodeCMPR(global uchar *dst,
                      const global uchar *src, int width)
{
    int x = get_global_id(0) * 8, y = get_global_id(1) * 8;
    
    src += x * 4 + (y * width) / 2;
    
    decodeCMPRBlock(dst + (y * width + x) * 4, src, width);
    src += 8;
    decodeCMPRBlock(dst + (y * width + x + 4) * 4, src, width); // + 16
    src += 8;
    decodeCMPRBlock(dst + ((y + 4) * width + x) * 4, src, width); // + 16*width
    src += 8;
    decodeCMPRBlock(dst + ((y + 4) * width + x + 4) * 4, src, width); // + 16*(width+1)

}