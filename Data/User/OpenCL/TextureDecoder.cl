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
        res.odd = (val >> (uchar8)4) & (uchar8)0x0F;
        res.even  = val & (uchar8)0x0F;
        res |= res << (uchar16)4;
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
        val = (val >> (ushort4)8) | (val << (ushort4)8);
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
        ushort4 vs = val.odd | (ushort4)(val.even << (ushort4)8);
        
        uchar16 resNoAlpha;
        resNoAlpha.s26AE = convert_uchar4(vs >> (ushort4)7); // R
        resNoAlpha.s159D = convert_uchar4(vs >> (ushort4)2); // G
        resNoAlpha.s048C = convert_uchar4(vs << (ushort4)3); // B
        resNoAlpha &= (uchar16)0xF8;        
        resNoAlpha |= (uchar16)(resNoAlpha >> (uchar16)5) & (uchar16)3; // 5 -> 8
        resNoAlpha.s37BF = (uchar4)(0xFF);
        
        uchar16 resAlpha;
        resAlpha.s26AE = convert_uchar4(vs >> (ushort4)8); // R
        resAlpha.s159D = convert_uchar4(vs >> (ushort4)4); // G
        resAlpha.s048C = convert_uchar4(vs);      // B
        resAlpha &= (uchar16)0x0F;
        resAlpha |= (resAlpha << (uchar16)4);
        resAlpha.s37BF = convert_uchar4(vs >> (ushort4)7) & (uchar4)0xE0;
        resAlpha.s37BF |= ((resAlpha.s37BF >> (uchar4)3) & (uchar4)0x1C)
                            | ((resAlpha.s37BF >> (uchar4)6) & (uchar4)0x3);
        uchar16 choice = (uchar16)((uchar4)(vs.s0 >> 8),
                                   (uchar4)(vs.s1 >> 8),
                                   (uchar4)(vs.s2 >> 8),
                                   (uchar4)(vs.s3 >> 8));
        uchar16 res;
        res = select(resAlpha, resNoAlpha, choice);
        vstore16(res, 0, dst + ((y + iy) * width + x) * 4);
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

kernel void decodeCMPRBlock(global uchar *dst,
                      const global uchar *src, int width)
{
    int x = get_global_id(0) * 4, y = get_global_id(1) * 4;
    uchar8 val = vload8(0, src);
    ushort2 color565 = (ushort2)((val.s1 & 0xFF) | (val.s0 << 8), (val.s3 & 0xFF) | (val.s2 << 8));
    uchar8 color32 = convert_uchar8((ushort8)
         (((color565 << (ushort2)3) & (ushort2)0xF8) | ((color565 >>  (ushort2)2) & (ushort2)0x7),
          ((color565 >> (ushort2)3) & (ushort2)0xFC) | ((color565 >>  (ushort2)9) & (ushort2)0x3),
          ((color565 >> (ushort2)8) & (ushort2)0xF8) | ((color565 >> (ushort2)13) & (ushort2)0x7),
            0xFF, 0xFF));
    uint4 colors;
    uint4 colorNoAlpha;
    uchar4 frac = convert_uchar4((((convert_ushort4(color32.even) & (ushort4)0xFF) - (convert_ushort4(color32.odd) & (ushort4)0xFF)) * (ushort4)3) / (ushort4)8);
    colorNoAlpha = convert_uint4(color32.odd + frac);
    colorNoAlpha = (colorNoAlpha << (uint4)8) | convert_uint4(color32.even - frac);
    colorNoAlpha = (colorNoAlpha << (uint4)8) | convert_uint4(color32.odd);
    colorNoAlpha = (colorNoAlpha << (uint4)8) | convert_uint4(color32.even);

    uint4 colorAlpha;
    uchar4 midpoint = convert_uchar4((convert_ushort4(color32.odd) + convert_ushort4(color32.even) + (ushort4)1) / (ushort4)2);
    midpoint.s3 = 0xFF;
    colorAlpha = convert_uint4((uchar4)(0, 0, 0, 0));
    colorAlpha = (colorAlpha << (uint4)8) | convert_uint4(midpoint);    
    colorAlpha = (colorAlpha << (uint4)8) | convert_uint4(color32.odd);
    colorAlpha = (colorAlpha << (uint4)8) | convert_uint4(color32.even);

    colors = color565.s0 > color565.s1 ? colorNoAlpha : colorAlpha;

    uint16 colorsFull = (uint16)(colors, colors, colors, colors);

    uint4 shift0 = unpack2bits(val.s4);
    uint4 shift1 = unpack2bits(val.s5);
    uint4 shift2 = unpack2bits(val.s6);
    uint4 shift3 = unpack2bits(val.s7);
    uint16 shifts = (uint16)((uint4)(shift3.s0), (uint4)(shift3.s1), (uint4)(shift3.s2), (uint4)(shift3.s3));
    shifts = (shifts << (uint16)8) | (uint16)((uint4)(shift2.s0), (uint4)(shift2.s1), (uint4)(shift2.s2), (uint4)(shift2.s3));
    shifts = (shifts << (uint16)8) | (uint16)((uint4)(shift1.s0), (uint4)(shift1.s1), (uint4)(shift1.s2), (uint4)(shift1.s3));
    shifts = (shifts << (uint16)8) | (uint16)((uint4)(shift0.s0), (uint4)(shift0.s1), (uint4)(shift0.s2), (uint4)(shift0.s3)) << (uint16)3;
    
    for (int iy = 0; iy < 4; iy++)
    {
        uchar16 res;
        res = convert_uchar16(colorsFull >> (shifts & (uint16)0xFF)) >> (uchar16)8;
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