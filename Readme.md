This is a modified dolphin build. For information on Dolphin please check the original documentation at the links below.
# Dolphin - A GameCube and Wii Emulator

[Homepage](https://dolphin-emu.org/) | [Project/Github Site](https://github.com/dolphin-emu/dolphin) | [Buildbot](https://dolphin.ci/) | [Forums](https://forums.dolphin-emu.org/) | [Wiki](https://wiki.dolphin-emu.org/) | [GitHub Wiki](https://github.com/dolphin-emu/dolphin/wiki) | [Issue Tracker](https://bugs.dolphin-emu.org/projects/emulator/issues) | [Coding Style](https://github.com/dolphin-emu/dolphin/blob/master/Contributing.md) | [Transifex Page](https://app.transifex.com/delroth/dolphin-emu/dashboard/)

Dolphin is an emulator for running GameCube and Wii games on Windows,
Linux, macOS, and recent Android devices. It's licensed under the terms
of the GNU General Public License, version 2 or later (GPLv2+).

Please read the [FAQ](https://dolphin-emu.org/docs/faq/) before using Dolphin.

## EFB Slider

This build provides new graphic options to fix poor quality bloom on Dolphin games. Bloom is created from an image or texture that is blurred until it becomes a cloud/halo of color (like something out of focus in a camera). The problem with bloom is that it is supposed to average out the bloom color/strength over a large area of the screen. The area is effectively determined with the number of pixels, and when the Internal Resolution is increased, the number of pixels increases.  If the game expects 10 pixels to be 1% of the screen, but you double the number of pixels, 10 pixels becomes 0.5% of the screen.  This results in the bloom being averaged over a smaller portion of the screen, and the original image becomes sharper (more in focus). Instead of seeing bloom you see something with a similar sturcture to the original image used to create the bloom.  

In order to fix this, bloom can either be kept at native resolution or it can be averaged/blurred over a larger area of the screen.  If kept at native resolution some games will experience a shimmer effect, possibly caused by bloom snapping to 1 pixel offsets on a small resolution, then being upscaled to a large resolution. Using the method to average out the bloom fixes this problem.  Averaging/blurring may not produce accurate results for some games, but may also increase the graphical quality of other games.

This description isn't 100% technically correct, but is meant to convey the idea of the issue and the solution as simply as possible.
