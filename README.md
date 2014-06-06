SimpleBackdoor
==============

A simple multithreaded backdoor. Spawns a command prompt and redirects input and output over a TCP socket that can be accessed via netcat.

Change the PORT_NUMBER define at the top of the file to fit your needs.

Good to add on to other projects. 

Based (heavily in some sections) on code from http://hackforlifeee.wordpress.com/2012/01/13/basic-backdoor-c/ but adds a few things:
- Socket timouts via select() (program isn't hung up by accept())
- Multithreads the whole thing so multiple connections can be made simultaneously
- Increments the port number if binding fails so it can still be accessed
- Removed global variables
- Commented more, made variable names a bit more understandable

Compile for maximum compatibility (no VC++ Redistributables required) as detailed here: http://www.havefuninsi.de/?p=290

Enjoy!
