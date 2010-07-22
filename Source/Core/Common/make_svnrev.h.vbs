set wshShell	= CreateObject("WScript.Shell")
outfile			= "Src/svnrev.h"
svncmd			= "SubWCRev ../../.. Src/svnrev_template.h " & outfile
hgcmd			= "hg svn info"

ret = wshShell.run(svncmd, 0, true)
if ret <> 0 then ' Perhaps we should just check for 6? dunno/care...
	set hgexec = wshShell.exec(hgcmd)
	do while hgexec.status = 0 : wscript.sleep 100 : loop
	do while true
		line = hgexec.stdout.readline
		if instr(line, "Revision") then
			sline = split(line)
			wscript.echo "Hg: Working copy at SVN revision " & sline(1)
			set oFS		= CreateObject("Scripting.fileSystemObject")
			set oFile	= oFS.CreateTextFile(outfile, true)
			oFile.writeline("#define SVN_REV_STR """ & sline(1) & """")
			set oFS 	= nothing
			exit do
		end if
		if hgexec.stdout.atEndofStream then
			wscript.echo "Neither SVN or Hg revision info found!"
			wscript.quit 1
		end if
	loop
end if
