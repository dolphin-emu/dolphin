PORTING LIBUSB TO OTHER PLATFORMS

Introduction
============

This document is aimed at developers wishing to port libusb to unsupported
platforms. I believe the libusb API is OS-independent, so by supporting
multiple operating systems we pave the way for cross-platform USB device
drivers.

Implementation-wise, the basic idea is that you provide an interface to
libusb's internal "backend" API, which performs the appropriate operations on
your target platform.

In terms of USB I/O, your backend provides functionality to submit
asynchronous transfers (synchronous transfers are implemented in the higher
layers, based on the async interface). Your backend must also provide
functionality to cancel those transfers.

Your backend must also provide an event handling function to "reap" ongoing
transfers and process their results.

The backend must also provide standard functions for other USB operations,
e.g. setting configuration, obtaining descriptors, etc.


File descriptors for I/O polling
================================

For libusb to work, your event handling function obviously needs to be called
at various points in time. Your backend must provide a set of file descriptors
which libusb and its users can pass to poll() or select() to determine when
it is time to call the event handling function.

On Linux, this is easy: the usbfs kernel interface exposes a file descriptor
which can be passed to poll(). If something similar is not true for your
platform, you can emulate this using an internal library thread to reap I/O as
necessary, and a pipe() with the main library to raise events. The file
descriptor of the pipe can then be provided to libusb as an event source.


Interface semantics and documentation
=====================================

Documentation of the backend interface can be found in libusbi.h inside the
usbi_os_backend structure definition.

Your implementations of these functions will need to call various internal
libusb functions, prefixed with "usbi_". Documentation for these functions
can be found in the .c files where they are implemented.

You probably want to skim over *all* the documentation before starting your
implementation. For example, you probably need to allocate and store private
OS-specific data for device handles, but the documentation for the mechanism
for doing so is probably not the first thing you will see.

The Linux backend acts as a good example - view it as a reference
implementation which you should try to match the behaviour of.


Getting started
===============

1. Modify configure.ac to detect your platform appropriately (see the OS_LINUX
stuff for an example).

2. Implement your backend in the libusb/os/ directory, modifying
libusb/os/Makefile.am appropriately.

3. Add preprocessor logic to the top of libusb/core.c to statically assign the
right usbi_backend for your platform.

4. Produce and test your implementation.

5. Send your implementation to libusb-devel mailing list.


Implementation difficulties? Questions?
=======================================

If you encounter difficulties porting libusb to your platform, please raise
these issues on the libusb-devel mailing list. Where possible and sensible, I
am interested in solving problems preventing libusb from operating on other
platforms.

The libusb-devel mailing list is also a good place to ask questions and
make suggestions about the internal API. Hopefully we can produce some
better documentation based on your questions and other input.

You are encouraged to get involved in the process; if the library needs
some infrastructure additions/modifications to better support your platform,
you are encouraged to make such changes (in cleanly distinct patch
submissions). Even if you do not make such changes yourself, please do raise
the issues on the mailing list at the very minimum.
