////////////////////////////////////////////////////////////
/// \mainpage
///
/// \section welcome Welcome
/// Welcome to the official SFML documentation. Here you will find a detailed
/// view of all the SFML <a href="./annotated.php">classes</a> and functions. <br/>
/// If you are looking for tutorials, you can visit the official website
/// at <a href="http://www.sfml-dev.org/">www.sfml-dev.org</a>.
///
/// \section example Short example
/// Here is a short example, to show you how simple it is to use SFML:
///
/// \code
///
/// #include <SFML/Audio.hpp>
/// #include <SFML/Graphics.hpp>
///
/// int main()
/// {
///     // Create the main window
///     sf::RenderWindow window(sf::VideoMode(800, 600), "SFML window");
///
///     // Load a sprite to display
///     sf::Texture texture;
///     if (!texture.loadFromFile("cute_image.jpg"))
///         return EXIT_FAILURE;
///     sf::Sprite sprite(texture);
///
///     // Create a graphical text to display
///     sf::Font font;
///     if (!font.loadFromFile("arial.ttf"))
///         return EXIT_FAILURE;
///     sf::Text text("Hello SFML", font, 50);
///
///     // Load a music to play
///     sf::Music music;
///     if (!music.openFromFile("nice_music.ogg"))
///         return EXIT_FAILURE;
///
///     // Play the music
///     music.play();
///
///     // Start the game loop
///     while (window.isOpen())
///     {
///         // Process events
///         sf::Event event;
///         while (window.pollEvent(event))
///         {
///             // Close window: exit
///             if (event.type == sf::Event::Closed)
///                 window.close();
///         }
///
///         // Clear screen
///         window.clear();
///
///         // Draw the sprite
///         window.draw(sprite);
///
///         // Draw the string
///         window.draw(text);
///
///         // Update the window
///         window.display();
///     }
///
///     return EXIT_SUCCESS;
/// }
/// \endcode
////////////////////////////////////////////////////////////
