SET	app_name	create-dmg

VERSION	create-dmg.cur	create-dmg	heads/master

NEWDIR	build.dir	temp	%-build	-

NEWFILE	create-dmg.zip	featured	%.zip	%


COPYTO	[build.dir]
	INTO	create-dmg	[create-dmg.cur]/create-dmg
	INTO	sample	[create-dmg.cur]/sample
	INTO	support	[create-dmg.cur]/support
	
SUBSTVARS	[build.dir<alter>]/create-dmg	[[]]


ZIP	[create-dmg.zip]
	INTO	[build-files-prefix]	[build.dir]


PUT	megabox-builds	create-dmg.zip
PUT	megabox-builds	build.log

PUT	s3-builds	create-dmg.zip
PUT	s3-builds	build.log
