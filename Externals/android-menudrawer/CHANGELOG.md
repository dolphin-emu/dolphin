Change Log
==========

Version 2.0.2 *(2013-03-31)*
----------------------------
 * Added listener that makes it possible to disabllow intercepting touch events over
   certain views
 * Added setter for the maximum animation duration
 * Added getter for menu size
 * Added methods that enable/disable indicator animation
 * Fix: Removed log statements
 * Fix: Drawing the active indicator might cause crash if the active view is not a
        child of the MenuDrawer
 * Fix: Crash in static drawer if no active indicator bitmap was set

Version 2.0.1 *(2013-02-12)*
----------------------------
 * Indicator now animates between active views
 * Fixed restoring state for right/bottom drawer

Version 2.0.0 *(2013-01-23)*
----------------------------

 * Major API changes

    * All classes are now in the net.simonvt.menudrawer package.
    * MenuDrawerManager no longet exists. Menu is added with MenuDrawer#attach(...).
    * Drawer position is now selected with Position enums instead of int constants.
    * Width methods/attributes have been renamed to 'size'.

 * Added top/bottom drawer.
 * Added static (non-draggable, always visible) drawers.
 * The touch bezel size is now configurable with MenuDrawer#setTouchBezelSize(int).
 * MenuDrawer#saveState() now only required when dragging the entire window.
 * Drawers can now be used in XML layouts.
 * Fix: Scroller class caused conflicts with other libraries.
 * Fix: No more overdraw when the drawer is closed.
 * Fix: Content no longer falls behind when slowly dragging.


Version 1.0.0 *(2012-10-30)*
----------------------------

Initial release.
