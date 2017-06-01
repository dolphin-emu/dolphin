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

#ifndef SFML_VIEW_HPP
#define SFML_VIEW_HPP

////////////////////////////////////////////////////////////
// Headers
////////////////////////////////////////////////////////////
#include <SFML/Graphics/Export.hpp>
#include <SFML/Graphics/Rect.hpp>
#include <SFML/Graphics/Transform.hpp>
#include <SFML/System/Vector2.hpp>


namespace sf
{
////////////////////////////////////////////////////////////
/// \brief 2D camera that defines what region is shown on screen
///
////////////////////////////////////////////////////////////
class SFML_GRAPHICS_API View
{
public:

    ////////////////////////////////////////////////////////////
    /// \brief Default constructor
    ///
    /// This constructor creates a default view of (0, 0, 1000, 1000)
    ///
    ////////////////////////////////////////////////////////////
    View();

    ////////////////////////////////////////////////////////////
    /// \brief Construct the view from a rectangle
    ///
    /// \param rectangle Rectangle defining the zone to display
    ///
    ////////////////////////////////////////////////////////////
    explicit View(const FloatRect& rectangle);

    ////////////////////////////////////////////////////////////
    /// \brief Construct the view from its center and size
    ///
    /// \param center Center of the zone to display
    /// \param size   Size of zone to display
    ///
    ////////////////////////////////////////////////////////////
    View(const Vector2f& center, const Vector2f& size);

    ////////////////////////////////////////////////////////////
    /// \brief Set the center of the view
    ///
    /// \param x X coordinate of the new center
    /// \param y Y coordinate of the new center
    ///
    /// \see setSize, getCenter
    ///
    ////////////////////////////////////////////////////////////
    void setCenter(float x, float y);

    ////////////////////////////////////////////////////////////
    /// \brief Set the center of the view
    ///
    /// \param center New center
    ///
    /// \see setSize, getCenter
    ///
    ////////////////////////////////////////////////////////////
    void setCenter(const Vector2f& center);

    ////////////////////////////////////////////////////////////
    /// \brief Set the size of the view
    ///
    /// \param width  New width of the view
    /// \param height New height of the view
    ///
    /// \see setCenter, getCenter
    ///
    ////////////////////////////////////////////////////////////
    void setSize(float width, float height);

    ////////////////////////////////////////////////////////////
    /// \brief Set the size of the view
    ///
    /// \param size New size
    ///
    /// \see setCenter, getCenter
    ///
    ////////////////////////////////////////////////////////////
    void setSize(const Vector2f& size);

    ////////////////////////////////////////////////////////////
    /// \brief Set the orientation of the view
    ///
    /// The default rotation of a view is 0 degree.
    ///
    /// \param angle New angle, in degrees
    ///
    /// \see getRotation
    ///
    ////////////////////////////////////////////////////////////
    void setRotation(float angle);

    ////////////////////////////////////////////////////////////
    /// \brief Set the target viewport
    ///
    /// The viewport is the rectangle into which the contents of the
    /// view are displayed, expressed as a factor (between 0 and 1)
    /// of the size of the RenderTarget to which the view is applied.
    /// For example, a view which takes the left side of the target would
    /// be defined with View.setViewport(sf::FloatRect(0, 0, 0.5, 1)).
    /// By default, a view has a viewport which covers the entire target.
    ///
    /// \param viewport New viewport rectangle
    ///
    /// \see getViewport
    ///
    ////////////////////////////////////////////////////////////
    void setViewport(const FloatRect& viewport);

    ////////////////////////////////////////////////////////////
    /// \brief Reset the view to the given rectangle
    ///
    /// Note that this function resets the rotation angle to 0.
    ///
    /// \param rectangle Rectangle defining the zone to display
    ///
    /// \see setCenter, setSize, setRotation
    ///
    ////////////////////////////////////////////////////////////
    void reset(const FloatRect& rectangle);

    ////////////////////////////////////////////////////////////
    /// \brief Get the center of the view
    ///
    /// \return Center of the view
    ///
    /// \see getSize, setCenter
    ///
    ////////////////////////////////////////////////////////////
    const Vector2f& getCenter() const;

    ////////////////////////////////////////////////////////////
    /// \brief Get the size of the view
    ///
    /// \return Size of the view
    ///
    /// \see getCenter, setSize
    ///
    ////////////////////////////////////////////////////////////
    const Vector2f& getSize() const;

    ////////////////////////////////////////////////////////////
    /// \brief Get the current orientation of the view
    ///
    /// \return Rotation angle of the view, in degrees
    ///
    /// \see setRotation
    ///
    ////////////////////////////////////////////////////////////
    float getRotation() const;

    ////////////////////////////////////////////////////////////
    /// \brief Get the target viewport rectangle of the view
    ///
    /// \return Viewport rectangle, expressed as a factor of the target size
    ///
    /// \see setViewport
    ///
    ////////////////////////////////////////////////////////////
    const FloatRect& getViewport() const;

    ////////////////////////////////////////////////////////////
    /// \brief Move the view relatively to its current position
    ///
    /// \param offsetX X coordinate of the move offset
    /// \param offsetY Y coordinate of the move offset
    ///
    /// \see setCenter, rotate, zoom
    ///
    ////////////////////////////////////////////////////////////
    void move(float offsetX, float offsetY);

    ////////////////////////////////////////////////////////////
    /// \brief Move the view relatively to its current position
    ///
    /// \param offset Move offset
    ///
    /// \see setCenter, rotate, zoom
    ///
    ////////////////////////////////////////////////////////////
    void move(const Vector2f& offset);

    ////////////////////////////////////////////////////////////
    /// \brief Rotate the view relatively to its current orientation
    ///
    /// \param angle Angle to rotate, in degrees
    ///
    /// \see setRotation, move, zoom
    ///
    ////////////////////////////////////////////////////////////
    void rotate(float angle);

    ////////////////////////////////////////////////////////////
    /// \brief Resize the view rectangle relatively to its current size
    ///
    /// Resizing the view simulates a zoom, as the zone displayed on
    /// screen grows or shrinks.
    /// \a factor is a multiplier:
    /// \li 1 keeps the size unchanged
    /// \li > 1 makes the view bigger (objects appear smaller)
    /// \li < 1 makes the view smaller (objects appear bigger)
    ///
    /// \param factor Zoom factor to apply
    ///
    /// \see setSize, move, rotate
    ///
    ////////////////////////////////////////////////////////////
    void zoom(float factor);

    ////////////////////////////////////////////////////////////
    /// \brief Get the projection transform of the view
    ///
    /// This function is meant for internal use only.
    ///
    /// \return Projection transform defining the view
    ///
    /// \see getInverseTransform
    ///
    ////////////////////////////////////////////////////////////
    const Transform& getTransform() const;

    ////////////////////////////////////////////////////////////
    /// \brief Get the inverse projection transform of the view
    ///
    /// This function is meant for internal use only.
    ///
    /// \return Inverse of the projection transform defining the view
    ///
    /// \see getTransform
    ///
    ////////////////////////////////////////////////////////////
    const Transform& getInverseTransform() const;

private:

    ////////////////////////////////////////////////////////////
    // Member data
    ////////////////////////////////////////////////////////////
    Vector2f          m_center;              ///< Center of the view, in scene coordinates
    Vector2f          m_size;                ///< Size of the view, in scene coordinates
    float             m_rotation;            ///< Angle of rotation of the view rectangle, in degrees
    FloatRect         m_viewport;            ///< Viewport rectangle, expressed as a factor of the render-target's size
    mutable Transform m_transform;           ///< Precomputed projection transform corresponding to the view
    mutable Transform m_inverseTransform;    ///< Precomputed inverse projection transform corresponding to the view
    mutable bool      m_transformUpdated;    ///< Internal state telling if the transform needs to be updated
    mutable bool      m_invTransformUpdated; ///< Internal state telling if the inverse transform needs to be updated
};

} // namespace sf


#endif // SFML_VIEW_HPP


////////////////////////////////////////////////////////////
/// \class sf::View
/// \ingroup graphics
///
/// sf::View defines a camera in the 2D scene. This is a
/// very powerful concept: you can scroll, rotate or zoom
/// the entire scene without altering the way that your
/// drawable objects are drawn.
///
/// A view is composed of a source rectangle, which defines
/// what part of the 2D scene is shown, and a target viewport,
/// which defines where the contents of the source rectangle
/// will be displayed on the render target (window or texture).
///
/// The viewport allows to map the scene to a custom part
/// of the render target, and can be used for split-screen
/// or for displaying a minimap, for example. If the source
/// rectangle doesn't have the same size as the viewport, its
/// contents will be stretched to fit in.
///
/// To apply a view, you have to assign it to the render target.
/// Then, objects drawn in this render target will be
/// affected by the view until you use another view.
///
/// Usage example:
/// \code
/// sf::RenderWindow window;
/// sf::View view;
///
/// // Initialize the view to a rectangle located at (100, 100) and with a size of 400x200
/// view.reset(sf::FloatRect(100, 100, 400, 200));
///
/// // Rotate it by 45 degrees
/// view.rotate(45);
///
/// // Set its target viewport to be half of the window
/// view.setViewport(sf::FloatRect(0.f, 0.f, 0.5f, 1.f));
///
/// // Apply it
/// window.setView(view);
///
/// // Render stuff
/// window.draw(someSprite);
///
/// // Set the default view back
/// window.setView(window.getDefaultView());
///
/// // Render stuff not affected by the view
/// window.draw(someText);
/// \endcode
///
/// See also the note on coordinates and undistorted rendering in sf::Transformable.
///
/// \see sf::RenderWindow, sf::RenderTexture
///
////////////////////////////////////////////////////////////
