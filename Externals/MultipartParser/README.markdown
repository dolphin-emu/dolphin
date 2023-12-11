What is it?
===========
An simple, efficient parser for multipart MIME messages, based on
[Formidable's](http://github.com/felixge/node-formidable) parser.

Why?
----
MIME multipart messages are a total pain to parse because the grammar is
so insane. Furthermore, the MIME specification is incredibly large. This
has led to an army of equally large and complex MIME libraries. If you
just want to parse a MIME multipart message without hassle then using all
of those libraries are less than ideal. They all tend to handle the kitchen
sink (e.g. they handling email parsing and all kinds of other stuff you don't
need) or they depend on other libraries that you may not want (e.g. APR, glib)
or they are under-documented or under-tested or just not efficient (e.g.
buffering all data in memory; good luck parsing a 2 GB file upload). You can
write your own parser but because the multipart grammar is so much of a pain
it's very easy to make mistakes.

Goals and highlights of this parser
-----------------------------------

 * Multipart parsing, and only multipart parsing.
 * Event-driven API.
 * No dependencies on any external libraries, just straight C++ with STL.
 * Efficient. Nothing in the input is buffered except what's absolutely
   necessary for parsing.
 * Only one level of multipart parsing. A multipart message part can itself
   be a multipart message, but this parser doesn't attempt to provide a
   complex API for handling nested multipart messages. Instead the developer
   should just use another parser instance to parse nested messages.
 * No I/O is handled for you. This parser won't depend on any particular
   I/O library or even any particular operating system's I/O API. It won't
   block on I/O by itself, giving you full control over when (not) to block.
   It won't save data to files by itself, giving you full control over what to
   do with the parsed data.
 * Not thread-safe, but reentrant. No dependencies on any threading libraries.