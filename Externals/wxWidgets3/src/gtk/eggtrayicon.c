/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* eggtrayicon.c
 * Copyright (C) 2002 Anders Carlsson <andersca@gnu.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/*

Permission to use this file under wxWindows licence given by
copyright holder:
--------
From andersca@gnu.org Tue Dec  9 13:01:56 2003
Return-path: <andersca@gnu.org>
Envelope-to: vasek@localhost
Delivery-date: Tue, 09 Dec 2003 13:04:35 +0100
Received: from localhost
	([127.0.0.1] helo=amavis ident=amavis)
	by armitage with esmtp (Exim 3.35 #1)
	id 1ATgbS-0001Gs-00
	for vasek@localhost; Tue, 09 Dec 2003 13:04:35 +0100
Received: from armitage ([127.0.0.1])
	by amavis (armitage [127.0.0.1]) (amavisd-new, port 10024) with ESMTP
	id 04227-09 for <vasek@localhost>;
	Tue, 9 Dec 2003 13:04:11 +0100 (CET)
Received: from localhost ([127.0.0.1] ident=fetchmail)
	by armitage with esmtp (Exim 3.35 #1)
	id 1ATgb5-0001GY-00
	for vasek@localhost; Tue, 09 Dec 2003 13:04:11 +0100
Delivered-To: alias-email-slavikvaclav@seznam.cz
Received: from pop3.seznam.cz [212.80.76.45]
	by localhost with POP3 (fetchmail-5.9.11)
	for vasek@localhost (single-drop); Tue, 09 Dec 2003 13:04:11 +0100 (CET)
Received: (qmail 9861 invoked from network); 9 Dec 2003 12:02:17 -0000
Received: from unknown (HELO maxipes.logix.cz) (81.0.234.97)
  by buster.go.seznam.cz with SMTP; 9 Dec 2003 12:02:17 -0000
Received: by maxipes.logix.cz (Postfix, from userid 604)
	id 37E6D29A51; Tue,  9 Dec 2003 13:02:16 +0100 (CET)
X-Original-To: vaclav.slavik@matfyz.cz
Received: from mail.csbnet.se (glutus.csbnet.se [193.11.248.2])
	by maxipes.logix.cz (Postfix) with ESMTP id 90D6A29A51
	for <vaclav.slavik@matfyz.cz>; Tue,  9 Dec 2003 13:02:15 +0100 (CET)
Received: by mail.csbnet.se (Postfix, from userid 8)
	id 7AA7F10A6D7; Tue,  9 Dec 2003 13:02:14 +0100 (CET)
Received: from carbon.csbnet.se (carbon.csbnet.se [193.11.248.180])
	by mail.csbnet.se (Postfix) with ESMTP id A190F10A71D
	for <vaclav.slavik@matfyz.cz>; Tue,  9 Dec 2003 13:01:56 +0100 (CET)
Subject: Re: eggtrayicon.{c,h} licensing
From: Anders Carlsson <andersca@gnu.org>
To: Vaclav Slavik <vaclav.slavik@matfyz.cz>
In-Reply-To: <200312091142.54542.vaclav.slavik@matfyz.cz>
References: <200312091142.54542.vaclav.slavik@matfyz.cz>
Content-Type: text/plain
Message-Id: <1070971316.30989.0.camel@carbon.csbnet.se>
Mime-Version: 1.0
X-Mailer: Ximian Evolution 1.5 
Date: Tue, 09 Dec 2003 13:01:56 +0100
Content-Transfer-Encoding: 7bit
X-Scanned-By: CLAM (openantivirus DB) antivirus scanner at mail.csbnet.se
X-Virus-Scanned: by amavisd-new-20030616-p5 (Debian) at armitage
X-Spam-Status: No, hits=-4.9 tagged_above=-999.0 required=6.3 tests=BAYES_00
X-Spam-Level: 
Status: R 
X-Status: N
X-KMail-EncryptionState:  
X-KMail-SignatureState:  

On tis, 2003-12-09 at 11:42 +0100, Vaclav Slavik wrote:
> Hi,
> 
> I'm working on the wxWindows cross-platform GUI toolkit
> (http://www.wxwindows.org) which uses GTK+ and it would save me a lot
> of time if I could use your eggtrayicon code to implement tray icons
> on X11. Unfortunately I can't use it right now because it is not part
> of any library we could depend on (as we do depend on GTK+) and would
> have to be included in our sources and it is under the LGPL license.
> The problem is that wxWindows' license is more permissive (see
> http://www.opensource.org/licenses/wxwindows.php for details) and so
> I can't take your code and put it under wxWindows License. And I
> can't put code that can't be used under the terms of wxWindows
> License into wxWindows either. Do you think it would be possible to
> get permission to include eggtrayicon under wxWindows license?
>
> Thanks,
> Vaclav
>

Sure, that's fine by me.

Anders
--------
*/


#include "wx/platform.h"

#if wxUSE_TASKBARICON

#include <gtk/gtk.h>
#if GTK_CHECK_VERSION(2, 1, 0)

#include <string.h>
#include "eggtrayicon.h"

#include <gdkconfig.h>
#if defined (GDK_WINDOWING_X11)
#include <gdk/gdkx.h>
#include <X11/Xatom.h>
#elif defined (GDK_WINDOWING_WIN32)
#include <gdk/gdkwin32.h>
#endif


#define SYSTEM_TRAY_REQUEST_DOCK    0
#define SYSTEM_TRAY_BEGIN_MESSAGE   1
#define SYSTEM_TRAY_CANCEL_MESSAGE  2

#define SYSTEM_TRAY_ORIENTATION_HORZ 0
#define SYSTEM_TRAY_ORIENTATION_VERT 1

enum {
  PROP_0,
  PROP_ORIENTATION
};

static GtkPlugClass *parent_class = NULL;

static void egg_tray_icon_init (EggTrayIcon *icon);
static void egg_tray_icon_class_init (EggTrayIconClass *klass);

static void egg_tray_icon_get_property (GObject    *object,
					guint       prop_id,
					GValue     *value,
					GParamSpec *pspec);

static void egg_tray_icon_realize   (GtkWidget *widget);
static void egg_tray_icon_unrealize (GtkWidget *widget);

static void egg_tray_icon_add (GtkContainer *container,
			       GtkWidget    *widget);

#ifdef GDK_WINDOWING_X11
static void egg_tray_icon_update_manager_window    (EggTrayIcon *icon,
						    gboolean     dock_if_realized);
static void egg_tray_icon_manager_window_destroyed (EggTrayIcon *icon);
#endif

GType
egg_tray_icon_get_type (void)
{
  static GType our_type = 0;

  if (our_type == 0)
    {
      static const GTypeInfo our_info =
      {
	sizeof (EggTrayIconClass),
	(GBaseInitFunc) NULL,
	(GBaseFinalizeFunc) NULL,
	(GClassInitFunc) egg_tray_icon_class_init,
	NULL, /* class_finalize */
	NULL, /* class_data */
	sizeof (EggTrayIcon),
	0,    /* n_preallocs */
	(GInstanceInitFunc) egg_tray_icon_init
      };

      our_type = g_type_register_static (GTK_TYPE_PLUG, "EggTrayIcon", &our_info, 0);
    }

  return our_type;
}

static void
egg_tray_icon_init (EggTrayIcon *icon)
{
  icon->stamp = 1;
  icon->orientation = GTK_ORIENTATION_HORIZONTAL;

  gtk_widget_add_events (GTK_WIDGET (icon), GDK_PROPERTY_CHANGE_MASK);
}

static void
egg_tray_icon_class_init (EggTrayIconClass *klass)
{
  GObjectClass *gobject_class = (GObjectClass *)klass;
  GtkWidgetClass *widget_class = (GtkWidgetClass *)klass;
  GtkContainerClass *container_class = (GtkContainerClass *)klass;

  parent_class = g_type_class_peek_parent (klass);

  gobject_class->get_property = egg_tray_icon_get_property;

  widget_class->realize   = egg_tray_icon_realize;
  widget_class->unrealize = egg_tray_icon_unrealize;

  container_class->add = egg_tray_icon_add;

  g_object_class_install_property (gobject_class,
				   PROP_ORIENTATION,
				   g_param_spec_enum ("orientation",
						      "Orientation",
						      "The orientation of the tray.",
						      GTK_TYPE_ORIENTATION,
						      GTK_ORIENTATION_HORIZONTAL,
						      G_PARAM_READABLE));
}

static void
egg_tray_icon_get_property (GObject    *object,
			    guint       prop_id,
			    GValue     *value,
			    GParamSpec *pspec)
{
  EggTrayIcon *icon = EGG_TRAY_ICON (object);

  switch (prop_id)
    {
    case PROP_ORIENTATION:
      g_value_set_enum (value, icon->orientation);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

#ifdef GDK_WINDOWING_X11

static void
egg_tray_icon_get_orientation_property (EggTrayIcon *icon)
{
  Display *xdisplay;
  Atom type;
  int format;
  union {
	gulong *prop;
	guchar *prop_ch;
  } prop = { NULL };
  gulong nitems;
  gulong bytes_after;
  int error, result;

  g_assert (icon->manager_window != None);

  xdisplay = GDK_DISPLAY_XDISPLAY (gtk_widget_get_display (GTK_WIDGET (icon)));

  gdk_error_trap_push ();
  type = None;
  result = XGetWindowProperty (xdisplay,
			       icon->manager_window,
			       icon->orientation_atom,
			       0, G_MAXLONG, FALSE,
			       XA_CARDINAL,
			       &type, &format, &nitems,
			       &bytes_after, &(prop.prop_ch));
  error = gdk_error_trap_pop ();

  if (error || result != Success)
    return;

  if (type == XA_CARDINAL)
    {
      GtkOrientation orientation;

      orientation = (prop.prop [0] == SYSTEM_TRAY_ORIENTATION_HORZ) ?
					GTK_ORIENTATION_HORIZONTAL :
					GTK_ORIENTATION_VERTICAL;

      if (icon->orientation != orientation)
	{
	  icon->orientation = orientation;

	  g_object_notify (G_OBJECT (icon), "orientation");
	}
    }

  if (prop.prop)
    XFree (prop.prop);
}

static GdkFilterReturn
egg_tray_icon_manager_filter (GdkXEvent *xevent, GdkEvent *event, gpointer user_data)
{
  EggTrayIcon *icon = user_data;
  XEvent *xev = (XEvent *)xevent;

  if (xev->xany.type == ClientMessage &&
      xev->xclient.message_type == icon->manager_atom &&
      xev->xclient.data.l[1] == icon->selection_atom)
    {
      egg_tray_icon_update_manager_window (icon, TRUE);
    }
  else if (xev->xany.window == icon->manager_window)
    {
      if (xev->xany.type == PropertyNotify &&
	  xev->xproperty.atom == icon->orientation_atom)
	{
	  egg_tray_icon_get_orientation_property (icon);
	}
      if (xev->xany.type == DestroyNotify)
	{
	  egg_tray_icon_manager_window_destroyed (icon);
	}
    }
  return GDK_FILTER_CONTINUE;
}

#endif

static void
egg_tray_icon_unrealize (GtkWidget *widget)
{
#ifdef GDK_WINDOWING_X11
  EggTrayIcon *icon = EGG_TRAY_ICON (widget);
  GdkWindow *root_window;

  if (icon->manager_window != None)
    {
      GdkWindow *gdkwin;

      gdkwin = gdk_window_lookup_for_display (gtk_widget_get_display (widget),
                                              icon->manager_window);

      gdk_window_remove_filter (gdkwin, egg_tray_icon_manager_filter, icon);
    }

  root_window = gdk_screen_get_root_window (gtk_widget_get_screen (widget));

  gdk_window_remove_filter (root_window, egg_tray_icon_manager_filter, icon);

  if (GTK_WIDGET_CLASS (parent_class)->unrealize)
    (* GTK_WIDGET_CLASS (parent_class)->unrealize) (widget);
#endif
}

#ifdef GDK_WINDOWING_X11

static void
egg_tray_icon_send_manager_message (EggTrayIcon *icon,
				    long         message,
				    Window       window,
				    long         data1,
				    long         data2,
				    long         data3)
{
  XClientMessageEvent ev;
  Display *display;

  ev.type = ClientMessage;
  ev.window = window;
  ev.message_type = icon->system_tray_opcode_atom;
  ev.format = 32;
  ev.data.l[0] = gdk_x11_get_server_time (GTK_WIDGET (icon)->window);
  ev.data.l[1] = message;
  ev.data.l[2] = data1;
  ev.data.l[3] = data2;
  ev.data.l[4] = data3;

  display = GDK_DISPLAY_XDISPLAY (gtk_widget_get_display (GTK_WIDGET (icon)));

  gdk_error_trap_push ();
  XSendEvent (display,
	      icon->manager_window, False, NoEventMask, (XEvent *)&ev);
  XSync (display, False);
  gdk_error_trap_pop ();
}

static void
egg_tray_icon_send_dock_request (EggTrayIcon *icon)
{
  egg_tray_icon_send_manager_message (icon,
				      SYSTEM_TRAY_REQUEST_DOCK,
				      icon->manager_window,
				      gtk_plug_get_id (GTK_PLUG (icon)),
				      0, 0);
}

static void
egg_tray_icon_update_manager_window (EggTrayIcon *icon,
				     gboolean     dock_if_realized)
{
  Display *xdisplay;

  if (icon->manager_window != None)
    return;

  xdisplay = GDK_DISPLAY_XDISPLAY (gtk_widget_get_display (GTK_WIDGET (icon)));

  XGrabServer (xdisplay);

  icon->manager_window = XGetSelectionOwner (xdisplay,
					     icon->selection_atom);

  if (icon->manager_window != None)
    XSelectInput (xdisplay,
		  icon->manager_window, StructureNotifyMask|PropertyChangeMask);

  XUngrabServer (xdisplay);
  XFlush (xdisplay);

  if (icon->manager_window != None)
    {
      GdkWindow *gdkwin;

      gdkwin = gdk_window_lookup_for_display (gtk_widget_get_display (GTK_WIDGET (icon)),
					      icon->manager_window);

      gdk_window_add_filter (gdkwin, egg_tray_icon_manager_filter, icon);

      if (dock_if_realized && GTK_WIDGET_REALIZED (icon))
	egg_tray_icon_send_dock_request (icon);

      egg_tray_icon_get_orientation_property (icon);
    }
}

static void
egg_tray_icon_manager_window_destroyed (EggTrayIcon *icon)
{
  GdkWindow *gdkwin;

  g_return_if_fail (icon->manager_window != None);

  gdkwin = gdk_window_lookup_for_display (gtk_widget_get_display (GTK_WIDGET (icon)),
					  icon->manager_window);

  gdk_window_remove_filter (gdkwin, egg_tray_icon_manager_filter, icon);

  icon->manager_window = None;

  egg_tray_icon_update_manager_window (icon, TRUE);
}

#endif

static gboolean
transparent_expose_event (GtkWidget *widget, GdkEventExpose *event, gpointer user_data)
{
  gdk_window_clear_area (widget->window, event->area.x, event->area.y,
			 event->area.width, event->area.height);
  return FALSE;
}

static void
make_transparent_again (GtkWidget *widget, GtkStyle *previous_style,
			gpointer user_data)
{
  gdk_window_set_back_pixmap (widget->window, NULL, TRUE);
}

static void
make_transparent (GtkWidget *widget, gpointer user_data)
{
  if (GTK_WIDGET_NO_WINDOW (widget) || GTK_WIDGET_APP_PAINTABLE (widget))
    return;

  gtk_widget_set_app_paintable (widget, TRUE);
  gtk_widget_set_double_buffered (widget, FALSE);
  gdk_window_set_back_pixmap (widget->window, NULL, TRUE);
  g_signal_connect (widget, "expose_event",
		    G_CALLBACK (transparent_expose_event), NULL);
  g_signal_connect_after (widget, "style_set",
			  G_CALLBACK (make_transparent_again), NULL);
}

static void
egg_tray_icon_realize (GtkWidget *widget)
{
#ifdef GDK_WINDOWING_X11
  EggTrayIcon *icon = EGG_TRAY_ICON (widget);
  GdkScreen *screen;
  GdkDisplay *display;
  Display *xdisplay;
  char buffer[256];
  GdkWindow *root_window;

  if (GTK_WIDGET_CLASS (parent_class)->realize)
    GTK_WIDGET_CLASS (parent_class)->realize (widget);

  make_transparent (widget, NULL);

  screen = gtk_widget_get_screen (widget);
  display = gdk_screen_get_display (screen);
  xdisplay = gdk_x11_display_get_xdisplay (display);

  /* Now see if there's a manager window around */
  g_snprintf (buffer, sizeof (buffer),
	      "_NET_SYSTEM_TRAY_S%d",
	      gdk_screen_get_number (screen));

  icon->selection_atom = XInternAtom (xdisplay, buffer, False);

  icon->manager_atom = XInternAtom (xdisplay, "MANAGER", False);

  icon->system_tray_opcode_atom = XInternAtom (xdisplay,
						   "_NET_SYSTEM_TRAY_OPCODE",
						   False);

  icon->orientation_atom = XInternAtom (xdisplay,
					"_NET_SYSTEM_TRAY_ORIENTATION",
					False);

  egg_tray_icon_update_manager_window (icon, FALSE);
  egg_tray_icon_send_dock_request (icon);

  root_window = gdk_screen_get_root_window (screen);

  /* Add a root window filter so that we get changes on MANAGER */
  gdk_window_add_filter (root_window,
			 egg_tray_icon_manager_filter, icon);
#endif
}

static void
egg_tray_icon_add (GtkContainer *container, GtkWidget *widget)
{
  g_signal_connect (widget, "realize",
		    G_CALLBACK (make_transparent), NULL);
  GTK_CONTAINER_CLASS (parent_class)->add (container, widget);
}

EggTrayIcon *
egg_tray_icon_new_for_screen (GdkScreen *screen, const char *name)
{
  g_return_val_if_fail (GDK_IS_SCREEN (screen), NULL);

  return g_object_new (EGG_TYPE_TRAY_ICON, "screen", screen, "title", name, NULL);
}

EggTrayIcon*
egg_tray_icon_new (const gchar *name)
{
  return g_object_new (EGG_TYPE_TRAY_ICON, "title", name, NULL);
}

guint
egg_tray_icon_send_message (EggTrayIcon *icon,
			    gint         timeout,
			    const gchar *message,
			    gint         len)
{
  guint stamp;

  g_return_val_if_fail (EGG_IS_TRAY_ICON (icon), 0);
  g_return_val_if_fail (timeout >= 0, 0);
  g_return_val_if_fail (message != NULL, 0);

#ifdef GDK_WINDOWING_X11
  if (icon->manager_window == None)
    return 0;
#endif

  if (len < 0)
    len = strlen (message);

  stamp = icon->stamp++;

#ifdef GDK_WINDOWING_X11
  /* Get ready to send the message */
  egg_tray_icon_send_manager_message (icon, SYSTEM_TRAY_BEGIN_MESSAGE,
				      icon->manager_window,
				      timeout, len, stamp);

  /* Now to send the actual message */
  gdk_error_trap_push ();
  while (len > 0)
    {
      XClientMessageEvent ev;
      Display *xdisplay;

      xdisplay = GDK_DISPLAY_XDISPLAY (gtk_widget_get_display (GTK_WIDGET (icon)));

      ev.type = ClientMessage;
      ev.window = icon->manager_window;
      ev.format = 8;
      ev.message_type = XInternAtom (xdisplay,
				     "_NET_SYSTEM_TRAY_MESSAGE_DATA", False);
      if (len > 20)
	{
	  memcpy (&ev.data, message, 20);
	  len -= 20;
	  message += 20;
	}
      else
	{
	  memcpy (&ev.data, message, len);
	  len = 0;
	}

      XSendEvent (xdisplay,
		  icon->manager_window, False, StructureNotifyMask, (XEvent *)&ev);
      XSync (xdisplay, False);
    }
  gdk_error_trap_pop ();
#endif

  return stamp;
}

void
egg_tray_icon_cancel_message (EggTrayIcon *icon,
			      guint        id)
{
  g_return_if_fail (EGG_IS_TRAY_ICON (icon));
  g_return_if_fail (id > 0);
#ifdef GDK_WINDOWING_X11
  egg_tray_icon_send_manager_message (icon, SYSTEM_TRAY_CANCEL_MESSAGE,
				      (Window)gtk_plug_get_id (GTK_PLUG (icon)),
				      id, 0, 0);
#endif
}

GtkOrientation
egg_tray_icon_get_orientation (EggTrayIcon *icon)
{
  g_return_val_if_fail (EGG_IS_TRAY_ICON (icon), GTK_ORIENTATION_HORIZONTAL);

  return icon->orientation;
}





#endif /* GTK_CHECK_VERSION(2, 1, 0) */
#endif /* wxUSE_TASKBARICON */
