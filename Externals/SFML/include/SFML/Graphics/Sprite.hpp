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

#ifndef SFML_SPRITE_HPP
#define SFML_SPRITE_HPP

////////////////////////////////////////////////////////////
// Headers
////////////////////////////////////////////////////////////
#include <SFML/Graphics/Export.hpp>
#include <SFML/Graphics/Drawable.hpp>
#include <SFML/Graphics/Transformable.hpp>
#include <SFML/Graphics/Vertex.hpp>
#include <SFML/Graphics/Rect.hpp>


namespace sf
{
class Texture;

////////////////////////////////////////////////////////////
/// \brief Drawable representation of a texture, with its
///        own transformations, color, etc.
///
////////////////////////////////////////////////////////////
class SFML_GRAPHICS_API Sprite : public Drawable, public Transformable
{
public:

    ////////////////////////////////////////////////////////////
    /// \brief Default constructor
    ///
    /// Creates an empty sprite with no source texture.
    ///
    ////////////////////////////////////////////////////////////
    Sprite();

    ////////////////////////////////////////////////////////////
    /// \brief Construct the sprite from a source texture
    ///
    /// \param texture Source texture
    ///
    /// \see setTexture
    ///
    ////////////////////////////////////////////////////////////
    explicit Sprite(const Texture& texture);

    ////////////////////////////////////////////////////////////
    /// \brief Construct the sprite from a sub-rectangle of a source texture
    ///
    /// \param texture   Source texture
    /// \param rectangle Sub-rectangle of the texture to assign to the sprite
    ///
    /// \see setTexture, setTextureRect
    ///
    ////////////////////////////////////////////////////////////
    Sprite(const Texture& texture, const IntRect& rectangle);

    ////////////////////////////////////////////////////////////
    /// \brief Change the source texture of the sprite
    ///
    /// The \a texture argument refers to a texture that must
    /// exist as long as the sprite uses it. Indeed, the sprite
    /// doesn't store its own copy of the texture, but rather keeps
    /// a pointer to the one that you passed to this function.
    /// If the source texture is destroyed and the sprite tries to
    /// use it, the behavior is undefined.
    /// If \a resetRect is true, the TextureRect property of
    /// the sprite is automatically adjusted to the size of the new
    /// texture. If it is false, the texture rect is left unchanged.
    ///
    /// \param texture   New texture
    /// \param resetRect Should the texture rect be reset to the size of the new texture?
    ///
    /// \see getTexture, setTextureRect
    ///
    ////////////////////////////////////////////////////////////
    void setTexture(const Texture& texture, bool resetRect = false);

    ////////////////////////////////////////////////////////////
    /// \brief Set the sub-rectangle of the texture that the sprite will display
    ///
    /// The texture rect is useful when you don't want to display
    /// the whole texture, but rather a part of it.
    /// By default, the texture rect covers the entire texture.
    ///
    /// \param rectangle Rectangle defining the region of the texture to display
    ///
    /// \see getTextureRect, setTexture
    ///
    ////////////////////////////////////////////////////////////
    void setTextureRect(const IntRect& rectangle);

    ////////////////////////////////////////////////////////////
    /// \brief Set the global color of the sprite
    ///
    /// This color is modulated (multiplied) with the sprite's
    /// texture. It can be used to colorize the sprite, or change
    /// its global opacity.
    /// By default, the sprite's color is opaque white.
    ///
    /// \param color New color of the sprite
    ///
    /// \see getColor
    ///
    ////////////////////////////////////////////////////////////
    void setColor(const Color& color);

    ////////////////////////////////////////////////////////////
    /// \brief Get the source texture of the sprite
    ///
    /// If the sprite has no source texture, a NULL pointer is returned.
    /// The returned pointer is const, which means that you can't
    /// modify the texture when you retrieve it with this function.
    ///
    /// \return Pointer to the sprite's texture
    ///
    /// \see setTexture
    ///
    ////////////////////////////////////////////////////////////
    const Texture* getTexture() const;

    ////////////////////////////////////////////////////////////
    /// \brief Get the sub-rectangle of the texture displayed by the sprite
    ///
    /// \return Texture rectangle of the sprite
    ///
    /// \see setTextureRect
    ///
    ////////////////////////////////////////////////////////////
    const IntRect& getTextureRect() const;

    ////////////////////////////////////////////////////////////
    /// \brief Get the global color of the sprite
    ///
    /// \return Global color of the sprite
    ///
    /// \see setColor
    ///
    ////////////////////////////////////////////////////////////
    const Color& getColor() const;

    ////////////////////////////////////////////////////////////
    /// \brief Get the local bounding rectangle of the entity
    ///
    /// The returned rectangle is in local coordinates, which means
    /// that it ignores the transformations (translation, rotation,
    /// scale, ...) that are applied to the entity.
    /// In other words, this function returns the bounds of the
    /// entity in the entity's coordinate system.
    ///
    /// \return Local bounding rectangle of the entity
    ///
    ////////////////////////////////////////////////////////////
    FloatRect getLocalBounds() const;

    ////////////////////////////////////////////////////////////
    /// \brief Get the global bounding rectangle of the entity
    ///
    /// The returned rectangle is in global coordinates, which means
    /// that it takes into account the transformations (translation,
    /// rotation, scale, ...) that are applied to the entity.
    /// In other words, this function returns the bounds of the
    /// sprite in the global 2D world's coordinate system.
    ///
    /// \return Global bounding rectangle of the entity
    ///
    ////////////////////////////////////////////////////////////
    FloatRect getGlobalBounds() const;

private:

    ////////////////////////////////////////////////////////////
    /// \brief Draw the sprite to a render target
    ///
    /// \param target Render target to draw to
    /// \param states Current render states
    ///
    ////////////////////////////////////////////////////////////
    virtual void draw(RenderTarget& target, RenderStates states) const;

    ////////////////////////////////////////////////////////////
    /// \brief Update the vertices' positions
    ///
    ////////////////////////////////////////////////////////////
    void updatePositions();

    ////////////////////////////////////////////////////////////
    /// \brief Update the vertices' texture coordinates
    ///
    ////////////////////////////////////////////////////////////
    void updateTexCoords();

    ////////////////////////////////////////////////////////////
    // Member data
    ////////////////////////////////////////////////////////////
    Vertex         m_vertices[4]; ///< Vertices defining the sprite's geometry
    const Texture* m_texture;     ///< Texture of the sprite
    IntRect        m_textureRect; ///< Rectangle defining the area of the source texture to display
};

} // namespace sf


#endif // SFML_SPRITE_HPP


////////////////////////////////////////////////////////////
/// \class sf::Sprite
/// \ingroup graphics
///
/// sf::Sprite is a drawable class that allows to easily display
/// a texture (or a part of it) on a render target.
///
/// It inherits all the functions from sf::Transformable:
/// position, rotation, scale, origin. It also adds sprite-specific
/// properties such as the texture to use, the part of it to display,
/// and some convenience functions to change the overall color of the
/// sprite, or to get its bounding rectangle.
///
/// sf::Sprite works in combination with the sf::Texture class, which
/// loads and provides the pixel data of a given texture.
///
/// The separation of sf::Sprite and sf::Texture allows more flexibility
/// and better performances: indeed a sf::Texture is a heavy resource,
/// and any operation on it is slow (often too slow for real-time
/// applications). On the other side, a sf::Sprite is a lightweight
/// object which can use the pixel data of a sf::Texture and draw
/// it with its own transformation/color/blending attributes.
///
/// It is important to note that the sf::Sprite instance doesn't
/// copy the texture that it uses, it only keeps a reference to it.
/// Thus, a sf::Texture must not be destroyed while it is
/// used by a sf::Sprite (i.e. never write a function that
/// uses a local sf::Texture instance for creating a sprite).
///
/// See also the note on coordinates and undistorted rendering in sf::Transformable.
///
/// Usage example:
/// \code
/// // Declare and load a texture
/// sf::Texture texture;
/// texture.loadFromFile("texture.png");
///
/// // Create a sprite
/// sf::Sprite sprite;
/// sprite.setTexture(texture);
/// sprite.setTextureRect(sf::IntRect(10, 10, 50, 30));
/// sprite.setColor(sf::Color(255, 255, 255, 200));
/// sprite.setPosition(100, 25);
///
/// // Draw it
/// window.draw(sprite);
/// \endcode
///
/// \see sf::Texture, sf::Transformable
///
////////////////////////////////////////////////////////////
