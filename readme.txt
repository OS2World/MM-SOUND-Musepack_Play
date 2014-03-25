Musepack v1.2.2 Play 0.00
=========================

Introduction
------------
	This is Musepack Living Audio Compression decoder plugin for PM123. 
	Based on Musepack sv7 1.5 (library version 1.2.2). Early OS/2 build.

	Musepack is an audio compression format with a strong emphasis on high 
	quality. It's not lossless, but it is designed for transparency, so 
	that you won't be able to hear differences between the original wave 
	file and the much smaller MPC file.

	It is based on the MPEG-1 Layer-2 / MP2 algorithms, but since 1997 it 
	has rapidly developed and vastly improved and is now at an advanced 
	stage in which it contains heavily optimized and patentless code.
	(http://www.musepack.com/)

System requirements
-------------------
	Working PM123 (relatively modern version), http://glass.os2.spb.ru/software/english/pm123.html
	libc061, ftp://ftp.netlabs.org/pub/gcc/libc-0.6.1-csd1.zip

Installation
------------
	- make it sure that LIBC061.DLL located somewhere in your LIBPATH
	- place the file mpcplay.dll into directory where PM123.EXE located
	- start PM123
	- Right-click on the PM123 window to open the "properties" dialog
	- Choose the page "plugins"
	- Press the "add" button in the "decoder plugin" section
	- Choose "mpcplay.dll" in the file dialog.
	  Press Ok.
	- Close "PM123 properties" dialog

	Now you can listen Musepack compressed (.mpc, .mp+) audio files!

De-Installation
---------------
	In case of any trouble with this plugin, close PM123 and remove 
	mpcplay.dll from the PM123.EXE directory.

License
-------
	wvplay.dll use libmpcdec 1.2.2 library, copyrighted by:

	Copyright (c) 2005, The Musepack Development Team
                          All rights reserved.

	Please see doc/COPYING for terms of redistribution and use.


	mpcplay.dll based on source, copyrighted by:

	 * Copyright 1997-2003 Samuel Audet <guardia@step.polymtl.ca>
	 *                     Taneli Lepp„ <rosmo@sektori.com>

	Please see src/wvplay.cpp for terms of redistribution and use.


	All other source parts and compilation itself copyrighted by:

		Copyright (C) 2006 ntim <ntim@softhome.net> 

	and subject of use and distribution (in binary or source code) 
	under terms of GNU General Public License as published by the Free 
	Software Foundation; either version 2 of the License, or (at your 
	option) any later version.

	See doc/GNUCOPYING for details.

Warnings and known bugs
-----------------------
	This version are from a litle to almost untested. Use with care!
	The author is not responsible for any damage this program may cause.

	Due to limitation of seek algorithm random positioning in a big media 
	file may take a time.
	Fast forward may cause delay also.
	Rewind is disabled at all.
	
	Due to "always-VBR" nature of Musepack stream the bit rates in 
	pm123 v.1.3.1 and early are not shown correctly with some skins 
	(including default one). Please see later versions...

Sources
-------
	Sources of mpcplay.dll located in src/ directory.
	To build from sources you also need libmpcdec 1.2.2 library,
	please find source at http://www.musepack.com/

Contacts
--------
	All questions about this build please send to ntim@softhome.net
	or contact ntim on #os2russian (irc://efnet/os2russian)

June 2006
ntim
