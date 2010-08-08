set wshShell	= CreateObject("WScript.Shell")
basedir			= wscript.arguments(0)
outfile			= basedir & "/Src/svnrev.h"
svncmd			= "SubWCRev ../../.. " & basedir & "/Src/svnrev_template.h " & outfile
svntestcmd		= "SubWCRev ../../.."
hgcmd			= "hg svn info"
const svn		= 0
const hg		= 1

set oFS			= CreateObject("Scripting.fileSystemObject")
if not oFS.FileExists(outfile) then
	oFS.CreateTextFile(outfile)
	file_rev	= 0
else
	set oFile	= oFS.OpenTextFile(outfile)
	set re		= new regexp
	re.pattern	= "[0-9]+"
	file_rev	= re.execute(oFile.readline)(0)
end if

set testexec	= wshShell.exec(svntestcmd)
do while testexec.status = 0 : wscript.sleep 100 : loop
if testexec.exitcode = 0 then
	testout		= testexec.stdout.readall
	set re		= new regexp
	re.pattern	= "Last committed at revision [0-9]+"
	cur_rev		= split(re.execute(testout)(0))(4)
	cur_cms		= svn
else
	set hgexec	= wshShell.exec(hgcmd)
	do while hgexec.status = 0 : wscript.sleep 100 : loop
	do while true
		line = hgexec.stdout.readline
		if instr(line, "Revision") then
			cur_rev = split(line)(1)
			cur_cms	= hg
			exit do
		end if
		if hgexec.stdout.atEndofStream then
			wscript.echo "Neither SVN or Hg revision info found!"
			wscript.quit 1
		end if
	loop
end if

if file_rev = cur_rev then
	wscript.echo "svnrev.h doesn't need updating"
	wscript.quit 0
elseif cur_cms = svn then
	ret = wshShell.run(svncmd, 0, true)
elseif cur_cms = hg then
	set oFile = CreateObject("Scripting.fileSystemObject").CreateTextFile(outfile, true)
	oFile.writeline("#define SVN_REV_STR """ & cur_rev & """")
else
	wscript.echo "WTF, shouldn't be here!"
end if