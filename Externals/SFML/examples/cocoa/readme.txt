SFML IN COCOA APPLICATION
=========================

This is a small example of the integration of SFML in a Cocoa application.

Features
--------

 * This example shows how basic UI elements can interact with SFML
   render areas such as sf::RenderWindow (you can use sf::Window and
   OpenGL code too, of course).
 * It also provides tools for converting NSString to and from
   std::[w]string in an Objective-C Category of NSString.
 * Moreover, it shows how you can prevent annoying the system alerts
   produced when the SFML view has focus and the user press a key
   (see SilentWindow interface in CocoaAppDelegate.[h|mm]).

Special Considerations
----------------------

While mixing SFML into a Cocoa application you have to deal with mixing
C++ and Objective-C. In order to proceed you should use .mm extension for
Objective-C++ files.

Be aware of the limitations of Objective-C++. Please refer to the official
documentation provided by Apple for more information.

You can also work around these limitations by using CSFML.
