// Copyright (C) 2013 Scott Moreau <oreaus@gmail.com>

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, version 2.0.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License 2.0 for more details.

// A copy of the GPL 2.0 should have been included with the program.
// If not, see http://www.gnu.org/licenses/

// Official SVN repository and contact information can be found at
// http://code.google.com/p/dolphin-emu/

#include "Core.h"
#include "State.h"
#include "../GLInterface.h"
#include <linux/input.h>
#include <sys/mman.h>


static void
redraw(void *data, struct wl_callback *callback, uint32_t time);

static const struct wl_callback_listener frame_listener = {
	redraw
};

static void
redraw(void *data, struct wl_callback *callback, uint32_t time)
{
	if (GLWin.wl_callback != callback) {
		printf("Got wrong callback from wayland server\n");
		exit(-1);
	}

	GLWin.wl_callback = NULL;

	if (callback)
		wl_callback_destroy(callback);

	if (!GLWin.configured)
		return;

	// Reset the frame callback
	GLWin.wl_callback = wl_surface_frame(GLWin.wl_surface);
	wl_callback_add_listener(GLWin.wl_callback, &frame_listener, 0);

	// Present rendered buffer on screen
	//eglSwapBuffers(GLWin.egl_dpy, GLWin.egl_surf);
}

static void
hide_cursor(void)
{
	if (!GLWin.pointer.wl_pointer)
		return;

	wl_pointer_set_cursor(GLWin.pointer.wl_pointer,
			      GLWin.pointer.serial, NULL, 0, 0);
}

static void
configure_callback(void *data, struct wl_callback *callback, uint32_t  time)
{
	wl_callback_destroy(callback);

	GLWin.configured = true;

	if (GLWin.wl_callback == NULL)
		redraw(data, NULL, time);
}

static struct wl_callback_listener configure_callback_listener = {
	configure_callback,
};

static void
handle_ping(void *data, struct wl_shell_surface *wl_shell_surface,
	    uint32_t serial)
{
	wl_shell_surface_pong(wl_shell_surface, serial);
}

static void
handle_configure(void *data, struct wl_shell_surface *wl_shell_surface,
		 uint32_t edges, int32_t width, int32_t height)
{

	if (GLWin.wl_egl_native)
		wl_egl_window_resize(GLWin.wl_egl_native, width, height, 0, 0);

	GLWin.geometry.width = width;
	GLWin.geometry.height = height;

	GLInterface->SetBackBufferDimensions(width, height);

	if (!GLWin.fullscreen)
		GLWin.window_size = GLWin.geometry;
}

static void
handle_popup_done(void *data, struct wl_shell_surface *wl_shell_surface)
{
}

static const struct wl_shell_surface_listener shell_surface_listener = {
	handle_ping,
	handle_configure,
	handle_popup_done
};

static void
pointer_handle_enter(void *data, struct wl_pointer *pointer,
		     uint32_t serial, struct wl_surface *surface,
		     wl_fixed_t sx, wl_fixed_t sy)
{
	GLWin.pointer.serial = serial;

	if (GLWin.fullscreen)
		hide_cursor();
}

static void
pointer_handle_leave(void *data, struct wl_pointer *pointer,
		     uint32_t serial, struct wl_surface *surface)
{
}

static void
pointer_handle_motion(void *data, struct wl_pointer *pointer,
		      uint32_t time, wl_fixed_t sx, wl_fixed_t sy)
{
}

static void
pointer_handle_button(void *data, struct wl_pointer *wl_pointer,
		      uint32_t serial, uint32_t time, uint32_t button,
		      uint32_t state)
{
}

static void
pointer_handle_axis(void *data, struct wl_pointer *wl_pointer,
		    uint32_t time, uint32_t axis, wl_fixed_t value)
{
}

static const struct wl_pointer_listener pointer_listener = {
	pointer_handle_enter,
	pointer_handle_leave,
	pointer_handle_motion,
	pointer_handle_button,
	pointer_handle_axis,
};

void setup_callback_listener()
{
	struct wl_callback *callback;

	callback = wl_display_sync(GLWin.wl_display);
	wl_callback_add_listener(callback, &configure_callback_listener, 0);
}

static void
toggle_fullscreen(bool fullscreen)
{
	GLWin.fullscreen = fullscreen;
	GLWin.configured = false;

	if (fullscreen) {
		wl_shell_surface_set_fullscreen(GLWin.wl_shell_surface,
						WL_SHELL_SURFACE_FULLSCREEN_METHOD_DEFAULT,
						0, NULL);
	} else {
		wl_shell_surface_set_toplevel(GLWin.wl_shell_surface);
		handle_configure(NULL, GLWin.wl_shell_surface, 0,
				 GLWin.window_size.width,
				 GLWin.window_size.height);
	}

	setup_callback_listener();
}

static void
keyboard_handle_keymap(void *data, struct wl_keyboard *keyboard,
		       uint32_t format, int fd, uint32_t size)
{
	char *map_str;

	if (format != WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1) {
		close(fd);
		return;
	}

	map_str = (char *) mmap(NULL, size, PROT_READ, MAP_SHARED, fd, 0);
	if (map_str == MAP_FAILED) {
		close(fd);
		return;
	}

	GLWin.keyboard.xkb.keymap = xkb_map_new_from_string(GLWin.keyboard.xkb.context,
						    map_str,
						    XKB_KEYMAP_FORMAT_TEXT_V1,
						    (xkb_keymap_compile_flags) 0);
	munmap(map_str, size);
	close(fd);

	if (!GLWin.keyboard.xkb.keymap) {
		fprintf(stderr, "failed to compile keymap\n");
		return;
	}

	GLWin.keyboard.xkb.state = xkb_state_new(GLWin.keyboard.xkb.keymap);
	if (!GLWin.keyboard.xkb.state) {
		fprintf(stderr, "failed to create XKB state\n");
		xkb_map_unref(GLWin.keyboard.xkb.keymap);
		GLWin.keyboard.xkb.keymap = NULL;
		return;
	}

	GLWin.keyboard.xkb.control_mask =
		1 << xkb_map_mod_get_index(GLWin.keyboard.xkb.keymap, "Control");
	GLWin.keyboard.xkb.alt_mask =
		1 << xkb_map_mod_get_index(GLWin.keyboard.xkb.keymap, "Mod1");
	GLWin.keyboard.xkb.shift_mask =
		1 << xkb_map_mod_get_index(GLWin.keyboard.xkb.keymap, "Shift");
}

static void
keyboard_handle_enter(void *data, struct wl_keyboard *keyboard,
		      uint32_t serial, struct wl_surface *surface,
		      struct wl_array *keys)
{
}

static void
keyboard_handle_leave(void *data, struct wl_keyboard *keyboard,
		      uint32_t serial, struct wl_surface *surface)
{
}

static void
keyboard_handle_key(void *data, struct wl_keyboard *keyboard,
		    uint32_t serial, uint32_t time, uint32_t key,
		    uint32_t state)
{
	if (state == WL_KEYBOARD_KEY_STATE_RELEASED)
		return;

	if (key == KEY_ESC)
		GLWin.running = false;
	else if ((key == KEY_P) ||
		((key == KEY_ENTER) && (GLWin.keyboard.modifiers == 0)))
		Core::SetState((Core::GetState() == Core::CORE_RUN) ?
				Core::CORE_PAUSE : Core::CORE_RUN);
	else if (key == KEY_F)
		toggle_fullscreen(!GLWin.fullscreen);
	else if ((key == KEY_ENTER) && (GLWin.keyboard.modifiers == MOD_ALT_MASK))
		toggle_fullscreen(!GLWin.fullscreen);
	else if (key >= KEY_F1 && key <= KEY_F8) {
		int slot_number = key - KEY_F1 + 1;
		if (GLWin.keyboard.modifiers == MOD_SHIFT_MASK)
			State::Save(slot_number);
		else
			State::Load(slot_number);
	}
	else if (key == KEY_F9)
		Core::SaveScreenShot();
	else if (key == KEY_F11)
		State::LoadLastSaved();
	else if (key == KEY_F12) {
		if (GLWin.keyboard.modifiers == MOD_SHIFT_MASK)
			State::UndoLoadState();
		else
			State::UndoSaveState();
	}
}

static void
keyboard_handle_modifiers(void *data, struct wl_keyboard *keyboard,
			  uint32_t serial, uint32_t mods_depressed,
			  uint32_t mods_latched, uint32_t mods_locked,
			  uint32_t group)
{
	xkb_mod_mask_t mask;

	xkb_state_update_mask(GLWin.keyboard.xkb.state, mods_depressed, mods_latched,
			      mods_locked, 0, 0, group);
	mask = xkb_state_serialize_mods(GLWin.keyboard.xkb.state,
					(xkb_state_component)
					(XKB_STATE_DEPRESSED |
					 XKB_STATE_LATCHED));
	GLWin.keyboard.modifiers = 0;
	if (mask & GLWin.keyboard.xkb.control_mask)
		GLWin.keyboard.modifiers |= MOD_CONTROL_MASK;
	if (mask & GLWin.keyboard.xkb.alt_mask)
		GLWin.keyboard.modifiers |= MOD_ALT_MASK;
	if (mask & GLWin.keyboard.xkb.shift_mask)
		GLWin.keyboard.modifiers |= MOD_SHIFT_MASK;
}

static const struct wl_keyboard_listener keyboard_listener = {
	keyboard_handle_keymap,
	keyboard_handle_enter,
	keyboard_handle_leave,
	keyboard_handle_key,
	keyboard_handle_modifiers,
};

static void
seat_handle_capabilities(void *data, struct wl_seat *seat,
			 uint32_t caps)
{
	struct wl_pointer *wl_pointer = NULL;
	struct wl_keyboard *wl_keyboard = NULL;

	if ((caps & WL_SEAT_CAPABILITY_POINTER) && !wl_pointer) {
		wl_pointer = wl_seat_get_pointer(seat);
		wl_pointer_add_listener(wl_pointer, &pointer_listener, 0);
	} else if (!(caps & WL_SEAT_CAPABILITY_POINTER) && wl_pointer) {
		wl_pointer_destroy(wl_pointer);
		wl_pointer = NULL;
	}

	GLWin.pointer.wl_pointer = wl_pointer;

	if ((caps & WL_SEAT_CAPABILITY_KEYBOARD) && !wl_keyboard) {
		wl_keyboard = wl_seat_get_keyboard(seat);
		wl_keyboard_add_listener(wl_keyboard, &keyboard_listener, 0);
	} else if (!(caps & WL_SEAT_CAPABILITY_KEYBOARD) && wl_keyboard) {
		wl_keyboard_destroy(wl_keyboard);
		wl_keyboard = NULL;
	}

	GLWin.keyboard.wl_keyboard = wl_keyboard;
}

static const struct wl_seat_listener seat_listener = {
	seat_handle_capabilities,
};

static void
registry_handle_global(void *data, struct wl_registry *registry,
		       uint32_t name, const char *interface, uint32_t version)
{
	if (strcmp(interface, "wl_compositor") == 0) {
		GLWin.wl_compositor = (wl_compositor *)
			wl_registry_bind(registry, name,
					 &wl_compositor_interface, 1);
	} else if (strcmp(interface, "wl_shell") == 0) {
		GLWin.wl_shell = (wl_shell *) wl_registry_bind(registry, name,
					    &wl_shell_interface, 1);
	} else if (strcmp(interface, "wl_seat") == 0) {
		GLWin.wl_seat = (wl_seat *) wl_registry_bind(registry, name,
					   &wl_seat_interface, 1);
		wl_seat_add_listener(GLWin.wl_seat, &seat_listener, 0);
	} else if (strcmp(interface, "wl_shm") == 0) {
		GLWin.wl_shm = (wl_shm *) wl_registry_bind(registry, name,
					  &wl_shm_interface, 1);
		GLWin.wl_cursor_theme = (wl_cursor_theme *) wl_cursor_theme_load(NULL, 32, GLWin.wl_shm);
		GLWin.wl_cursor = (wl_cursor *)
			wl_cursor_theme_get_cursor(GLWin.wl_cursor_theme, "left_ptr");
	}
}

static void
registry_handle_global_remove(void *data, struct wl_registry *registry,
			      uint32_t name)
{
}

static const struct wl_registry_listener registry_listener = {
	registry_handle_global,
	registry_handle_global_remove
};

bool cWaylandInterface::ServerConnect(void)
{
	GLWin.wl_display = wl_display_connect(NULL);

	if (!GLWin.wl_display)
		return false;

	return true;
}

bool cWaylandInterface::Initialize(void *config)
{
	if (!GLWin.wl_display) {
		printf("Error: couldn't open wayland display\n");
		return false;
	}

	GLWin.pointer.wl_pointer = NULL;
	GLWin.keyboard.wl_keyboard = NULL;

	GLWin.keyboard.xkb.context = xkb_context_new((xkb_context_flags) 0);
	if (GLWin.keyboard.xkb.context == NULL) {
		fprintf(stderr, "Failed to create XKB context\n");
		return NULL;
	}

	GLWin.wl_registry = wl_display_get_registry(GLWin.wl_display);
	wl_registry_add_listener(GLWin.wl_registry,
				 &registry_listener, NULL);

	wl_display_dispatch(GLWin.wl_display);

	GLWin.wl_cursor_surface =
		wl_compositor_create_surface(GLWin.wl_compositor);

	return true;
}

void *cWaylandInterface::EGLGetDisplay(void)
{
#if HAVE_X11
	return eglGetDisplay((_XDisplay *) GLWin.wl_display);
#else
	return eglGetDisplay(GLWin.wl_display);
#endif
}

void *cWaylandInterface::CreateWindow(void)
{
	GLWin.window_size.width = 640;
	GLWin.window_size.height = 480;
	GLWin.fullscreen = true;
	GLWin.frame_drawn = false;
	GLWin.swap_complete = false;

	GLWin.wl_surface = wl_compositor_create_surface(GLWin.wl_compositor);
	GLWin.wl_shell_surface = wl_shell_get_shell_surface(GLWin.wl_shell,
							   GLWin.wl_surface);

	wl_shell_surface_add_listener(GLWin.wl_shell_surface,
				      &shell_surface_listener, 0);

	GLWin.wl_egl_native = wl_egl_window_create(GLWin.wl_surface,
						   GLWin.window_size.width,
						   GLWin.window_size.height);
#if HAVE_X11
	return GLWin.wl_egl_native;
#else
	return GLWin.wl_egl_native;
#endif
}

void cWaylandInterface::DestroyWindow(void)
{
	wl_egl_window_destroy(GLWin.wl_egl_native);

	wl_shell_surface_destroy(GLWin.wl_shell_surface);
	wl_surface_destroy(GLWin.wl_surface);

	if (GLWin.wl_callback)
		wl_callback_destroy(GLWin.wl_callback);
}

void cWaylandInterface::UpdateFPSDisplay(const char *text)
{
	wl_shell_surface_set_title(GLWin.wl_shell_surface, text);
}

void cWaylandInterface::ToggleFullscreen(bool fullscreen)
{
	toggle_fullscreen(GLWin.fullscreen);
}
