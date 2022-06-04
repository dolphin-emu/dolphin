"""
Module for doing stuff with the graphical user interface.

All colors are in ARGB.
All positions are (x, y) with (0, 0) being top left. X is the horizontal axis.
"""

Position = tuple[float, float]


def add_osd_message(message: str, duration_ms: int = 2000, color: int = 0xFFFFFF30):
    """
    Adds a new message to the on-screen-display.

    :param message: message to agg
    :param duration_ms: how long the message should be visible for, in milliseconds
    :param color: color of the message text as an int
    """


def clear_osd_messages():
    """
    Clear all on-screen-display messages.
    """


def get_display_size() -> tuple[float, float]:
    """
    :return: The current display size in pixels.
    """


def draw_line(a: Position, b: Position, color: int, thickness: float = 1):
    """
    Draws a line from a to b
    """


def draw_rect(a: Position, b: Position, color: int, rounding: float = 0, thickness: float = 1):
    """
    Draws a hollow rectangle from a (upper left) to b (lower right)
    """


def draw_rect_filled(a: Position, b: Position, color: int, rounding: float = 0):
    """
    Draws a filled rectangle from a (upper left) to b (lower right)
    """


def draw_quad(a: Position, b: Position, c: Position, d: Position, color: int, thickness: float = 1):
    """
    Draws a hollow quad through the points a, b, c and d.
    Points should be defined in clockwise order.
    """


def draw_quad_filled(a: Position, b: Position, c: Position, d: Position, color: int):
    """
    Draws a filled quad through the points a, b, c and d.
    """


def draw_triangle(a: Position, b: Position, c: Position, color: int, thickness: float = 1):
    """
    Draws a hollow triangle through the points a, b and c.
    """


def draw_triangle_filled(a: Position, b: Position, c: Position, color: int):
    """
    Draws a filled triangle through the points a, b and c.
    """


def draw_circle(center: Position, radius: float, color: int, num_segments: int = None, thickness: float = 1):
    """
    Draws a hollow circle with the given center point and radius.
    If num_segments is set to None (default), a sensible default is used.
    """


def draw_circle_filled(center: Position, radius: float, color: int, num_segments: int = None):
    """
    Draws a filled circle with the given center point and radius.
    If num_segments is set to None (default), a sensible default is used.
    """


def draw_text(pos: Position, color: int, text: str):
    """
    Draws text at a fixed position.
    """


def draw_polyline(points: list[Position], color: int, closed: bool = False, thickness: float = 1):
    """
    Draws a line through a list of points
    """


def draw_convex_poly_filled(points: list[Position], color: int):
    """
    Draws a convex polygon through a list of points.
    Points should be defined in clockwise order.
    """
