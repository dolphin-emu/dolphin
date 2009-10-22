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
        res.even = (val >> 4) & 0x0F;
        res.odd = val & 0x0F;
        res |= res << 4;
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
        uchar8 val = vload8(srcOffset, src);
        uchar8 res;
        res.odd = val.even;
        res.even = val.odd;
        vstore8(res, 0, dst + ((y + iy)*width + x) * 2);
        srcOffset++;
    }
}

kernel void DecodeIA4(global uchar *dst,
                     const global uchar *src, int width)
{
    int x = get_global_id(0) * 8, y = get_global_id(1) * 4;
    int srcOffset = ((x * 4) + (y * width)) / 8;
    for (int iy = 0; iy < 4; iy++)
    {
        uchar8 val = vload8(srcOffset, src);
        uchar16 res;
        res.odd = (val >> 4) & 0x0F;
        res.even  = val & 0x0F;
        res |= res << 4;
        vstore16(res, 0, dst + ((y + iy)*width + x) * 2);
        srcOffset++;
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
                         const global ushort *src, int width)
{
    int x = get_global_id(0) * 4, y = get_global_id(1) * 4;
    int srcOffset = x + (y * width) / 4;
    for (int iy = 0; iy < 4; iy++)
    {
        ushort4 val = vload4(srcOffset, src);
        val = (val >> 8) | (val << 8);
        vstore4(val, 0, dst + ((y + iy)*width + x));
        srcOffset++;
    }
}

kernel void DecodeRGB5A3(global uchar *dst,
                         const global uchar *src, int width)
{
    int x = get_global_id(0) * 4, y = get_global_id(1) * 4;
    int srcOffset = x + (y * width) / 4;
    for (int iy = 0; iy < 4; iy++)
    {
        ushort8 val = convert_ushort8(vload8(srcOffset, src));
        ushort4 vs = val.odd | (val.even << 8);
        
        uchar16 resNoAlpha;
        resNoAlpha.s26AE = convert_uchar4(vs >> 7); // R
        resNoAlpha.s159D = convert_uchar4(vs >> 2); // G
        resNoAlpha.s048C = convert_uchar4(vs << 3); // B
        resNoAlpha &= 0xF8;        
        resNoAlpha |= (resNoAlpha >> 5) & 3; // 5 -> 8
        resNoAlpha.s37BF = (uchar4)(0xFF);
        
        uchar16 resAlpha;
        resAlpha.s26AE = convert_uchar4(vs >> 8); // R
        resAlpha.s159D = convert_uchar4(vs >> 4); // G
        resAlpha.s048C = convert_uchar4(vs);      // B
        resAlpha &= 0x0F;
        resAlpha |= (resAlpha << 4);
        resAlpha.s37BF = convert_uchar4(vs >> 7) & 0xE0;
        resAlpha.s37BF |= ((resAlpha.s37BF >> 3) & 0x1C)
                            | ((resAlpha.s37BF >> 6) & 0x3);
        uchar16 choice = (uchar16)((uchar4)(vs.s0 >> 8),
                                   (uchar4)(vs.s1 >> 8),
                                   (uchar4)(vs.s2 >> 8),
                                   (uchar4)(vs.s3 >> 8));
        uchar16 res;
        res = select(resAlpha, resNoAlpha, choice);
        vstore16(res, 0, dst + ((y + iy)*width + x) * 4);
        srcOffset++;
    }
}

uint4 unpack2bits(uchar b)
{
    return (uint4)(b >> 6,
                  (b >> 4) & 3,
                  (b >> 2) & 3,
                  b & 3);
}       

/*
Lots of debug code there that I'm using to find the problems with CMPR decoding
I think blocks having no alpha are properly decoded, only the blocks with alpha
are problematic. This is WIP !
*/
kernel void decodeCMPRBlock(global uchar *dst,
                      const global uchar *src, int width)
{
    int x = get_global_id(0) * 4, y = get_global_id(1) * 4;
    int srcOffset = (x * 2 + y * width / 2) / 8; //x / 4 + y * width / 16; //(x * 4) + (y * width) / 16;
    uchar8 val = vload8(0, src);
    ushort2 color565 = (ushort2)((val.s1 & 0xFF) | (val.s0 << 8), (val.s3 & 0xFF) | (val.s2 << 8));
    uchar8 color32 = convert_uchar8((ushort8)
         (((color565 << 3) & 0xF8) | ((color565 >>  2) & 0x7),
          ((color565 >> 3) & 0xFC) | ((color565 >>  9) & 0x3),
          ((color565 >> 8) & 0xF8) | ((color565 >> 13) & 0x7),
            0xFF, 0xFF));
    uint4 colors;
    //uint4 choice = (uint4)((color565.s0 - color565.s1) << 16);
    uint4 colorNoAlpha;
    //uchar4 frac = (color32.odd - color32.even) / 2;
    //frac = frac - (frac / 4);
    uchar4 frac = convert_uchar4((((convert_ushort4(color32.even) & 0xFF) - (convert_ushort4(color32.odd) & 0xFF)) * 3) / 8);
    colorNoAlpha = convert_uint4(color32.odd + frac);
    colorNoAlpha = (colorNoAlpha << 8) | convert_uint4(color32.even - frac);
    colorNoAlpha = (colorNoAlpha << 8) | convert_uint4(color32.odd);
    colorNoAlpha = (colorNoAlpha << 8) | convert_uint4(color32.even);

    uint4 colorAlpha;
    //uchar4 midpoint = rhadd(color32.odd, color32.even);
    uchar4 midpoint = convert_uchar4((convert_ushort4(color32.odd) + convert_ushort4(color32.even) + 1) / 2);
    midpoint.s3 = 0xFF;
    colorAlpha = convert_uint4((uchar4)(0, 0, 0, 0));
    colorAlpha = (colorAlpha << 8) | convert_uint4(midpoint);    
    colorAlpha = (colorAlpha << 8) | convert_uint4(color32.odd);
    colorAlpha = (colorAlpha << 8) | convert_uint4(color32.even);

    //colorNoAlpha = (uint4)(0xFFFFFFFF);
    //colorAlpha = (uint4)(0, 0, 0, 0xFFFFFFFF);

    //colors  = select(colorAlpha, colorNoAlpha, choice);
    colors = color565.s0 > color565.s1 ? colorNoAlpha : colorAlpha;

    uint16 colorsFull = (uint16)(colors, colors, colors, colors);

    uint4 shift0 = unpack2bits(val.s4);
    uint4 shift1 = unpack2bits(val.s5);
    uint4 shift2 = unpack2bits(val.s6);
    uint4 shift3 = unpack2bits(val.s7);
    uint16 shifts =          (uint16)((uint4)(shift3.s0), (uint4)(shift3.s1), (uint4)(shift3.s2), (uint4)(shift3.s3));
    shifts = (shifts << 8) | (uint16)((uint4)(shift2.s0), (uint4)(shift2.s1), (uint4)(shift2.s2), (uint4)(shift2.s3));
    shifts = (shifts << 8) | (uint16)((uint4)(shift1.s0), (uint4)(shift1.s1), (uint4)(shift1.s2), (uint4)(shift1.s3));
    shifts = (shifts << 8) | (uint16)((uint4)(shift0.s0), (uint4)(shift0.s1), (uint4)(shift0.s2), (uint4)(shift0.s3));
    shifts <<= 3;
    
    for (int iy = 0; iy < 4; iy++)
    {
        uchar16 res;
        res = convert_uchar16(colorsFull >> (shifts & 0xFF));
        shifts >>= 8;
        //uchar4 t = convert_uchar4((ushort4)(color565.s0 >> 8, color565.s0 & 0xFF, color565.s1 >> 8, color565.s1 & 0xFF));
        //res = (uchar16)(t, t, t, t);
        //res = (uchar16)(frac, color32.even - color32.odd, (color32.even - color32.odd) / 2, (color32.even - color32.odd) / 2 - ((color32.even - color32.odd) / 8));
        //res = (uchar16)(color32.even, color32.odd, frac, convert_uchar4(choice));
        //res = convert_uchar16((uint16)(colorNoAlpha >> 24, colorNoAlpha >> 16, colorNoAlpha >> 8, colorNoAlpha));
        //res = convert_uchar16((uint16)(colorAlpha >> 24, colorAlpha >> 16, colorAlpha >> 8, colorAlpha));
        //res = convert_uchar16((uint16)(colors >> 24, colors >> 16, colors >> 8, colors));
        //res = convert_uchar16(shifts & 0xFF);
        //res = convert_uchar16((uint16)(shift0, shift1, shift2, shift3));
        //res = (uchar16)(((x))) + (uchar16)(0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3);
        //res.lo = val; res.s8 = x >> 8; res.s9 = x; res.sA = (iy + y) >> 8; res.sB = y + iy; res.sC = width >> 8; res.sD = width; res.sE = srcOffset >> 8; res.sF = srcOffset;
        vstore16(res, 0, dst);
        dst += width * 4;
    }
}

kernel void DecodeCMPR(global uchar *dst,
                      const global uchar *src, int width)
{
    int x = get_global_id(0) * 8, y = get_global_id(1) * 8;
    
    src += x * 4 + (y * width) / 2;
    
    decodeCMPRBlock(dst + (y * width + x) * 4, src, width);
    src += 8;
    decodeCMPRBlock(dst + (y * width + x + 4) * 4, src, width);
    src += 8;
    decodeCMPRBlock(dst + ((y + 4) * width + x) * 4, src, width);
    src += 8;
    decodeCMPRBlock(dst + ((y + 4) * width + x + 4) * 4, src, width);

}