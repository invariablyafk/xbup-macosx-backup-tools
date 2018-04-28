
xbup: a set of backup tools for Mac OSX

Note: This is a clone of the 2.1 version of Victor Shoup's `xbup` backup tools. Available here: http://www.shoup.net/xbup/

These are command-line backup utilities that can be used to split off HFS and HFS+ resource forks in files from the classic Macintosh days, into separate "normal" files. These separate files allow the resource forks to be maintained in git, or when transferred to non-HFS filesystems. Later, these same tools can be used to merge the forks back into the original files.

These files are similar in function to Apple Double files (ie https://opensource.apple.com/source/xnu/xnu-792.13.8/bsd/vfs/vfs_xattr.c) Although I've made zero attempt to verify any compatibility with the AppleDouble format.



******

See the user's manual doc.pdf for installation
and usage intructions.

******

Copyright 2007--2008 Victor Shoup.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.


