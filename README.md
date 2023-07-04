# Developer Tools for the Atari ST
These are files rescued from my old hard drive containing the source to my 68k debugger (X-Debug) and linker (AmpLink) from the early 90s.

# X-Debug
As soon as I left HiSoft I I immediately missed the ability to add features to the debugger I used every day, so set about creating a new one, and made X-Debug.
Unlike MonST (which I wrote) it was written mostly in C and was highly configurable, supporting customizable commands and new screen types.

# AmpLink
I can't recall why I decided I needed my own linker, but I made this one which can also create Amiga images. ('Amp' are my initials and you'll see them all over the code).

# To Build
I used Lattice C and HiSoft GenST 2.something (which I also wrote) to build these, along with a Unix-like shell to run the makefiles. I no longer have the ability to build or verify these sources myself.

# Other Utilities
There is also the source to a few utilities here, namely DReloc, DumpDB, LoadHigh, ResetFPU and TTHead.
