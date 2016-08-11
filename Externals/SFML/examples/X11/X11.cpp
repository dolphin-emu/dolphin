
////////////////////////////////////////////////////////////
// Headers
////////////////////////////////////////////////////////////
#include <SFML/Window.hpp>
#include <SFML/System/Err.hpp>
#include <SFML/OpenGL.hpp>
#include <X11/Xlib-xcb.h>
#include <iostream>
#include <cmath>


////////////////////////////////////////////////////////////
/// Initialize OpenGL states into the specified view
///
/// \param Window Target window to initialize
///
////////////////////////////////////////////////////////////
void initialize(sf::Window& window)
{
    // Activate the window
    window.setActive();

    // Setup OpenGL states
    // Set color and depth clear value
    glClearDepth(1.f);
    glClearColor(0.f, 0.5f, 0.5f, 0.f);

    // Enable Z-buffer read and write
    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);

    // Setup a perspective projection
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    static const double pi = 3.141592654;
    GLdouble extent = std::tan(90.0 * pi / 360.0);
    glFrustum(-extent, extent, -extent, extent, 1.0, 500.0);

    // Enable position and texture coordinates vertex components
    glEnableClientState(GL_VERTEX_ARRAY);
    glEnableClientState(GL_COLOR_ARRAY);
}

////////////////////////////////////////////////////////////
/// Draw the OpenGL scene (a rotating cube) into
/// the specified view
///
/// \param window      Target window for rendering
/// \param elapsedTime Time elapsed since the last draw
///
////////////////////////////////////////////////////////////
void draw(sf::Window& window, float elapsedTime)
{
    // Activate the window
    window.setActive();

    // Clear color and depth buffers
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // Apply some transformations
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glTranslatef(0.f, 0.f, -200.f);
    glRotatef(elapsedTime * 10.f, 1.f, 0.f, 0.f);
    glRotatef(elapsedTime * 6.f, 0.f, 1.f, 0.f);
    glRotatef(elapsedTime * 18.f, 0.f, 0.f, 1.f);

    // Define a 3D cube (6 faces made of 2 triangles composed by 3 vertices)
    static const GLfloat cube[] =
    {
        // positions    // colors
        -50, -50, -50,  1, 1, 0,
        -50,  50, -50,  1, 1, 0,
        -50, -50,  50,  1, 1, 0,
        -50, -50,  50,  1, 1, 0,
        -50,  50, -50,  1, 1, 0,
        -50,  50,  50,  1, 1, 0,

         50, -50, -50,  1, 1, 0,
         50,  50, -50,  1, 1, 0,
         50, -50,  50,  1, 1, 0,
         50, -50,  50,  1, 1, 0,
         50,  50, -50,  1, 1, 0,
         50,  50,  50,  1, 1, 0,

        -50, -50, -50,  1, 0, 1,
         50, -50, -50,  1, 0, 1,
        -50, -50,  50,  1, 0, 1,
        -50, -50,  50,  1, 0, 1,
         50, -50, -50,  1, 0, 1,
         50, -50,  50,  1, 0, 1,

        -50,  50, -50,  1, 0, 1,
         50,  50, -50,  1, 0, 1,
        -50,  50,  50,  1, 0, 1,
        -50,  50,  50,  1, 0, 1,
         50,  50, -50,  1, 0, 1,
         50,  50,  50,  1, 0, 1,

        -50, -50, -50,  0, 1, 1,
         50, -50, -50,  0, 1, 1,
        -50,  50, -50,  0, 1, 1,
        -50,  50, -50,  0, 1, 1,
         50, -50, -50,  0, 1, 1,
         50,  50, -50,  0, 1, 1,

        -50, -50,  50,  0, 1, 1,
         50, -50,  50,  0, 1, 1,
        -50,  50,  50,  0, 1, 1,
        -50,  50,  50,  0, 1, 1,
         50, -50,  50,  0, 1, 1,
         50,  50,  50,  0, 1, 1
    };

    // Draw the cube
    glVertexPointer(3, GL_FLOAT, 6 * sizeof(GLfloat), cube);
    glColorPointer(3, GL_FLOAT, 6 * sizeof(GLfloat), cube + 3);
    glDrawArrays(GL_TRIANGLES, 0, 36);
}


////////////////////////////////////////////////////////////
/// Entry point of application
///
/// \return Error code
///
////////////////////////////////////////////////////////////
int main()
{
    // Open a connection with the X server
    Display* display = XOpenDisplay(NULL);
    if (!display)
        return EXIT_FAILURE;

    // Get the XCB connection for the opened display.
    xcb_connection_t* xcbConnection = XGetXCBConnection(display);

    if (!xcbConnection)
    {
        sf::err() << "Failed to get the XCB connection for opened display." << std::endl;
        return EXIT_FAILURE;
    }

    // Get XCB screen.
    const xcb_setup_t* xcbSetup = xcb_get_setup(xcbConnection);
    xcb_screen_iterator_t xcbScreenIter = xcb_setup_roots_iterator(xcbSetup);
    xcb_screen_t* screen = xcbScreenIter.data;

    if (!screen)
    {
        sf::err() << "Failed to get the XCB screen." << std::endl;
        return EXIT_FAILURE;
    }

    // Generate the XCB window IDs.
    xcb_window_t rootWindowId = xcb_generate_id(xcbConnection);
    xcb_window_t view1WindowId = xcb_generate_id(xcbConnection);
    xcb_window_t view2WindowId = xcb_generate_id(xcbConnection);

    // Create the root window with a black background.
    uint32_t mask = XCB_CW_BACK_PIXEL | XCB_CW_EVENT_MASK;
    uint32_t attributes[2] = {screen->black_pixel, XCB_EVENT_MASK_KEY_PRESS};

    xcb_create_window(xcbConnection,
                      XCB_COPY_FROM_PARENT,
                      rootWindowId,
                      screen->root,
                      0, 0, 650, 330,
                      0,
                      XCB_WINDOW_CLASS_INPUT_OUTPUT,
                      screen->root_visual,
                      mask, attributes);

    // Create windows for the SFML views.
    xcb_create_window(xcbConnection,
                      XCB_COPY_FROM_PARENT,
                      view1WindowId,
                      rootWindowId,
                      10, 10, 310, 310,
                      0,
                      XCB_WINDOW_CLASS_INPUT_OUTPUT,
                      screen->root_visual,
                      mask, attributes);

    xcb_create_window(xcbConnection,
                      XCB_COPY_FROM_PARENT,
                      view2WindowId,
                      rootWindowId,
                      330, 10, 310, 310,
                      0,
                      XCB_WINDOW_CLASS_INPUT_OUTPUT,
                      screen->root_visual,
                      mask, attributes);

    // Map windows to screen.
    xcb_map_window(xcbConnection, rootWindowId);
    xcb_map_window(xcbConnection, view1WindowId);
    xcb_map_window(xcbConnection, view2WindowId);

    // Flush commands.
    xcb_flush(xcbConnection);

    // Create our SFML views
    sf::Window sfmlView1(view1WindowId);
    sf::Window sfmlView2(view2WindowId);

    // Create a clock for measuring elapsed time
    sf::Clock clock;

    // Initialize our views
    initialize(sfmlView1);
    initialize(sfmlView2);

    // Start the event loop
    bool running = true;
    xcb_generic_event_t* event = NULL;

    while (running)
    {
        while ((event = xcb_poll_for_event(xcbConnection)))
        {
            running = false;
        }

        // Draw something into our views
        draw(sfmlView1, clock.getElapsedTime().asSeconds());
        draw(sfmlView2, clock.getElapsedTime().asSeconds() * 0.3f);

        // Display the views on screen
        sfmlView1.display();
        sfmlView2.display();
    }

    return EXIT_SUCCESS;
}
