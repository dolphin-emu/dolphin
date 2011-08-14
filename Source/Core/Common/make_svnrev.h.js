var wshShell	= new ActiveXObject("WScript.Shell")
var oFS			= new ActiveXObject("Scripting.FileSystemObject");

var outfile		= "./Src/svnrev.h";
var svncmd		= "SubWCRev ../../.. ./Src/svnrev_template.h " + outfile;
var svntestcmd	= "SubWCRev ../../..";
var hgcmd		= "hg svn info";
var gitcmd      = "git.cmd rev-parse HEAD";

var SVN = 1, HG = 2, git = 3;
var file_rev = 0, cur_rev = 0, cur_cms = 0;

function RunCmdGetMatch(cmd, regex)
{
	// run the command
	try
	{
		var cmdexec	= wshShell.Exec(cmd);
	}
	catch (e)
	{
		// catch "the system cannot find the file specified" error
		return 0;
	}
	// ReadLine is synchronous
	while (!cmdexec.StdOut.AtEndOfStream)
	{
		var reg_exec = regex.exec(cmdexec.StdOut.ReadLine())
		if (reg_exec)
			return reg_exec[1];	// return first capture group
	}
	// failed
	return 0;
}

try
{
	// read the value of SVN_REV_STR
	file_rev = oFS.OpenTextFile(outfile).ReadLine().match(/\d+/);
}
catch (e)
{
	// file doesn't exist or string not found, (re)create it
	oFS.CreateTextFile(outfile);
}

// get the "Last commited at revision" from SubWCRev's output
cur_rev = RunCmdGetMatch(svntestcmd, /^Last .*?(\d+)/);
if (cur_rev)
	cur_cms = SVN;
else
{
	// SubWCRev failed, try hg
	cur_rev = RunCmdGetMatch(hgcmd, /Revision.*?(\d+)/);
	if (cur_rev)
		cur_cms = HG;
	else
	{
		// hg failed, try git
	   cur_rev = RunCmdGetMatch(gitcmd, /(.*)/);
	   if (cur_rev)
		   cur_cms = git;
	   else
	   {
		   WScript.Echo("Trying to get SVN, Hg, and git info all failed");
		   WScript.Quit(1);
	   }
	}
}

// check if svnrev.h needs updating
if (cur_rev == file_rev)
{
	WScript.Echo("svnrev.h doesn't need updating (already at " + cur_rev + ")");
	WScript.Quit(0);
}
else if (cur_cms == SVN)
{
	// update using SubWCRev and template file
	var ret = wshShell.run(svncmd, 0, true);
}
else
{
	// manually create the file
	oFS.CreateTextFile(outfile, true).WriteLine("#define SVN_REV_STR \"" + cur_rev + "\"");
}
WScript.Echo("svnrev.h updated (" + cur_rev + ")");