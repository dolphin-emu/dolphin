# Bundled GameCube input-display overlay. Driven by the native InputDisplayController,
# never by the Scripts window. Live input rendering and skins arrive in a later phase.

from dolphin import gui

WIDTH = 320
HEIGHT = 240
BG = 0xCC101010

cv = gui.canvas("Input Display", WIDTH, HEIGHT, overlay=True)
cv.clear()
cv.rect_filled((0, 0), (WIDTH, HEIGHT), BG)
cv.commit()
