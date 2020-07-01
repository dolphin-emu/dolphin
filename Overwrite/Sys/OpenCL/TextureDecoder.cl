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

kernel void DecodeI4_RGBA(global uint *dst,
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
        vstore8(upsample(upsample(res,res),upsample(res,res)), 0, dst + ((y + iy)*width + x));
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
        vstore8(vload8(srcOffset++, src),
                0, dst + ((y + iy)*width + x));
    }
}

kernel void DecodeI8_RGBA(global uint *dst,
                          const global uchar *src, int width)
{
    int x = get_global_id(0) * 8, y = get_global_id(1) * 4;
    int srcOffset = ((x * 4) + (y * width)) / 8;
    for (int iy = 0; iy < 4; iy++)
    {
        uchar8 val = vload8(srcOffset++, src);
        vstore8(upsample(upsample(val,val),upsample(val,val)),
                0, dst + ((y + iy)*width + x));
    }
}

kernel void DecodeIA8(global ushort *dst,
                      const global uchar *src, int width)
{
    int x = get_global_id(0) * 4, y = get_global_id(1) * 4;
    int srcOffset = ((x * 4) + (y * width)) / 4;
    for (int iy = 0; iy < 4; iy++)
    {
        uchar8 val = vload8(srcOffset++, src);
        vstore4(upsample(val.even, val.odd), 0, dst + ((y + iy)*width + x));
    }
}

kernel void DecodeIA8_RGBA(global uint *dst,
                           const global uchar *src, int width)
{
    int x = get_global_id(0) * 4, y = get_global_id(1) * 4;
    int srcOffset = ((x * 4) + (y * width)) / 4;
    for (int iy = 0; iy < 4; iy++)
    {
        uchar8 val = vload8(srcOffset++, src);
        vstore4(upsample(upsample(val.even,val.odd),upsample(val.odd, val.odd)), 0, dst + ((y + iy)*width + x));
    }
}

kernel void DecodeIA4(global ushort *dst,
                      const global uchar *src, int width)
{
    int x = get_global_id(0) * 8, y = get_global_id(1) * 4;
    int srcOffset = ((x * 4) + (y * width)) / 8;
    uchar8 val;
    ushort8 res;
    for (int iy = 0; iy < 4; iy++)
    {
        val = vload8(srcOffset++, src);
        res = upsample(val >> (uchar8)4, val & (uchar8)0xF);
        res |= res << (ushort8)4; 
        vstore8(res, 0, dst + y*width + x);
        dst+=width;
    }
}

kernel void DecodeIA4_RGBA(global uint *dst,
                           const global uchar *src, int width)
{
    int x = get_global_id(0) * 8, y = get_global_id(1) * 4;
    int srcOffset = ((x * 4) + (y * width)) / 8;
    uchar8 val;
    uint8 res;
    for (int iy = 0; iy < 4; iy++)
    {
        val = vload8(srcOffset++, src);
        uchar8 a = val >> (uchar8)4;
        uchar8 l = val & (uchar8)0xF;
        res = upsample(upsample(a, l), upsample(l,l));
        res |= res << (uint8)4;
        vstore8(res, 0, dst + y*width + x);
        dst+=width;
    }
}

kernel void DecodeRGBA8(global ushort *dst,
                      const global ushort *src, int width)
{
    int x = get_global_id(0) * 4, y = get_global_id(1) * 4;
    int srcOffset = (x * 2) + (y * width) / 2;
    for (int iy = 0; iy < 4; iy++)
    {
        ushort8 val = (ushort8)(vload4(srcOffset, src), vload4(srcOffset + 4, src));
        ushort8 temp = rotate(val, (ushort8)4);
        ushort8 bgra = rotate(temp, (ushort8)4).s40516273;
        vstore8(bgra, 0, dst + ((y + iy)*width + x) * 2);
        srcOffset++;
    }
}

kernel void DecodeRGBA8_RGBA(global uchar *dst,
                             const global uchar *src, int width)
{
    int x = get_global_id(0) * 4, y = get_global_id(1) * 4;
    int srcOffset = (x * 2) + (y * width) / 2;
    for (int iy = 0; iy < 4; iy++)
    {
        uchar8 ar = vload8(srcOffset, src);
        uchar8 gb = vload8(srcOffset + 4, src);
        uchar16 res;
        res.even.even = ar.odd;
        res.even.odd = gb.odd;
        res.odd.even = gb.even;
        res.odd.odd = ar.even;
        vstore16(res, 0, dst + ((y + iy)*width + x) * 4);
        srcOffset++;
    }
}

kernel void DecodeRGB565(global ushort *dst,
                         const global ushort *src, int width)
{
    int x = get_global_id(0) * 4, y = get_global_id(1) * 4;
    int srcOffset = x + (y * width) / 4;
    dst += width*y + x;
    for (int iy = 0; iy < 4; iy++)
    {
        ushort4 val = rotate(vload4(srcOffset++, src),(ushort4)4);
        vstore4(rotate(val,(ushort4)4), 0, dst + iy*width);
    }
}

kernel void DecodeRGB565_RGBA(global uchar *dst,
                              const global uchar *src, int width)
{
    int x = get_global_id(0) * 4, y = get_global_id(1) * 4;
    int srcOffset = x + (y * width) / 4;
    for (int iy = 0; iy < 4; iy++)
    {
        uchar8 val = vload8(srcOffset++, src);

        uchar16 res;
        res.even.even = bitselect(val.even, val.even >> (uchar4)5, (uchar4)7);
        res.odd.even = bitselect((val.odd >> (uchar4)3) | (val.even << (uchar4)5), val.even >> (uchar4)1, (uchar4)3);
        res.even.odd = bitselect(val.odd << (uchar4)3, val.odd >> (uchar4)2, (uchar4)7);
        res.odd.odd = (uchar4)0xFF;
     
        vstore16(res, 0, dst + ((y + iy)*width + x) * 4);
    }
}

kernel void DecodeRGB5A3(global uchar *dst,
                         const global uchar *src, int width)
{
    int x = get_global_id(0) * 4, y = get_global_id(1) * 4;
    int srcOffset = x + (y * width) / 4;
    uchar8 val;
    uchar16 resNoAlpha, resAlpha, choice;
    #define iterateRGB5A3() \
    val = vload8(srcOffset++, src); \
    resNoAlpha.s26AE = val.even << (uchar4)1; \
    resNoAlpha.s159D = val.even << (uchar4)6 | val.odd >> (uchar4)2; \
    resNoAlpha.s048C = val.odd << (uchar4)3; \
    resNoAlpha = bitselect(resNoAlpha, resNoAlpha >> (uchar16)5, (uchar16)0x3); \
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

kernel void DecodeRGB5A3_RGBA(global uchar *dst,
                              const global uchar *src, int width)
{
    int x = get_global_id(0) * 4, y = get_global_id(1) * 4;
    int srcOffset = x + (y * width) / 4;
    uchar8 val;
    uchar16 resNoAlpha, resAlpha, choice;
    #define iterateRGB5A3_RGBA() \
    val = vload8(srcOffset++, src); \
    resNoAlpha.s048C = val.even << (uchar4)1; \
    resNoAlpha.s159D = val.even << (uchar4)6 | val.odd >> (uchar4)2; \
    resNoAlpha.s26AE = val.odd << (uchar4)3; \
    resNoAlpha = bitselect(resNoAlpha, resNoAlpha >> (uchar16)5, (uchar16)0x3); \
    resNoAlpha.s37BF = (uchar4)(0xFF); \
    resAlpha.s048C = bitselect(val.even << (uchar4)4, val.even, (uchar4)0xF); \
    resAlpha.s159D = bitselect(val.odd, val.odd >> (uchar4)4,   (uchar4)0xF); \
    resAlpha.s26AE = bitselect(val.odd << (uchar4)4, val.odd,   (uchar4)0xF); \
    resAlpha.s37BF = bitselect(val.even << (uchar4)1, val.even >> (uchar4)2, (uchar4)0x1C); \
    resAlpha.s37BF = bitselect(resAlpha.s37BF, val.even >> (uchar4)5, (uchar4)0x3); \
    choice = (uchar16)((uchar4)(val.even.s0), \
                       (uchar4)(val.even.s1), \
                       (uchar4)(val.even.s2), \
                       (uchar4)(val.even.s3)); \
    vstore16(select(resAlpha, resNoAlpha, choice), 0, dst + (y * width + x) * 4);
    iterateRGB5A3_RGBA(); dst += width*4;
    iterateRGB5A3_RGBA(); dst += width*4;
    iterateRGB5A3_RGBA(); dst += width*4;
    iterateRGB5A3_RGBA();
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
    uchar8 color32 = (uchar8)(bitselect(colora565 << (uchar2)3, colora565 >> (uchar2)2, (uchar2)7),
                              bitselect((colora565 >> (uchar2)3) | (colorb565 << (uchar2)5), colorb565 >> (uchar2)1, (uchar2)3),
                              bitselect(colorb565, colorb565 >> (uchar2)5, (uchar2)7),
                              (uchar2)0xFF);

    ushort4 frac2 = convert_ushort4(color32.even) - convert_ushort4(color32.odd);
    uchar4 frac = convert_uchar4((frac2 * (ushort4)3) / (ushort4)8);
    
    ushort4 colorAlpha =   upsample((uchar4)(color32.even.s0,color32.even.s1,color32.even.s2,0),
                                    rhadd(color32.odd, color32.even));
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

kernel void decodeCMPRBlock_RGBA(global uchar *dst,
                                 const global uchar *src, int width)
{
    int x = get_global_id(0) * 4, y = get_global_id(1) * 4;
    uchar8 val = vload8(0, src);

    uchar2 colora565 = (uchar2)(val.s1, val.s3);
    uchar2 colorb565 = (uchar2)(val.s0, val.s2);
    uchar8 color32 = (uchar8)(bitselect(colorb565, colorb565 >> (uchar2)5, (uchar2)7),
                              bitselect((colora565 >> (uchar2)3) | (colorb565 << (uchar2)5), colorb565 >> (uchar2)1, (uchar2)3),
                              bitselect(colora565 << (uchar2)3, colora565 >> (uchar2)2, (uchar2)7),
                              (uchar2)0xFF);

    ushort4 frac2 = convert_ushort4(color32.even) - convert_ushort4(color32.odd);
    uchar4 frac = convert_uchar4((frac2 * (ushort4)3) / (ushort4)8);
    
    ushort4 colorAlpha =   upsample((uchar4)(color32.even.s0,color32.even.s1,color32.even.s2,0),
                                    rhadd(color32.odd, color32.even));
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

kernel void DecodeCMPR_RGBA(global uchar *dst,
                            const global uchar *src, int width)
{
    int x = get_global_id(0) * 8, y = get_global_id(1) * 8;
    
    src += x * 4 + (y * width) / 2;
    dst += (y * width + x) * 4;
    
    decodeCMPRBlock_RGBA(dst, src, width);              src += 8;
    decodeCMPRBlock_RGBA(dst + 16, src, width);         src += 8;
    decodeCMPRBlock_RGBA(dst + 16 * width, src, width); src += 8;
    decodeCMPRBlock_RGBA(dst + 16 * (width + 1), src, width);
}
