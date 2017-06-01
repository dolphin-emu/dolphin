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

#ifndef SFML_IMAGELOADER_HPP
#define SFML_IMAGELOADER_HPP

////////////////////////////////////////////////////////////
// Headers
////////////////////////////////////////////////////////////
#include <SFML/System/NonCopyable.hpp>
#include <SFML/System/Vector2.hpp>
#include <string>
#include <vector>


namespace sf
{
class InputStream;

namespace priv
{
////////////////////////////////////////////////////////////
/// \brief Load/save image files
///
////////////////////////////////////////////////////////////
class ImageLoader : NonCopyable
{
public:

    ////////////////////////////////////////////////////////////
    /// \brief Get the unique instance of the class
    ///
    /// \return Reference to the ImageLoader instance
    ///
    ////////////////////////////////////////////////////////////
    static ImageLoader& getInstance();

    ////////////////////////////////////////////////////////////
    /// \brief Load an image from a file on disk
    ///
    /// \param filename Path of image file to load
    /// \param pixels   Array of pixels to fill with loaded image
    /// \param size     Size of loaded image, in pixels
    ///
    /// \return True if loading was successful
    ///
    ////////////////////////////////////////////////////////////
    bool loadImageFromFile(const std::string& filename, std::vector<Uint8>& pixels, Vector2u& size);

    ////////////////////////////////////////////////////////////
    /// \brief Load an image from a file in memory
    ///
    /// \param data     Pointer to the file data in memory
    /// \param dataSize Size of the data to load, in bytes
    /// \param pixels   Array of pixels to fill with loaded image
    /// \param size     Size of loaded image, in pixels
    ///
    /// \return True if loading was successful
    ///
    ////////////////////////////////////////////////////////////
    bool loadImageFromMemory(const void* data, std::size_t dataSize, std::vector<Uint8>& pixels, Vector2u& size);

    ////////////////////////////////////////////////////////////
    /// \brief Load an image from a custom stream
    ///
    /// \param stream Source stream to read from
    /// \param pixels Array of pixels to fill with loaded image
    /// \param size   Size of loaded image, in pixels
    ///
    /// \return True if loading was successful
    ///
    ////////////////////////////////////////////////////////////
    bool loadImageFromStream(InputStream& stream, std::vector<Uint8>& pixels, Vector2u& size);

    ////////////////////////////////////////////////////////////
    /// \brief Save an array of pixels as an image file
    ///
    /// \param filename Path of image file to save
    /// \param pixels   Array of pixels to save to image
    /// \param size     Size of image to save, in pixels
    ///
    /// \return True if saving was successful
    ///
    ////////////////////////////////////////////////////////////
    bool saveImageToFile(const std::string& filename, const std::vector<Uint8>& pixels, const Vector2u& size);

private:

    ////////////////////////////////////////////////////////////
    /// \brief Default constructor
    ///
    ////////////////////////////////////////////////////////////
    ImageLoader();

    ////////////////////////////////////////////////////////////
    /// \brief Destructor
    ///
    ////////////////////////////////////////////////////////////
    ~ImageLoader();

    ////////////////////////////////////////////////////////////
    /// \brief Save an image file in JPEG format
    ///
    /// \param filename Path of image file to save
    /// \param pixels   Array of pixels to save to image
    /// \param width    Width of image to save, in pixels
    /// \param height   Height of image to save, in pixels
    ///
    /// \return True if saving was successful
    ///
    ////////////////////////////////////////////////////////////
    bool writeJpg(const std::string& filename, const std::vector<Uint8>& pixels, unsigned int width, unsigned int height);
};

} // namespace priv

} // namespace sf


#endif // SFML_IMAGELOADER_HPP
