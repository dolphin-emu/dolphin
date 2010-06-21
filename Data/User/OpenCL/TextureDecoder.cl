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
    uchar8 val;
    uchar16 res;
    dst += 2*(y*width + x);
    #define iterateIA4() \
    val = vload8(srcOffset++, src); \
    res.odd = (val >> (uchar8)4); \
    res.even  = val & (uchar8)0x0F; \
    res |= res << (uchar16)4; \
    vstore16(res, 0, dst);
    iterateIA4(); dst += 2*width;
    iterateIA4(); dst += 2*width;
    iterateIA4(); dst += 2*width;
    iterateIA4();
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
    #define iterateRGB5A3() \
    val = vload8(srcOffset++, src); \
    resNoAlpha.s26AE = val.even << (uchar4)1; \
    resNoAlpha.s159D = val.even << (uchar4)6 | val.odd >> (uchar4)2; \
    resNoAlpha.s048C = val.odd << (uchar4)3; \
    resNoAlpha.s37BF = (uchar4)(0xFF); \
    resAlpha.s26AE = bitselect(val.even << (uchar4)4, val.even, (uchar4)0xF); \
    resAlpha.s159D = bitselect(val.odd, val.odd >> (uchar4)4,   (uchar4)0xF); \
    resAlpha.s048C = bitselect(val.odd << (uchar4)4, val.odd,   (uchar4)0xF); \
    resAlpha.s37BF = bitselect(val.even << (uchar4)1, val.even >> (uchar4)2, (uchar4)0x1C); \
    resAlpha.s37BF = bitselect(resAlpha.s37BF, val.even >> (uchar4)5, (uchar4)0x3); \
    choice = (uchar16)((uchar4)(val.even.s0), \
                       (uchar4)(val.even.s1), \
                       (uchar4)(val.even.s2), \
                       (uchar4)(val.even.s3)); \
    vstore16(select(resAlpha, resNoAlpha, choice), 0, dst + (y * width + x) * 4);
    iterateRGB5A3(); dst += width*4;
    iterateRGB5A3(); dst += width*4;
    iterateRGB5A3(); dst += width*4;
    iterateRGB5A3();
}

uint16 unpack(uchar b)
{
    return (uint16)((uint4)(b >> 3 & 0x18),
                    (uint4)(b >> 1 & 0x18),
                    (uint4)(b << 1 & 0x18),
                    (uint4)(b << 3 & 0x18));
}

kernel void decodeCMPRBlock(global uchar *dst,
                      const global uchar *src, int width)
{
    int x = get_global_id(0) * 4, y = get_global_id(1) * 4;
    uchar8 val = vload8(0, src);

    uchar2 colora565 = (uchar2)(val.s1, val.s3);
    uchar2 colorb565 = (uchar2)(val.s0, val.s2);
    uchar8 color32 = (uchar8)(colora565 << (uchar2)3,
                              colora565 >> (uchar2)3 | colorb565 << (uchar2)5,
                              colorb565,
                              (uchar2)0xFF);

    ushort4 frac2 = convert_ushort4(color32.even & (uchar4)0xFF) - convert_ushort4(color32.odd & (uchar4)0xFF);
    uchar4 frac = convert_uchar4((frac2 * (ushort4)3) / (ushort4)8);
    
    ushort4 colorAlpha =   upsample((uchar4)0, rhadd(color32.odd, color32.even));
    colorAlpha.s3 = 0xFF;
    ushort4 colorNoAlpha = upsample(color32.odd + frac, color32.even - frac);

    uint4 colors = upsample((upsample(val.s0,val.s1) > upsample(val.s2,val.s3))?colorNoAlpha:colorAlpha,
                            upsample(color32.odd, color32.even));

    uint16 colorsFull = (uint16)(colors, colors, colors, colors);

    vstore16(convert_uchar16(colorsFull >> unpack(val.s4)), 0, dst);
    vstore16(convert_uchar16(colorsFull >> unpack(val.s5)), 0, dst+=width*4);
    vstore16(convert_uchar16(colorsFull >> unpack(val.s6)), 0, dst+=width*4);
    vstore16(convert_uchar16(colorsFull >> unpack(val.s7)), 0, dst+=width*4);
}

kernel void DecodeCMPR(global uchar *dst,
                      const global uchar *src, int width)
{
    int x = get_global_id(0) * 8, y = get_global_id(1) * 8;
    
    src += x * 4 + (y * width) / 2;
    dst += (y * width + x) * 4;
    
    decodeCMPRBlock(dst, src, width);              src += 8;
    decodeCMPRBlock(dst + 16, src, width);         src += 8;
    decodeCMPRBlock(dst + 16 * width, src, width); src += 8;
    decodeCMPRBlock(dst + 16 * (width + 1), src, width);
}