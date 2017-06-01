
////////////////////////////////////////////////////////////
// Headers
////////////////////////////////////////////////////////////
#include <SFML/Graphics.hpp>
#include <SFML/OpenGL.hpp>

#ifndef GL_SRGB8_ALPHA8
#define GL_SRGB8_ALPHA8 0x8C43
#endif


////////////////////////////////////////////////////////////
/// Entry point of application
///
/// \return Application exit code
///
////////////////////////////////////////////////////////////
int main()
{
    bool exit = false;
    bool sRgb = false;

    while (!exit)
    {
        // Request a 24-bits depth buffer when creating the window
        sf::ContextSettings contextSettings;
        contextSettings.depthBits = 24;
        contextSettings.sRgbCapable = sRgb;

        // Create the main window
        sf::RenderWindow window(sf::VideoMode(800, 600), "SFML graphics with OpenGL", sf::Style::Default, contextSettings);
        window.setVerticalSyncEnabled(true);

        // Create a sprite for the background
        sf::Texture backgroundTexture;
        backgroundTexture.setSrgb(sRgb);
        if (!backgroundTexture.loadFromFile("resources/background.jpg"))
            return EXIT_FAILURE;
        sf::Sprite background(backgroundTexture);

        // Create some text to draw on top of our OpenGL object
        sf::Font font;
        if (!font.loadFromFile("resources/sansation.ttf"))
            return EXIT_FAILURE;
        sf::Text text("SFML / OpenGL demo", font);
        sf::Text sRgbInstructions("Press space to toggle sRGB conversion", font);
        sf::Text mipmapInstructions("Press return to toggle mipmapping", font);
        text.setFillColor(sf::Color(255, 255, 255, 170));
        sRgbInstructions.setFillColor(sf::Color(255, 255, 255, 170));
        mipmapInstructions.setFillColor(sf::Color(255, 255, 255, 170));
        text.setPosition(250.f, 450.f);
        sRgbInstructions.setPosition(150.f, 500.f);
        mipmapInstructions.setPosition(180.f, 550.f);

        // Load a texture to apply to our 3D cube
        sf::Texture texture;
        if (!texture.loadFromFile("resources/texture.jpg"))
            return EXIT_FAILURE;

        // Attempt to generate a mipmap for our cube texture
        // We don't check the return value here since
        // mipmapping is purely optional in this example
        texture.generateMipmap();

        // Enable Z-buffer read and write
        glEnable(GL_DEPTH_TEST);
        glDepthMask(GL_TRUE);
        glClearDepth(1.f);

        // Disable lighting
        glDisable(GL_LIGHTING);

        // Configure the viewport (the same size as the window)
        glViewport(0, 0, window.getSize().x, window.getSize().y);

        // Setup a perspective projection
        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        GLfloat ratio = static_cast<float>(window.getSize().x) / window.getSize().y;
        glFrustum(-ratio, ratio, -1.f, 1.f, 1.f, 500.f);

        // Bind the texture
        glEnable(GL_TEXTURE_2D);
        sf::Texture::bind(&texture);

        // Define a 3D cube (6 faces made of 2 triangles composed by 3 vertices)
        static const GLfloat cube[] =
        {
            // positions    // texture coordinates
            -20, -20, -20,  0, 0,
            -20,  20, -20,  1, 0,
            -20, -20,  20,  0, 1,
            -20, -20,  20,  0, 1,
            -20,  20, -20,  1, 0,
            -20,  20,  20,  1, 1,

             20, -20, -20,  0, 0,
             20,  20, -20,  1, 0,
             20, -20,  20,  0, 1,
             20, -20,  20,  0, 1,
             20,  20, -20,  1, 0,
             20,  20,  20,  1, 1,

            -20, -20, -20,  0, 0,
             20, -20, -20,  1, 0,
            -20, -20,  20,  0, 1,
            -20, -20,  20,  0, 1,
             20, -20, -20,  1, 0,
             20, -20,  20,  1, 1,

            -20,  20, -20,  0, 0,
             20,  20, -20,  1, 0,
            -20,  20,  20,  0, 1,
            -20,  20,  20,  0, 1,
             20,  20, -20,  1, 0,
             20,  20,  20,  1, 1,

            -20, -20, -20,  0, 0,
             20, -20, -20,  1, 0,
            -20,  20, -20,  0, 1,
            -20,  20, -20,  0, 1,
             20, -20, -20,  1, 0,
             20,  20, -20,  1, 1,

            -20, -20,  20,  0, 0,
             20, -20,  20,  1, 0,
            -20,  20,  20,  0, 1,
            -20,  20,  20,  0, 1,
             20, -20,  20,  1, 0,
             20,  20,  20,  1, 1
        };

        // Enable position and texture coordinates vertex components
        glEnableClientState(GL_VERTEX_ARRAY);
        glEnableClientState(GL_TEXTURE_COORD_ARRAY);
        glVertexPointer(3, GL_FLOAT, 5 * sizeof(GLfloat), cube);
        glTexCoordPointer(2, GL_FLOAT, 5 * sizeof(GLfloat), cube + 3);

        // Disable normal and color vertex components
        glDisableClientState(GL_NORMAL_ARRAY);
        glDisableClientState(GL_COLOR_ARRAY);

        // Create a clock for measuring the time elapsed
        sf::Clock clock;

        // Flag to track whether mipmapping is currently enabled
        bool mipmapEnabled = true;

        // Start game loop
        while (window.isOpen())
        {
            // Process events
            sf::Event event;
            while (window.pollEvent(event))
            {
                // Close window: exit
                if (event.type == sf::Event::Closed)
                {
                    exit = true;
                    window.close();
                }

                // Escape key: exit
                if ((event.type == sf::Event::KeyPressed) && (event.key.code == sf::Keyboard::Escape))
                {
                    exit = true;
                    window.close();
                }

                // Return key: toggle mipmapping
                if ((event.type == sf::Event::KeyPressed) && (event.key.code == sf::Keyboard::Return))
                {
                    if (mipmapEnabled)
                    {
                        // We simply reload the texture to disable mipmapping
                        if (!texture.loadFromFile("resources/texture.jpg"))
                            return EXIT_FAILURE;

                        mipmapEnabled = false;
                    }
                    else
                    {
                        texture.generateMipmap();

                        mipmapEnabled = true;
                    }
                }

                // Space key: toggle sRGB conversion
                if ((event.type == sf::Event::KeyPressed) && (event.key.code == sf::Keyboard::Space))
                {
                    sRgb = !sRgb;
                    window.close();
                }

                // Adjust the viewport when the window is resized
                if (event.type == sf::Event::Resized)
                    glViewport(0, 0, event.size.width, event.size.height);
            }

            // Draw the background
            window.pushGLStates();
            window.draw(background);
            window.popGLStates();

            // Clear the depth buffer
            glClear(GL_DEPTH_BUFFER_BIT);

            // We get the position of the mouse cursor, so that we can move the box accordingly
            float x =  sf::Mouse::getPosition(window).x * 200.f / window.getSize().x - 100.f;
            float y = -sf::Mouse::getPosition(window).y * 200.f / window.getSize().y + 100.f;

            // Apply some transformations
            glMatrixMode(GL_MODELVIEW);
            glLoadIdentity();
            glTranslatef(x, y, -100.f);
            glRotatef(clock.getElapsedTime().asSeconds() * 50.f, 1.f, 0.f, 0.f);
            glRotatef(clock.getElapsedTime().asSeconds() * 30.f, 0.f, 1.f, 0.f);
            glRotatef(clock.getElapsedTime().asSeconds() * 90.f, 0.f, 0.f, 1.f);

            // Draw the cube
            glDrawArrays(GL_TRIANGLES, 0, 36);

            // Draw some text on top of our OpenGL object
            window.pushGLStates();
            window.draw(text);
            window.draw(sRgbInstructions);
            window.draw(mipmapInstructions);
            window.popGLStates();

            // Finally, display the rendered frame on screen
            window.display();
        }
    }

    return EXIT_SUCCESS;
}
