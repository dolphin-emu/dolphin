////////////////////////////////////////////////////////////
//
// SFML - Simple and Fast Multimedia Library
// Copyright (C) 2007-2016 Laurent Gomila (laurent@sfml-dev.org)
//
// This software is provided 'as-is', without any express or implied warranty.
// In no event will the authors be held liable for any damages arising from the use of this software.
//
// Permission is granted to anyone to use this software for any purpose,
// including commercial applications, and to alter it and redistribute it freely,
// subject to the following restrictions:
//
// 1. The origin of this software must not be misrepresented;
//    you must not claim that you wrote the original software.
//    If you use this software in a product, an acknowledgment
//    in the product documentation would be appreciated but is not required.
//
// 2. Altered source versions must be plainly marked as such,
//    and must not be misrepresented as being the original software.
//
// 3. This notice may not be removed or altered from any source distribution.
//
////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////
// Headers
////////////////////////////////////////////////////////////
#include <SFML/Graphics/Font.hpp>
#include <SFML/Graphics/GLCheck.hpp>
#ifdef SFML_SYSTEM_ANDROID
    #include <SFML/System/Android/ResourceStream.hpp>
#endif
#include <SFML/System/InputStream.hpp>
#include <SFML/System/Err.hpp>
#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_GLYPH_H
#include FT_OUTLINE_H
#include FT_BITMAP_H
#include FT_STROKER_H
#include <cstdlib>
#include <cstring>


namespace
{
    // FreeType callbacks that operate on a sf::InputStream
    unsigned long read(FT_Stream rec, unsigned long offset, unsigned char* buffer, unsigned long count)
    {
        sf::InputStream* stream = static_cast<sf::InputStream*>(rec->descriptor.pointer);
        if (static_cast<unsigned long>(stream->seek(offset)) == offset)
        {
            if (count > 0)
                return static_cast<unsigned long>(stream->read(reinterpret_cast<char*>(buffer), count));
            else
                return 0;
        }
        else
            return count > 0 ? 0 : 1; // error code is 0 if we're reading, or nonzero if we're seeking
    }
    void close(FT_Stream)
    {
    }
}


namespace sf
{
////////////////////////////////////////////////////////////
Font::Font() :
m_library  (NULL),
m_face     (NULL),
m_streamRec(NULL),
m_stroker  (NULL),
m_refCount (NULL),
m_info     ()
{
    #ifdef SFML_SYSTEM_ANDROID
        m_stream = NULL;
    #endif
}


////////////////////////////////////////////////////////////
Font::Font(const Font& copy) :
m_library    (copy.m_library),
m_face       (copy.m_face),
m_streamRec  (copy.m_streamRec),
m_stroker    (copy.m_stroker),
m_refCount   (copy.m_refCount),
m_info       (copy.m_info),
m_pages      (copy.m_pages),
m_pixelBuffer(copy.m_pixelBuffer)
{
    #ifdef SFML_SYSTEM_ANDROID
        m_stream = NULL;
    #endif

    // Note: as FreeType doesn't provide functions for copying/cloning,
    // we must share all the FreeType pointers

    if (m_refCount)
        (*m_refCount)++;
}


////////////////////////////////////////////////////////////
Font::~Font()
{
    cleanup();

    #ifdef SFML_SYSTEM_ANDROID

    if (m_stream)
        delete (priv::ResourceStream*)m_stream;

    #endif
}


////////////////////////////////////////////////////////////
bool Font::loadFromFile(const std::string& filename)
{
    #ifndef SFML_SYSTEM_ANDROID

    // Cleanup the previous resources
    cleanup();
    m_refCount = new int(1);

    // Initialize FreeType
    // Note: we initialize FreeType for every font instance in order to avoid having a single
    // global manager that would create a lot of issues regarding creation and destruction order.
    FT_Library library;
    if (FT_Init_FreeType(&library) != 0)
    {
        err() << "Failed to load font \"" << filename << "\" (failed to initialize FreeType)" << std::endl;
        return false;
    }
    m_library = library;

    // Load the new font face from the specified file
    FT_Face face;
    if (FT_New_Face(static_cast<FT_Library>(m_library), filename.c_str(), 0, &face) != 0)
    {
        err() << "Failed to load font \"" << filename << "\" (failed to create the font face)" << std::endl;
        return false;
    }

    // Load the stroker that will be used to outline the font
    FT_Stroker stroker;
    if (FT_Stroker_New(static_cast<FT_Library>(m_library), &stroker) != 0)
    {
        err() << "Failed to load font \"" << filename << "\" (failed to create the stroker)" << std::endl;
        return false;
    }
    m_stroker = stroker;

    // Select the unicode character map
    if (FT_Select_Charmap(face, FT_ENCODING_UNICODE) != 0)
    {
        err() << "Failed to load font \"" << filename << "\" (failed to set the Unicode character set)" << std::endl;
        FT_Done_Face(face);
        return false;
    }

    // Store the loaded font in our ugly void* :)
    m_face = face;

    // Store the font information
    m_info.family = face->family_name ? face->family_name : std::string();

    return true;

    #else

    if (m_stream)
        delete (priv::ResourceStream*)m_stream;

    m_stream = new priv::ResourceStream(filename);
    return loadFromStream(*(priv::ResourceStream*)m_stream);

    #endif
}


////////////////////////////////////////////////////////////
bool Font::loadFromMemory(const void* data, std::size_t sizeInBytes)
{
    // Cleanup the previous resources
    cleanup();
    m_refCount = new int(1);

    // Initialize FreeType
    // Note: we initialize FreeType for every font instance in order to avoid having a single
    // global manager that would create a lot of issues regarding creation and destruction order.
    FT_Library library;
    if (FT_Init_FreeType(&library) != 0)
    {
        err() << "Failed to load font from memory (failed to initialize FreeType)" << std::endl;
        return false;
    }
    m_library = library;

    // Load the new font face from the specified file
    FT_Face face;
    if (FT_New_Memory_Face(static_cast<FT_Library>(m_library), reinterpret_cast<const FT_Byte*>(data), static_cast<FT_Long>(sizeInBytes), 0, &face) != 0)
    {
        err() << "Failed to load font from memory (failed to create the font face)" << std::endl;
        return false;
    }

    // Load the stroker that will be used to outline the font
    FT_Stroker stroker;
    if (FT_Stroker_New(static_cast<FT_Library>(m_library), &stroker) != 0)
    {
        err() << "Failed to load font from memory (failed to create the stroker)" << std::endl;
        return false;
    }
    m_stroker = stroker;

    // Select the Unicode character map
    if (FT_Select_Charmap(face, FT_ENCODING_UNICODE) != 0)
    {
        err() << "Failed to load font from memory (failed to set the Unicode character set)" << std::endl;
        FT_Done_Face(face);
        return false;
    }

    // Store the loaded font in our ugly void* :)
    m_face = face;

    // Store the font information
    m_info.family = face->family_name ? face->family_name : std::string();

    return true;
}


////////////////////////////////////////////////////////////
bool Font::loadFromStream(InputStream& stream)
{
    // Cleanup the previous resources
    cleanup();
    m_refCount = new int(1);

    // Initialize FreeType
    // Note: we initialize FreeType for every font instance in order to avoid having a single
    // global manager that would create a lot of issues regarding creation and destruction order.
    FT_Library library;
    if (FT_Init_FreeType(&library) != 0)
    {
        err() << "Failed to load font from stream (failed to initialize FreeType)" << std::endl;
        return false;
    }
    m_library = library;

    // Make sure that the stream's reading position is at the beginning
    stream.seek(0);

    // Prepare a wrapper for our stream, that we'll pass to FreeType callbacks
    FT_StreamRec* rec = new FT_StreamRec;
    std::memset(rec, 0, sizeof(*rec));
    rec->base               = NULL;
    rec->size               = static_cast<unsigned long>(stream.getSize());
    rec->pos                = 0;
    rec->descriptor.pointer = &stream;
    rec->read               = &read;
    rec->close              = &close;

    // Setup the FreeType callbacks that will read our stream
    FT_Open_Args args;
    args.flags  = FT_OPEN_STREAM;
    args.stream = rec;
    args.driver = 0;

    // Load the new font face from the specified stream
    FT_Face face;
    if (FT_Open_Face(static_cast<FT_Library>(m_library), &args, 0, &face) != 0)
    {
        err() << "Failed to load font from stream (failed to create the font face)" << std::endl;
        delete rec;
        return false;
    }

    // Load the stroker that will be used to outline the font
    FT_Stroker stroker;
    if (FT_Stroker_New(static_cast<FT_Library>(m_library), &stroker) != 0)
    {
        err() << "Failed to load font from stream (failed to create the stroker)" << std::endl;
        return false;
    }
    m_stroker = stroker;

    // Select the Unicode character map
    if (FT_Select_Charmap(face, FT_ENCODING_UNICODE) != 0)
    {
        err() << "Failed to load font from stream (failed to set the Unicode character set)" << std::endl;
        FT_Done_Face(face);
        delete rec;
        return false;
    }

    // Store the loaded font in our ugly void* :)
    m_face = face;
    m_streamRec = rec;

    // Store the font information
    m_info.family = face->family_name ? face->family_name : std::string();

    return true;
}


////////////////////////////////////////////////////////////
const Font::Info& Font::getInfo() const
{
    return m_info;
}


////////////////////////////////////////////////////////////
const Glyph& Font::getGlyph(Uint32 codePoint, unsigned int characterSize, bool bold, float outlineThickness) const
{
    // Get the page corresponding to the character size
    GlyphTable& glyphs = m_pages[characterSize].glyphs;

    // Build the key by combining the code point, bold flag, and outline thickness
    Uint64 key = (static_cast<Uint64>(*reinterpret_cast<Uint32*>(&outlineThickness)) << 32)
               | (static_cast<Uint64>(bold ? 1 : 0) << 31)
               |  static_cast<Uint64>(codePoint);

    // Search the glyph into the cache
    GlyphTable::const_iterator it = glyphs.find(key);
    if (it != glyphs.end())
    {
        // Found: just return it
        return it->second;
    }
    else
    {
        // Not found: we have to load it
        Glyph glyph = loadGlyph(codePoint, characterSize, bold, outlineThickness);
        return glyphs.insert(std::make_pair(key, glyph)).first->second;
    }
}


////////////////////////////////////////////////////////////
float Font::getKerning(Uint32 first, Uint32 second, unsigned int characterSize) const
{
    // Special case where first or second is 0 (null character)
    if (first == 0 || second == 0)
        return 0.f;

    FT_Face face = static_cast<FT_Face>(m_face);

    if (face && FT_HAS_KERNING(face) && setCurrentSize(characterSize))
    {
        // Convert the characters to indices
        FT_UInt index1 = FT_Get_Char_Index(face, first);
        FT_UInt index2 = FT_Get_Char_Index(face, second);

        // Get the kerning vector
        FT_Vector kerning;
        FT_Get_Kerning(face, index1, index2, FT_KERNING_DEFAULT, &kerning);

        // X advance is already in pixels for bitmap fonts
        if (!FT_IS_SCALABLE(face))
            return static_cast<float>(kerning.x);

        // Return the X advance
        return static_cast<float>(kerning.x) / static_cast<float>(1 << 6);
    }
    else
    {
        // Invalid font, or no kerning
        return 0.f;
    }
}


////////////////////////////////////////////////////////////
float Font::getLineSpacing(unsigned int characterSize) const
{
    FT_Face face = static_cast<FT_Face>(m_face);

    if (face && setCurrentSize(characterSize))
    {
        return static_cast<float>(face->size->metrics.height) / static_cast<float>(1 << 6);
    }
    else
    {
        return 0.f;
    }
}


////////////////////////////////////////////////////////////
float Font::getUnderlinePosition(unsigned int characterSize) const
{
    FT_Face face = static_cast<FT_Face>(m_face);

    if (face && setCurrentSize(characterSize))
    {
        // Return a fixed position if font is a bitmap font
        if (!FT_IS_SCALABLE(face))
            return characterSize / 10.f;

        return -static_cast<float>(FT_MulFix(face->underline_position, face->size->metrics.y_scale)) / static_cast<float>(1 << 6);
    }
    else
    {
        return 0.f;
    }
}


////////////////////////////////////////////////////////////
float Font::getUnderlineThickness(unsigned int characterSize) const
{
    FT_Face face = static_cast<FT_Face>(m_face);

    if (face && setCurrentSize(characterSize))
    {
        // Return a fixed thickness if font is a bitmap font
        if (!FT_IS_SCALABLE(face))
            return characterSize / 14.f;

        return static_cast<float>(FT_MulFix(face->underline_thickness, face->size->metrics.y_scale)) / static_cast<float>(1 << 6);
    }
    else
    {
        return 0.f;
    }
}


////////////////////////////////////////////////////////////
const Texture& Font::getTexture(unsigned int characterSize) const
{
    return m_pages[characterSize].texture;
}


////////////////////////////////////////////////////////////
Font& Font::operator =(const Font& right)
{
    Font temp(right);

    std::swap(m_library,     temp.m_library);
    std::swap(m_face,        temp.m_face);
    std::swap(m_streamRec,   temp.m_streamRec);
    std::swap(m_stroker,     temp.m_stroker);
    std::swap(m_refCount,    temp.m_refCount);
    std::swap(m_info,        temp.m_info);
    std::swap(m_pages,       temp.m_pages);
    std::swap(m_pixelBuffer, temp.m_pixelBuffer);

    #ifdef SFML_SYSTEM_ANDROID
        std::swap(m_stream, temp.m_stream);
    #endif

    return *this;
}


////////////////////////////////////////////////////////////
void Font::cleanup()
{
    // Check if we must destroy the FreeType pointers
    if (m_refCount)
    {
        // Decrease the reference counter
        (*m_refCount)--;

        // Free the resources only if we are the last owner
        if (*m_refCount == 0)
        {
            // Delete the reference counter
            delete m_refCount;

            // Destroy the stroker
            if (m_stroker)
                FT_Stroker_Done(static_cast<FT_Stroker>(m_stroker));

            // Destroy the font face
            if (m_face)
                FT_Done_Face(static_cast<FT_Face>(m_face));

            // Destroy the stream rec instance, if any (must be done after FT_Done_Face!)
            if (m_streamRec)
                delete static_cast<FT_StreamRec*>(m_streamRec);

            // Close the library
            if (m_library)
                FT_Done_FreeType(static_cast<FT_Library>(m_library));
        }
    }

    // Reset members
    m_library   = NULL;
    m_face      = NULL;
    m_stroker   = NULL;
    m_streamRec = NULL;
    m_refCount  = NULL;
    m_pages.clear();
    m_pixelBuffer.clear();
}


////////////////////////////////////////////////////////////
Glyph Font::loadGlyph(Uint32 codePoint, unsigned int characterSize, bool bold, float outlineThickness) const
{
    // The glyph to return
    Glyph glyph;

    // First, transform our ugly void* to a FT_Face
    FT_Face face = static_cast<FT_Face>(m_face);
    if (!face)
        return glyph;

    // Set the character size
    if (!setCurrentSize(characterSize))
        return glyph;

    // Load the glyph corresponding to the code point
    FT_Int32 flags = FT_LOAD_TARGET_NORMAL | FT_LOAD_FORCE_AUTOHINT;
    if (outlineThickness != 0)
        flags |= FT_LOAD_NO_BITMAP;
    if (FT_Load_Char(face, codePoint, flags) != 0)
        return glyph;

    // Retrieve the glyph
    FT_Glyph glyphDesc;
    if (FT_Get_Glyph(face->glyph, &glyphDesc) != 0)
        return glyph;

    // Apply bold and outline (there is no fallback for outline) if necessary -- first technique using outline (highest quality)
    FT_Pos weight = 1 << 6;
    bool outline = (glyphDesc->format == FT_GLYPH_FORMAT_OUTLINE);
    if (outline)
    {
        if (bold)
        {
            FT_OutlineGlyph outlineGlyph = (FT_OutlineGlyph)glyphDesc;
            FT_Outline_Embolden(&outlineGlyph->outline, weight);
        }

        if (outlineThickness != 0)
        {
            FT_Stroker stroker = static_cast<FT_Stroker>(m_stroker);

            FT_Stroker_Set(stroker, static_cast<FT_Fixed>(outlineThickness * static_cast<float>(1 << 6)), FT_STROKER_LINECAP_ROUND, FT_STROKER_LINEJOIN_ROUND, 0);
            FT_Glyph_Stroke(&glyphDesc, stroker, false);
        }
    }

    // Convert the glyph to a bitmap (i.e. rasterize it)
    FT_Glyph_To_Bitmap(&glyphDesc, FT_RENDER_MODE_NORMAL, 0, 1);
    FT_Bitmap& bitmap = reinterpret_cast<FT_BitmapGlyph>(glyphDesc)->bitmap;

    // Apply bold if necessary -- fallback technique using bitmap (lower quality)
    if (!outline)
    {
        if (bold)
            FT_Bitmap_Embolden(static_cast<FT_Library>(m_library), &bitmap, weight, weight);

        if (outlineThickness != 0)
            err() << "Failed to outline glyph (no fallback available)" << std::endl;
    }

    // Compute the glyph's advance offset
    glyph.advance = static_cast<float>(face->glyph->metrics.horiAdvance) / static_cast<float>(1 << 6);
    if (bold)
        glyph.advance += static_cast<float>(weight) / static_cast<float>(1 << 6);

    int width  = bitmap.width;
    int height = bitmap.rows;

    if ((width > 0) && (height > 0))
    {
        // Leave a small padding around characters, so that filtering doesn't
        // pollute them with pixels from neighbors
        const unsigned int padding = 1;

        // Get the glyphs page corresponding to the character size
        Page& page = m_pages[characterSize];

        // Find a good position for the new glyph into the texture
        glyph.textureRect = findGlyphRect(page, width + 2 * padding, height + 2 * padding);

        // Make sure the texture data is positioned in the center
        // of the allocated texture rectangle
        glyph.textureRect.left += padding;
        glyph.textureRect.top += padding;
        glyph.textureRect.width -= 2 * padding;
        glyph.textureRect.height -= 2 * padding;

        // Compute the glyph's bounding box
        glyph.bounds.left   =  static_cast<float>(face->glyph->metrics.horiBearingX) / static_cast<float>(1 << 6);
        glyph.bounds.top    = -static_cast<float>(face->glyph->metrics.horiBearingY) / static_cast<float>(1 << 6);
        glyph.bounds.width  =  static_cast<float>(face->glyph->metrics.width)        / static_cast<float>(1 << 6) + outlineThickness * 2;
        glyph.bounds.height =  static_cast<float>(face->glyph->metrics.height)       / static_cast<float>(1 << 6) + outlineThickness * 2;

        // Extract the glyph's pixels from the bitmap
        m_pixelBuffer.resize(width * height * 4, 255);
        const Uint8* pixels = bitmap.buffer;
        if (bitmap.pixel_mode == FT_PIXEL_MODE_MONO)
        {
            // Pixels are 1 bit monochrome values
            for (int y = 0; y < height; ++y)
            {
                for (int x = 0; x < width; ++x)
                {
                    // The color channels remain white, just fill the alpha channel
                    std::size_t index = (x + y * width) * 4 + 3;
                    m_pixelBuffer[index] = ((pixels[x / 8]) & (1 << (7 - (x % 8)))) ? 255 : 0;
                }
                pixels += bitmap.pitch;
            }
        }
        else
        {
            // Pixels are 8 bits gray levels
            for (int y = 0; y < height; ++y)
            {
                for (int x = 0; x < width; ++x)
                {
                    // The color channels remain white, just fill the alpha channel
                    std::size_t index = (x + y * width) * 4 + 3;
                    m_pixelBuffer[index] = pixels[x];
                }
                pixels += bitmap.pitch;
            }
        }

        // Write the pixels to the texture
        unsigned int x = glyph.textureRect.left;
        unsigned int y = glyph.textureRect.top;
        unsigned int w = glyph.textureRect.width;
        unsigned int h = glyph.textureRect.height;
        page.texture.update(&m_pixelBuffer[0], w, h, x, y);
    }

    // Delete the FT glyph
    FT_Done_Glyph(glyphDesc);

    // Force an OpenGL flush, so that the font's texture will appear updated
    // in all contexts immediately (solves problems in multi-threaded apps)
    glCheck(glFlush());

    // Done :)
    return glyph;
}


////////////////////////////////////////////////////////////
IntRect Font::findGlyphRect(Page& page, unsigned int width, unsigned int height) const
{
    // Find the line that fits well the glyph
    Row* row = NULL;
    float bestRatio = 0;
    for (std::vector<Row>::iterator it = page.rows.begin(); it != page.rows.end() && !row; ++it)
    {
        float ratio = static_cast<float>(height) / it->height;

        // Ignore rows that are either too small or too high
        if ((ratio < 0.7f) || (ratio > 1.f))
            continue;

        // Check if there's enough horizontal space left in the row
        if (width > page.texture.getSize().x - it->width)
            continue;

        // Make sure that this new row is the best found so far
        if (ratio < bestRatio)
            continue;

        // The current row passed all the tests: we can select it
        row = &*it;
        bestRatio = ratio;
    }

    // If we didn't find a matching row, create a new one (10% taller than the glyph)
    if (!row)
    {
        int rowHeight = height + height / 10;
        while ((page.nextRow + rowHeight >= page.texture.getSize().y) || (width >= page.texture.getSize().x))
        {
            // Not enough space: resize the texture if possible
            unsigned int textureWidth  = page.texture.getSize().x;
            unsigned int textureHeight = page.texture.getSize().y;
            if ((textureWidth * 2 <= Texture::getMaximumSize()) && (textureHeight * 2 <= Texture::getMaximumSize()))
            {
                // Make the texture 2 times bigger
                Image newImage;
                newImage.create(textureWidth * 2, textureHeight * 2, Color(255, 255, 255, 0));
                newImage.copy(page.texture.copyToImage(), 0, 0);
                page.texture.loadFromImage(newImage);
            }
            else
            {
                // Oops, we've reached the maximum texture size...
                err() << "Failed to add a new character to the font: the maximum texture size has been reached" << std::endl;
                return IntRect(0, 0, 2, 2);
            }
        }

        // We can now create the new row
        page.rows.push_back(Row(page.nextRow, rowHeight));
        page.nextRow += rowHeight;
        row = &page.rows.back();
    }

    // Find the glyph's rectangle on the selected row
    IntRect rect(row->width, row->top, width, height);

    // Update the row informations
    row->width += width;

    return rect;
}


////////////////////////////////////////////////////////////
bool Font::setCurrentSize(unsigned int characterSize) const
{
    // FT_Set_Pixel_Sizes is an expensive function, so we must call it
    // only when necessary to avoid killing performances

    FT_Face face = static_cast<FT_Face>(m_face);
    FT_UShort currentSize = face->size->metrics.x_ppem;

    if (currentSize != characterSize)
    {
        FT_Error result = FT_Set_Pixel_Sizes(face, 0, characterSize);

        if (result == FT_Err_Invalid_Pixel_Size)
        {
            // In the case of bitmap fonts, resizing can
            // fail if the requested size is not available
            if (!FT_IS_SCALABLE(face))
            {
                err() << "Failed to set bitmap font size to " << characterSize << std::endl;
                err() << "Available sizes are: ";
                for (int i = 0; i < face->num_fixed_sizes; ++i)
                    err() << face->available_sizes[i].height << " ";
                err() << std::endl;
            }
        }

        return result == FT_Err_Ok;
    }
    else
    {
        return true;
    }
}


////////////////////////////////////////////////////////////
Font::Page::Page() :
nextRow(3)
{
    // Make sure that the texture is initialized by default
    sf::Image image;
    image.create(128, 128, Color(255, 255, 255, 0));

    // Reserve a 2x2 white square for texturing underlines
    for (int x = 0; x < 2; ++x)
        for (int y = 0; y < 2; ++y)
            image.setPixel(x, y, Color(255, 255, 255, 255));

    // Create the texture
    texture.loadFromImage(image);
    texture.setSmooth(true);
}

} // namespace sf
