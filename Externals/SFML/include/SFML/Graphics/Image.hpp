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

#ifndef SFML_IMAGE_HPP
#define SFML_IMAGE_HPP

////////////////////////////////////////////////////////////
// Headers
////////////////////////////////////////////////////////////
#include <SFML/Graphics/Export.hpp>
#include <SFML/Graphics/Color.hpp>
#include <SFML/Graphics/Rect.hpp>
#include <string>
#include <vector>


namespace sf
{
class InputStream;

////////////////////////////////////////////////////////////
/// \brief Class for loading, manipulating and saving images
///
////////////////////////////////////////////////////////////
class SFML_GRAPHICS_API Image
{
public:

    ////////////////////////////////////////////////////////////
    /// \brief Default constructor
    ///
    /// Creates an empty image.
    ///
    ////////////////////////////////////////////////////////////
    Image();

    ////////////////////////////////////////////////////////////
    /// \brief Destructor
    ///
    ////////////////////////////////////////////////////////////
    ~Image();

    ////////////////////////////////////////////////////////////
    /// \brief Create the image and fill it with a unique color
    ///
    /// \param width  Width of the image
    /// \param height Height of the image
    /// \param color  Fill color
    ///
    ////////////////////////////////////////////////////////////
    void create(unsigned int width, unsigned int height, const Color& color = Color(0, 0, 0));

    ////////////////////////////////////////////////////////////
    /// \brief Create the image from an array of pixels
    ///
    /// The \a pixel array is assumed to contain 32-bits RGBA pixels,
    /// and have the given \a width and \a height. If not, this is
    /// an undefined behavior.
    /// If \a pixels is null, an empty image is created.
    ///
    /// \param width  Width of the image
    /// \param height Height of the image
    /// \param pixels Array of pixels to copy to the image
    ///
    ////////////////////////////////////////////////////////////
    void create(unsigned int width, unsigned int height, const Uint8* pixels);

    ////////////////////////////////////////////////////////////
    /// \brief Load the image from a file on disk
    ///
    /// The supported image formats are bmp, png, tga, jpg, gif,
    /// psd, hdr and pic. Some format options are not supported,
    /// like progressive jpeg.
    /// If this function fails, the image is left unchanged.
    ///
    /// \param filename Path of the image file to load
    ///
    /// \return True if loading was successful
    ///
    /// \see loadFromMemory, loadFromStream, saveToFile
    ///
    ////////////////////////////////////////////////////////////
    bool loadFromFile(const std::string& filename);

    ////////////////////////////////////////////////////////////
    /// \brief Load the image from a file in memory
    ///
    /// The supported image formats are bmp, png, tga, jpg, gif,
    /// psd, hdr and pic. Some format options are not supported,
    /// like progressive jpeg.
    /// If this function fails, the image is left unchanged.
    ///
    /// \param data Pointer to the file data in memory
    /// \param size Size of the data to load, in bytes
    ///
    /// \return True if loading was successful
    ///
    /// \see loadFromFile, loadFromStream
    ///
    ////////////////////////////////////////////////////////////
    bool loadFromMemory(const void* data, std::size_t size);

    ////////////////////////////////////////////////////////////
    /// \brief Load the image from a custom stream
    ///
    /// The supported image formats are bmp, png, tga, jpg, gif,
    /// psd, hdr and pic. Some format options are not supported,
    /// like progressive jpeg.
    /// If this function fails, the image is left unchanged.
    ///
    /// \param stream Source stream to read from
    ///
    /// \return True if loading was successful
    ///
    /// \see loadFromFile, loadFromMemory
    ///
    ////////////////////////////////////////////////////////////
    bool loadFromStream(InputStream& stream);

    ////////////////////////////////////////////////////////////
    /// \brief Save the image to a file on disk
    ///
    /// The format of the image is automatically deduced from
    /// the extension. The supported image formats are bmp, png,
    /// tga and jpg. The destination file is overwritten
    /// if it already exists. This function fails if the image is empty.
    ///
    /// \param filename Path of the file to save
    ///
    /// \return True if saving was successful
    ///
    /// \see create, loadFromFile, loadFromMemory
    ///
    ////////////////////////////////////////////////////////////
    bool saveToFile(const std::string& filename) const;

    ////////////////////////////////////////////////////////////
    /// \brief Return the size (width and height) of the image
    ///
    /// \return Size of the image, in pixels
    ///
    ////////////////////////////////////////////////////////////
    Vector2u getSize() const;

    ////////////////////////////////////////////////////////////
    /// \brief Create a transparency mask from a specified color-key
    ///
    /// This function sets the alpha value of every pixel matching
    /// the given color to \a alpha (0 by default), so that they
    /// become transparent.
    ///
    /// \param color Color to make transparent
    /// \param alpha Alpha value to assign to transparent pixels
    ///
    ////////////////////////////////////////////////////////////
    void createMaskFromColor(const Color& color, Uint8 alpha = 0);

    ////////////////////////////////////////////////////////////
    /// \brief Copy pixels from another image onto this one
    ///
    /// This function does a slow pixel copy and should not be
    /// used intensively. It can be used to prepare a complex
    /// static image from several others, but if you need this
    /// kind of feature in real-time you'd better use sf::RenderTexture.
    ///
    /// If \a sourceRect is empty, the whole image is copied.
    /// If \a applyAlpha is set to true, the transparency of
    /// source pixels is applied. If it is false, the pixels are
    /// copied unchanged with their alpha value.
    ///
    /// \param source     Source image to copy
    /// \param destX      X coordinate of the destination position
    /// \param destY      Y coordinate of the destination position
    /// \param sourceRect Sub-rectangle of the source image to copy
    /// \param applyAlpha Should the copy take into account the source transparency?
    ///
    ////////////////////////////////////////////////////////////
    void copy(const Image& source, unsigned int destX, unsigned int destY, const IntRect& sourceRect = IntRect(0, 0, 0, 0), bool applyAlpha = false);

    ////////////////////////////////////////////////////////////
    /// \brief Change the color of a pixel
    ///
    /// This function doesn't check the validity of the pixel
    /// coordinates, using out-of-range values will result in
    /// an undefined behavior.
    ///
    /// \param x     X coordinate of pixel to change
    /// \param y     Y coordinate of pixel to change
    /// \param color New color of the pixel
    ///
    /// \see getPixel
    ///
    ////////////////////////////////////////////////////////////
    void setPixel(unsigned int x, unsigned int y, const Color& color);

    ////////////////////////////////////////////////////////////
    /// \brief Get the color of a pixel
    ///
    /// This function doesn't check the validity of the pixel
    /// coordinates, using out-of-range values will result in
    /// an undefined behavior.
    ///
    /// \param x X coordinate of pixel to get
    /// \param y Y coordinate of pixel to get
    ///
    /// \return Color of the pixel at coordinates (x, y)
    ///
    /// \see setPixel
    ///
    ////////////////////////////////////////////////////////////
    Color getPixel(unsigned int x, unsigned int y) const;

    ////////////////////////////////////////////////////////////
    /// \brief Get a read-only pointer to the array of pixels
    ///
    /// The returned value points to an array of RGBA pixels made of
    /// 8 bits integers components. The size of the array is
    /// width * height * 4 (getSize().x * getSize().y * 4).
    /// Warning: the returned pointer may become invalid if you
    /// modify the image, so you should never store it for too long.
    /// If the image is empty, a null pointer is returned.
    ///
    /// \return Read-only pointer to the array of pixels
    ///
    ////////////////////////////////////////////////////////////
    const Uint8* getPixelsPtr() const;

    ////////////////////////////////////////////////////////////
    /// \brief Flip the image horizontally (left <-> right)
    ///
    ////////////////////////////////////////////////////////////
    void flipHorizontally();

    ////////////////////////////////////////////////////////////
    /// \brief Flip the image vertically (top <-> bottom)
    ///
    ////////////////////////////////////////////////////////////
    void flipVertically();

private:

    ////////////////////////////////////////////////////////////
    // Member data
    ////////////////////////////////////////////////////////////
    Vector2u           m_size;   ///< Image size
    std::vector<Uint8> m_pixels; ///< Pixels of the image
    #ifdef SFML_SYSTEM_ANDROID
    void*              m_stream; ///< Asset file streamer (if loaded from file)
    #endif
};

} // namespace sf


#endif // SFML_IMAGE_HPP


////////////////////////////////////////////////////////////
/// \class sf::Image
/// \ingroup graphics
///
/// sf::Image is an abstraction to manipulate images
/// as bidimensional arrays of pixels. The class provides
/// functions to load, read, write and save pixels, as well
/// as many other useful functions.
///
/// sf::Image can handle a unique internal representation of
/// pixels, which is RGBA 32 bits. This means that a pixel
/// must be composed of 8 bits red, green, blue and alpha
/// channels -- just like a sf::Color.
/// All the functions that return an array of pixels follow
/// this rule, and all parameters that you pass to sf::Image
/// functions (such as loadFromMemory) must use this
/// representation as well.
///
/// A sf::Image can be copied, but it is a heavy resource and
/// if possible you should always use [const] references to
/// pass or return them to avoid useless copies.
///
/// Usage example:
/// \code
/// // Load an image file from a file
/// sf::Image background;
/// if (!background.loadFromFile("background.jpg"))
///     return -1;
///
/// // Create a 20x20 image filled with black color
/// sf::Image image;
/// image.create(20, 20, sf::Color::Black);
///
/// // Copy image1 on image2 at position (10, 10)
/// image.copy(background, 10, 10);
///
/// // Make the top-left pixel transparent
/// sf::Color color = image.getPixel(0, 0);
/// color.a = 0;
/// image.setPixel(0, 0, color);
///
/// // Save the image to a file
/// if (!image.saveToFile("result.png"))
///     return -1;
/// \endcode
///
/// \see sf::Texture
///
////////////////////////////////////////////////////////////
