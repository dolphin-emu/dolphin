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

#ifndef SFML_SHADER_HPP
#define SFML_SHADER_HPP

////////////////////////////////////////////////////////////
// Headers
////////////////////////////////////////////////////////////
#include <SFML/Graphics/Export.hpp>
#include <SFML/Graphics/Glsl.hpp>
#include <SFML/Window/GlResource.hpp>
#include <SFML/System/NonCopyable.hpp>
#include <SFML/System/Vector2.hpp>
#include <SFML/System/Vector3.hpp>
#include <map>
#include <string>


namespace sf
{
class Color;
class InputStream;
class Texture;
class Transform;

////////////////////////////////////////////////////////////
/// \brief Shader class (vertex, geometry and fragment)
///
////////////////////////////////////////////////////////////
class SFML_GRAPHICS_API Shader : GlResource, NonCopyable
{
public:

    ////////////////////////////////////////////////////////////
    /// \brief Types of shaders
    ///
    ////////////////////////////////////////////////////////////
    enum Type
    {
        Vertex,   ///< %Vertex shader
        Geometry, ///< Geometry shader
        Fragment  ///< Fragment (pixel) shader
    };

    ////////////////////////////////////////////////////////////
    /// \brief Special type that can be passed to setUniform(),
    ///        and that represents the texture of the object being drawn
    ///
    /// \see setUniform(const std::string&, CurrentTextureType)
    ///
    ////////////////////////////////////////////////////////////
    struct CurrentTextureType {};

    ////////////////////////////////////////////////////////////
    /// \brief Represents the texture of the object being drawn
    ///
    /// \see setUniform(const std::string&, CurrentTextureType)
    ///
    ////////////////////////////////////////////////////////////
    static CurrentTextureType CurrentTexture;

public:

    ////////////////////////////////////////////////////////////
    /// \brief Default constructor
    ///
    /// This constructor creates an invalid shader.
    ///
    ////////////////////////////////////////////////////////////
    Shader();

    ////////////////////////////////////////////////////////////
    /// \brief Destructor
    ///
    ////////////////////////////////////////////////////////////
    ~Shader();

    ////////////////////////////////////////////////////////////
    /// \brief Load the vertex, geometry or fragment shader from a file
    ///
    /// This function loads a single shader, vertex, geometry or
    /// fragment, identified by the second argument.
    /// The source must be a text file containing a valid
    /// shader in GLSL language. GLSL is a C-like language
    /// dedicated to OpenGL shaders; you'll probably need to
    /// read a good documentation for it before writing your
    /// own shaders.
    ///
    /// \param filename Path of the vertex, geometry or fragment shader file to load
    /// \param type     Type of shader (vertex, geometry or fragment)
    ///
    /// \return True if loading succeeded, false if it failed
    ///
    /// \see loadFromMemory, loadFromStream
    ///
    ////////////////////////////////////////////////////////////
    bool loadFromFile(const std::string& filename, Type type);

    ////////////////////////////////////////////////////////////
    /// \brief Load both the vertex and fragment shaders from files
    ///
    /// This function loads both the vertex and the fragment
    /// shaders. If one of them fails to load, the shader is left
    /// empty (the valid shader is unloaded).
    /// The sources must be text files containing valid shaders
    /// in GLSL language. GLSL is a C-like language dedicated to
    /// OpenGL shaders; you'll probably need to read a good documentation
    /// for it before writing your own shaders.
    ///
    /// \param vertexShaderFilename   Path of the vertex shader file to load
    /// \param fragmentShaderFilename Path of the fragment shader file to load
    ///
    /// \return True if loading succeeded, false if it failed
    ///
    /// \see loadFromMemory, loadFromStream
    ///
    ////////////////////////////////////////////////////////////
    bool loadFromFile(const std::string& vertexShaderFilename, const std::string& fragmentShaderFilename);

    ////////////////////////////////////////////////////////////
    /// \brief Load the vertex, geometry and fragment shaders from files
    ///
    /// This function loads the vertex, geometry and fragment
    /// shaders. If one of them fails to load, the shader is left
    /// empty (the valid shader is unloaded).
    /// The sources must be text files containing valid shaders
    /// in GLSL language. GLSL is a C-like language dedicated to
    /// OpenGL shaders; you'll probably need to read a good documentation
    /// for it before writing your own shaders.
    ///
    /// \param vertexShaderFilename   Path of the vertex shader file to load
    /// \param geometryShaderFilename Path of the geometry shader file to load
    /// \param fragmentShaderFilename Path of the fragment shader file to load
    ///
    /// \return True if loading succeeded, false if it failed
    ///
    /// \see loadFromMemory, loadFromStream
    ///
    ////////////////////////////////////////////////////////////
    bool loadFromFile(const std::string& vertexShaderFilename, const std::string& geometryShaderFilename, const std::string& fragmentShaderFilename);

    ////////////////////////////////////////////////////////////
    /// \brief Load the vertex, geometry or fragment shader from a source code in memory
    ///
    /// This function loads a single shader, vertex, geometry
    /// or fragment, identified by the second argument.
    /// The source code must be a valid shader in GLSL language.
    /// GLSL is a C-like language dedicated to OpenGL shaders;
    /// you'll probably need to read a good documentation for
    /// it before writing your own shaders.
    ///
    /// \param shader String containing the source code of the shader
    /// \param type   Type of shader (vertex, geometry or fragment)
    ///
    /// \return True if loading succeeded, false if it failed
    ///
    /// \see loadFromFile, loadFromStream
    ///
    ////////////////////////////////////////////////////////////
    bool loadFromMemory(const std::string& shader, Type type);

    ////////////////////////////////////////////////////////////
    /// \brief Load both the vertex and fragment shaders from source codes in memory
    ///
    /// This function loads both the vertex and the fragment
    /// shaders. If one of them fails to load, the shader is left
    /// empty (the valid shader is unloaded).
    /// The sources must be valid shaders in GLSL language. GLSL is
    /// a C-like language dedicated to OpenGL shaders; you'll
    /// probably need to read a good documentation for it before
    /// writing your own shaders.
    ///
    /// \param vertexShader   String containing the source code of the vertex shader
    /// \param fragmentShader String containing the source code of the fragment shader
    ///
    /// \return True if loading succeeded, false if it failed
    ///
    /// \see loadFromFile, loadFromStream
    ///
    ////////////////////////////////////////////////////////////
    bool loadFromMemory(const std::string& vertexShader, const std::string& fragmentShader);

    ////////////////////////////////////////////////////////////
    /// \brief Load the vertex, geometry and fragment shaders from source codes in memory
    ///
    /// This function loads the vertex, geometry and fragment
    /// shaders. If one of them fails to load, the shader is left
    /// empty (the valid shader is unloaded).
    /// The sources must be valid shaders in GLSL language. GLSL is
    /// a C-like language dedicated to OpenGL shaders; you'll
    /// probably need to read a good documentation for it before
    /// writing your own shaders.
    ///
    /// \param vertexShader   String containing the source code of the vertex shader
    /// \param geometryShader String containing the source code of the geometry shader
    /// \param fragmentShader String containing the source code of the fragment shader
    ///
    /// \return True if loading succeeded, false if it failed
    ///
    /// \see loadFromFile, loadFromStream
    ///
    ////////////////////////////////////////////////////////////
    bool loadFromMemory(const std::string& vertexShader, const std::string& geometryShader, const std::string& fragmentShader);

    ////////////////////////////////////////////////////////////
    /// \brief Load the vertex, geometry or fragment shader from a custom stream
    ///
    /// This function loads a single shader, vertex, geometry
    /// or fragment, identified by the second argument.
    /// The source code must be a valid shader in GLSL language.
    /// GLSL is a C-like language dedicated to OpenGL shaders;
    /// you'll probably need to read a good documentation for it
    /// before writing your own shaders.
    ///
    /// \param stream Source stream to read from
    /// \param type   Type of shader (vertex, geometry or fragment)
    ///
    /// \return True if loading succeeded, false if it failed
    ///
    /// \see loadFromFile, loadFromMemory
    ///
    ////////////////////////////////////////////////////////////
    bool loadFromStream(InputStream& stream, Type type);

    ////////////////////////////////////////////////////////////
    /// \brief Load both the vertex and fragment shaders from custom streams
    ///
    /// This function loads both the vertex and the fragment
    /// shaders. If one of them fails to load, the shader is left
    /// empty (the valid shader is unloaded).
    /// The source codes must be valid shaders in GLSL language.
    /// GLSL is a C-like language dedicated to OpenGL shaders;
    /// you'll probably need to read a good documentation for
    /// it before writing your own shaders.
    ///
    /// \param vertexShaderStream   Source stream to read the vertex shader from
    /// \param fragmentShaderStream Source stream to read the fragment shader from
    ///
    /// \return True if loading succeeded, false if it failed
    ///
    /// \see loadFromFile, loadFromMemory
    ///
    ////////////////////////////////////////////////////////////
    bool loadFromStream(InputStream& vertexShaderStream, InputStream& fragmentShaderStream);

    ////////////////////////////////////////////////////////////
    /// \brief Load the vertex, geometry and fragment shaders from custom streams
    ///
    /// This function loads the vertex, geometry and fragment
    /// shaders. If one of them fails to load, the shader is left
    /// empty (the valid shader is unloaded).
    /// The source codes must be valid shaders in GLSL language.
    /// GLSL is a C-like language dedicated to OpenGL shaders;
    /// you'll probably need to read a good documentation for
    /// it before writing your own shaders.
    ///
    /// \param vertexShaderStream   Source stream to read the vertex shader from
    /// \param geometryShaderStream Source stream to read the geometry shader from
    /// \param fragmentShaderStream Source stream to read the fragment shader from
    ///
    /// \return True if loading succeeded, false if it failed
    ///
    /// \see loadFromFile, loadFromMemory
    ///
    ////////////////////////////////////////////////////////////
    bool loadFromStream(InputStream& vertexShaderStream, InputStream& geometryShaderStream, InputStream& fragmentShaderStream);

    ////////////////////////////////////////////////////////////
    /// \brief Specify value for \p float uniform
    ///
    /// \param name Name of the uniform variable in GLSL
    /// \param x    Value of the float scalar
    ///
    ////////////////////////////////////////////////////////////
    void setUniform(const std::string& name, float x);

    ////////////////////////////////////////////////////////////
    /// \brief Specify value for \p vec2 uniform
    ///
    /// \param name   Name of the uniform variable in GLSL
    /// \param vector Value of the vec2 vector
    ///
    ////////////////////////////////////////////////////////////
    void setUniform(const std::string& name, const Glsl::Vec2& vector);

    ////////////////////////////////////////////////////////////
    /// \brief Specify value for \p vec3 uniform
    ///
    /// \param name   Name of the uniform variable in GLSL
    /// \param vector Value of the vec3 vector
    ///
    ////////////////////////////////////////////////////////////
    void setUniform(const std::string& name, const Glsl::Vec3& vector);

    ////////////////////////////////////////////////////////////
    /// \brief Specify value for \p vec4 uniform
    ///
    /// This overload can also be called with sf::Color objects
    /// that are converted to sf::Glsl::Vec4.
    ///
    /// It is important to note that the components of the color are
    /// normalized before being passed to the shader. Therefore,
    /// they are converted from range [0 .. 255] to range [0 .. 1].
    /// For example, a sf::Color(255, 127, 0, 255) will be transformed
    /// to a vec4(1.0, 0.5, 0.0, 1.0) in the shader.
    ///
    /// \param name   Name of the uniform variable in GLSL
    /// \param vector Value of the vec4 vector
    ///
    ////////////////////////////////////////////////////////////
    void setUniform(const std::string& name, const Glsl::Vec4& vector);

    ////////////////////////////////////////////////////////////
    /// \brief Specify value for \p int uniform
    ///
    /// \param name Name of the uniform variable in GLSL
    /// \param x    Value of the int scalar
    ///
    ////////////////////////////////////////////////////////////
    void setUniform(const std::string& name, int x);

    ////////////////////////////////////////////////////////////
    /// \brief Specify value for \p ivec2 uniform
    ///
    /// \param name   Name of the uniform variable in GLSL
    /// \param vector Value of the ivec2 vector
    ///
    ////////////////////////////////////////////////////////////
    void setUniform(const std::string& name, const Glsl::Ivec2& vector);

    ////////////////////////////////////////////////////////////
    /// \brief Specify value for \p ivec3 uniform
    ///
    /// \param name   Name of the uniform variable in GLSL
    /// \param vector Value of the ivec3 vector
    ///
    ////////////////////////////////////////////////////////////
    void setUniform(const std::string& name, const Glsl::Ivec3& vector);

    ////////////////////////////////////////////////////////////
    /// \brief Specify value for \p ivec4 uniform
    ///
    /// This overload can also be called with sf::Color objects
    /// that are converted to sf::Glsl::Ivec4.
    ///
    /// If color conversions are used, the ivec4 uniform in GLSL
    /// will hold the same values as the original sf::Color
    /// instance. For example, sf::Color(255, 127, 0, 255) is
    /// mapped to ivec4(255, 127, 0, 255).
    ///
    /// \param name   Name of the uniform variable in GLSL
    /// \param vector Value of the ivec4 vector
    ///
    ////////////////////////////////////////////////////////////
    void setUniform(const std::string& name, const Glsl::Ivec4& vector);

    ////////////////////////////////////////////////////////////
    /// \brief Specify value for \p bool uniform
    ///
    /// \param name Name of the uniform variable in GLSL
    /// \param x    Value of the bool scalar
    ///
    ////////////////////////////////////////////////////////////
    void setUniform(const std::string& name, bool x);

    ////////////////////////////////////////////////////////////
    /// \brief Specify value for \p bvec2 uniform
    ///
    /// \param name   Name of the uniform variable in GLSL
    /// \param vector Value of the bvec2 vector
    ///
    ////////////////////////////////////////////////////////////
    void setUniform(const std::string& name, const Glsl::Bvec2& vector);

    ////////////////////////////////////////////////////////////
    /// \brief Specify value for \p bvec3 uniform
    ///
    /// \param name   Name of the uniform variable in GLSL
    /// \param vector Value of the bvec3 vector
    ///
    ////////////////////////////////////////////////////////////
    void setUniform(const std::string& name, const Glsl::Bvec3& vector);

    ////////////////////////////////////////////////////////////
    /// \brief Specify value for \p bvec4 uniform
    ///
    /// \param name   Name of the uniform variable in GLSL
    /// \param vector Value of the bvec4 vector
    ///
    ////////////////////////////////////////////////////////////
    void setUniform(const std::string& name, const Glsl::Bvec4& vector);

    ////////////////////////////////////////////////////////////
    /// \brief Specify value for \p mat3 matrix
    ///
    /// \param name   Name of the uniform variable in GLSL
    /// \param matrix Value of the mat3 matrix
    ///
    ////////////////////////////////////////////////////////////
    void setUniform(const std::string& name, const Glsl::Mat3& matrix);

    ////////////////////////////////////////////////////////////
    /// \brief Specify value for \p mat4 matrix
    ///
    /// \param name   Name of the uniform variable in GLSL
    /// \param matrix Value of the mat4 matrix
    ///
    ////////////////////////////////////////////////////////////
    void setUniform(const std::string& name, const Glsl::Mat4& matrix);

    ////////////////////////////////////////////////////////////
    /// \brief Specify a texture as \p sampler2D uniform
    ///
    /// \a name is the name of the variable to change in the shader.
    /// The corresponding parameter in the shader must be a 2D texture
    /// (\p sampler2D GLSL type).
    ///
    /// Example:
    /// \code
    /// uniform sampler2D the_texture; // this is the variable in the shader
    /// \endcode
    /// \code
    /// sf::Texture texture;
    /// ...
    /// shader.setUniform("the_texture", texture);
    /// \endcode
    /// It is important to note that \a texture must remain alive as long
    /// as the shader uses it, no copy is made internally.
    ///
    /// To use the texture of the object being drawn, which cannot be
    /// known in advance, you can pass the special value
    /// sf::Shader::CurrentTexture:
    /// \code
    /// shader.setUniform("the_texture", sf::Shader::CurrentTexture).
    /// \endcode
    ///
    /// \param name    Name of the texture in the shader
    /// \param texture Texture to assign
    ///
    ////////////////////////////////////////////////////////////
    void setUniform(const std::string& name, const Texture& texture);

    ////////////////////////////////////////////////////////////
    /// \brief Specify current texture as \p sampler2D uniform
    ///
    /// This overload maps a shader texture variable to the
    /// texture of the object being drawn, which cannot be
    /// known in advance. The second argument must be
    /// sf::Shader::CurrentTexture.
    /// The corresponding parameter in the shader must be a 2D texture
    /// (\p sampler2D GLSL type).
    ///
    /// Example:
    /// \code
    /// uniform sampler2D current; // this is the variable in the shader
    /// \endcode
    /// \code
    /// shader.setUniform("current", sf::Shader::CurrentTexture);
    /// \endcode
    ///
    /// \param name Name of the texture in the shader
    ///
    ////////////////////////////////////////////////////////////
    void setUniform(const std::string& name, CurrentTextureType);

    ////////////////////////////////////////////////////////////
    /// \brief Specify values for \p float[] array uniform
    ///
    /// \param name        Name of the uniform variable in GLSL
    /// \param scalarArray pointer to array of \p float values
    /// \param length      Number of elements in the array
    ///
    ////////////////////////////////////////////////////////////
    void setUniformArray(const std::string& name, const float* scalarArray, std::size_t length);

    ////////////////////////////////////////////////////////////
    /// \brief Specify values for \p vec2[] array uniform
    ///
    /// \param name        Name of the uniform variable in GLSL
    /// \param vectorArray pointer to array of \p vec2 values
    /// \param length      Number of elements in the array
    ///
    ////////////////////////////////////////////////////////////
    void setUniformArray(const std::string& name, const Glsl::Vec2* vectorArray, std::size_t length);

    ////////////////////////////////////////////////////////////
    /// \brief Specify values for \p vec3[] array uniform
    ///
    /// \param name        Name of the uniform variable in GLSL
    /// \param vectorArray pointer to array of \p vec3 values
    /// \param length      Number of elements in the array
    ///
    ////////////////////////////////////////////////////////////
    void setUniformArray(const std::string& name, const Glsl::Vec3* vectorArray, std::size_t length);

    ////////////////////////////////////////////////////////////
    /// \brief Specify values for \p vec4[] array uniform
    ///
    /// \param name        Name of the uniform variable in GLSL
    /// \param vectorArray pointer to array of \p vec4 values
    /// \param length      Number of elements in the array
    ///
    ////////////////////////////////////////////////////////////
    void setUniformArray(const std::string& name, const Glsl::Vec4* vectorArray, std::size_t length);

    ////////////////////////////////////////////////////////////
    /// \brief Specify values for \p mat3[] array uniform
    ///
    /// \param name        Name of the uniform variable in GLSL
    /// \param matrixArray pointer to array of \p mat3 values
    /// \param length      Number of elements in the array
    ///
    ////////////////////////////////////////////////////////////
    void setUniformArray(const std::string& name, const Glsl::Mat3* matrixArray, std::size_t length);

    ////////////////////////////////////////////////////////////
    /// \brief Specify values for \p mat4[] array uniform
    ///
    /// \param name        Name of the uniform variable in GLSL
    /// \param matrixArray pointer to array of \p mat4 values
    /// \param length      Number of elements in the array
    ///
    ////////////////////////////////////////////////////////////
    void setUniformArray(const std::string& name, const Glsl::Mat4* matrixArray, std::size_t length);

    ////////////////////////////////////////////////////////////
    /// \brief Change a float parameter of the shader
    ///
    /// \deprecated Use setUniform(const std::string&, float) instead.
    ///
    ////////////////////////////////////////////////////////////
    SFML_DEPRECATED void setParameter(const std::string& name, float x);

    ////////////////////////////////////////////////////////////
    /// \brief Change a 2-components vector parameter of the shader
    ///
    /// \deprecated Use setUniform(const std::string&, const Glsl::Vec2&) instead.
    ///
    ////////////////////////////////////////////////////////////
    SFML_DEPRECATED void setParameter(const std::string& name, float x, float y);

    ////////////////////////////////////////////////////////////
    /// \brief Change a 3-components vector parameter of the shader
    ///
    /// \deprecated Use setUniform(const std::string&, const Glsl::Vec3&) instead.
    ///
    ////////////////////////////////////////////////////////////
    SFML_DEPRECATED void setParameter(const std::string& name, float x, float y, float z);

    ////////////////////////////////////////////////////////////
    /// \brief Change a 4-components vector parameter of the shader
    ///
    /// \deprecated Use setUniform(const std::string&, const Glsl::Vec4&) instead.
    ///
    ////////////////////////////////////////////////////////////
    SFML_DEPRECATED void setParameter(const std::string& name, float x, float y, float z, float w);

    ////////////////////////////////////////////////////////////
    /// \brief Change a 2-components vector parameter of the shader
    ///
    /// \deprecated Use setUniform(const std::string&, const Glsl::Vec2&) instead.
    ///
    ////////////////////////////////////////////////////////////
    SFML_DEPRECATED void setParameter(const std::string& name, const Vector2f& vector);

    ////////////////////////////////////////////////////////////
    /// \brief Change a 3-components vector parameter of the shader
    ///
    /// \deprecated Use setUniform(const std::string&, const Glsl::Vec3&) instead.
    ///
    ////////////////////////////////////////////////////////////
    SFML_DEPRECATED void setParameter(const std::string& name, const Vector3f& vector);

    ////////////////////////////////////////////////////////////
    /// \brief Change a color parameter of the shader
    ///
    /// \deprecated Use setUniform(const std::string&, const Glsl::Vec4&) instead.
    ///
    ////////////////////////////////////////////////////////////
    SFML_DEPRECATED void setParameter(const std::string& name, const Color& color);

    ////////////////////////////////////////////////////////////
    /// \brief Change a matrix parameter of the shader
    ///
    /// \deprecated Use setUniform(const std::string&, const Glsl::Mat4&) instead.
    ///
    ////////////////////////////////////////////////////////////
    SFML_DEPRECATED void setParameter(const std::string& name, const Transform& transform);

    ////////////////////////////////////////////////////////////
    /// \brief Change a texture parameter of the shader
    ///
    /// \deprecated Use setUniform(const std::string&, const Texture&) instead.
    ///
    ////////////////////////////////////////////////////////////
    SFML_DEPRECATED void setParameter(const std::string& name, const Texture& texture);

    ////////////////////////////////////////////////////////////
    /// \brief Change a texture parameter of the shader
    ///
    /// \deprecated Use setUniform(const std::string&, CurrentTextureType) instead.
    ///
    ////////////////////////////////////////////////////////////
    SFML_DEPRECATED void setParameter(const std::string& name, CurrentTextureType);

    ////////////////////////////////////////////////////////////
    /// \brief Get the underlying OpenGL handle of the shader.
    ///
    /// You shouldn't need to use this function, unless you have
    /// very specific stuff to implement that SFML doesn't support,
    /// or implement a temporary workaround until a bug is fixed.
    ///
    /// \return OpenGL handle of the shader or 0 if not yet loaded
    ///
    ////////////////////////////////////////////////////////////
    unsigned int getNativeHandle() const;

    ////////////////////////////////////////////////////////////
    /// \brief Bind a shader for rendering
    ///
    /// This function is not part of the graphics API, it mustn't be
    /// used when drawing SFML entities. It must be used only if you
    /// mix sf::Shader with OpenGL code.
    ///
    /// \code
    /// sf::Shader s1, s2;
    /// ...
    /// sf::Shader::bind(&s1);
    /// // draw OpenGL stuff that use s1...
    /// sf::Shader::bind(&s2);
    /// // draw OpenGL stuff that use s2...
    /// sf::Shader::bind(NULL);
    /// // draw OpenGL stuff that use no shader...
    /// \endcode
    ///
    /// \param shader Shader to bind, can be null to use no shader
    ///
    ////////////////////////////////////////////////////////////
    static void bind(const Shader* shader);

    ////////////////////////////////////////////////////////////
    /// \brief Tell whether or not the system supports shaders
    ///
    /// This function should always be called before using
    /// the shader features. If it returns false, then
    /// any attempt to use sf::Shader will fail.
    ///
    /// \return True if shaders are supported, false otherwise
    ///
    ////////////////////////////////////////////////////////////
    static bool isAvailable();

    ////////////////////////////////////////////////////////////
    /// \brief Tell whether or not the system supports geometry shaders
    ///
    /// This function should always be called before using
    /// the geometry shader features. If it returns false, then
    /// any attempt to use sf::Shader geometry shader features will fail.
    ///
    /// This function can only return true if isAvailable() would also
    /// return true, since shaders in general have to be supported in
    /// order for geometry shaders to be supported as well.
    ///
    /// Note: The first call to this function, whether by your
    /// code or SFML will result in a context switch.
    ///
    /// \return True if geometry shaders are supported, false otherwise
    ///
    ////////////////////////////////////////////////////////////
    static bool isGeometryAvailable();

private:

    ////////////////////////////////////////////////////////////
    /// \brief Compile the shader(s) and create the program
    ///
    /// If one of the arguments is NULL, the corresponding shader
    /// is not created.
    ///
    /// \param vertexShaderCode   Source code of the vertex shader
    /// \param geometryShaderCode Source code of the geometry shader
    /// \param fragmentShaderCode Source code of the fragment shader
    ///
    /// \return True on success, false if any error happened
    ///
    ////////////////////////////////////////////////////////////
    bool compile(const char* vertexShaderCode, const char* geometryShaderCode, const char* fragmentShaderCode);

    ////////////////////////////////////////////////////////////
    /// \brief Bind all the textures used by the shader
    ///
    /// This function each texture to a different unit, and
    /// updates the corresponding variables in the shader accordingly.
    ///
    ////////////////////////////////////////////////////////////
    void bindTextures() const;

    ////////////////////////////////////////////////////////////
    /// \brief Get the location ID of a shader uniform
    ///
    /// \param name Name of the uniform variable to search
    ///
    /// \return Location ID of the uniform, or -1 if not found
    ///
    ////////////////////////////////////////////////////////////
    int getUniformLocation(const std::string& name);

    ////////////////////////////////////////////////////////////
    /// \brief RAII object to save and restore the program
    ///        binding while uniforms are being set
    ///
    /// Implementation is private in the .cpp file.
    ///
    ////////////////////////////////////////////////////////////
    struct UniformBinder;

    ////////////////////////////////////////////////////////////
    // Types
    ////////////////////////////////////////////////////////////
    typedef std::map<int, const Texture*> TextureTable;
    typedef std::map<std::string, int> UniformTable;

    ////////////////////////////////////////////////////////////
    // Member data
    ////////////////////////////////////////////////////////////
    unsigned int m_shaderProgram;  ///< OpenGL identifier for the program
    int          m_currentTexture; ///< Location of the current texture in the shader
    TextureTable m_textures;       ///< Texture variables in the shader, mapped to their location
    UniformTable m_uniforms;       ///< Parameters location cache
};

} // namespace sf


#endif // SFML_SHADER_HPP


////////////////////////////////////////////////////////////
/// \class sf::Shader
/// \ingroup graphics
///
/// Shaders are programs written using a specific language,
/// executed directly by the graphics card and allowing
/// to apply real-time operations to the rendered entities.
///
/// There are three kinds of shaders:
/// \li %Vertex shaders, that process vertices
/// \li Geometry shaders, that process primitives
/// \li Fragment (pixel) shaders, that process pixels
///
/// A sf::Shader can be composed of either a vertex shader
/// alone, a geometry shader alone, a fragment shader alone,
/// or any combination of them. (see the variants of the
/// load functions).
///
/// Shaders are written in GLSL, which is a C-like
/// language dedicated to OpenGL shaders. You'll probably
/// need to learn its basics before writing your own shaders
/// for SFML.
///
/// Like any C/C++ program, a GLSL shader has its own variables
/// called \a uniforms that you can set from your C++ application.
/// sf::Shader handles different types of uniforms:
/// \li scalars: \p float, \p int, \p bool
/// \li vectors (2, 3 or 4 components)
/// \li matrices (3x3 or 4x4)
/// \li samplers (textures)
///
/// Some SFML-specific types can be converted:
/// \li sf::Color as a 4D vector (\p vec4)
/// \li sf::Transform as matrices (\p mat3 or \p mat4)
///
/// Every uniform variable in a shader can be set through one of the
/// setUniform() or setUniformArray() overloads. For example, if you
/// have a shader with the following uniforms:
/// \code
/// uniform float offset;
/// uniform vec3 point;
/// uniform vec4 color;
/// uniform mat4 matrix;
/// uniform sampler2D overlay;
/// uniform sampler2D current;
/// \endcode
/// You can set their values from C++ code as follows, using the types
/// defined in the sf::Glsl namespace:
/// \code
/// shader.setUniform("offset", 2.f);
/// shader.setUniform("point", sf::Vector3f(0.5f, 0.8f, 0.3f));
/// shader.setUniform("color", sf::Glsl::Vec4(color));          // color is a sf::Color
/// shader.setUniform("matrix", sf::Glsl::Mat4(transform));     // transform is a sf::Transform
/// shader.setUniform("overlay", texture);                      // texture is a sf::Texture
/// shader.setUniform("current", sf::Shader::CurrentTexture);
/// \endcode
///
/// The old setParameter() overloads are deprecated and will be removed in a
/// future version. You should use their setUniform() equivalents instead.
///
/// The special Shader::CurrentTexture argument maps the
/// given \p sampler2D uniform to the current texture of the
/// object being drawn (which cannot be known in advance).
///
/// To apply a shader to a drawable, you must pass it as an
/// additional parameter to the \ref Window::draw() draw() function:
/// \code
/// window.draw(sprite, &shader);
/// \endcode
///
/// ... which is in fact just a shortcut for this:
/// \code
/// sf::RenderStates states;
/// states.shader = &shader;
/// window.draw(sprite, states);
/// \endcode
///
/// In the code above we pass a pointer to the shader, because it may
/// be null (which means "no shader").
///
/// Shaders can be used on any drawable, but some combinations are
/// not interesting. For example, using a vertex shader on a sf::Sprite
/// is limited because there are only 4 vertices, the sprite would
/// have to be subdivided in order to apply wave effects.
/// Another bad example is a fragment shader with sf::Text: the texture
/// of the text is not the actual text that you see on screen, it is
/// a big texture containing all the characters of the font in an
/// arbitrary order; thus, texture lookups on pixels other than the
/// current one may not give you the expected result.
///
/// Shaders can also be used to apply global post-effects to the
/// current contents of the target (like the old sf::PostFx class
/// in SFML 1). This can be done in two different ways:
/// \li draw everything to a sf::RenderTexture, then draw it to
///     the main target using the shader
/// \li draw everything directly to the main target, then use
///     sf::Texture::update(Window&) to copy its contents to a texture
///     and draw it to the main target using the shader
///
/// The first technique is more optimized because it doesn't involve
/// retrieving the target's pixels to system memory, but the
/// second one doesn't impact the rendering process and can be
/// easily inserted anywhere without impacting all the code.
///
/// Like sf::Texture that can be used as a raw OpenGL texture,
/// sf::Shader can also be used directly as a raw shader for
/// custom OpenGL geometry.
/// \code
/// sf::Shader::bind(&shader);
/// ... render OpenGL geometry ...
/// sf::Shader::bind(NULL);
/// \endcode
///
/// \see sf::Glsl
///
////////////////////////////////////////////////////////////
