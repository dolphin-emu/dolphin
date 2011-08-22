var wshShell		= new ActiveXObject("WScript.Shell")
var oFS				= new ActiveXObject("Scripting.FileSystemObject");

var outfile			= "./Src/scmrev.h";
var cmd_revision	= " rev-parse HEAD";
var cmd_describe	= " describe --always --dirty";
var cmd_branch		= " rev-parse --abbrev-ref HEAD";

function GetGitExe()
{
	var gitexe = "git.cmd";
	try
	{
		wshShell.Exec(gitexe);
	}
	catch (e)
	{
		try
		{
			gitexe = "git";
			wshShell.Exec(gitexe);
		}
		catch (e)
		{
			WScript.Echo("Cannot find git or git.cmd, check your PATH:\n" +
				wshShell.ExpandEnvironmentStrings("%PATH%"));
			WScript.Quit(1);
		}
	}
	return gitexe;
}

function GetFirstStdOutLine(cmd)
{
	try
	{
		return wshShell.Exec(cmd).StdOut.ReadLine();
	}
	catch (e)
	{
		// catch "the system cannot find the file specified" error
		WScript.Echo("Failed to exec " + cmd + " this should never happen");
		WScript.Quit(1);
	}
}

function GetFileContents(f)
{
	try
	{
		return oFS.OpenTextFile(f).ReadAll();
	}
	catch (e)
	{
		// file doesn't exist or string not found, (re)create it
		oFS.CreateTextFile(f);
		return 0;
	}
}

// get info from git
var gitexe = GetGitExe();
var revision	= GetFirstStdOutLine(gitexe + cmd_revision);
var describe	= GetFirstStdOutLine(gitexe + cmd_describe);
var branch		= GetFirstStdOutLine(gitexe + cmd_branch);
var isMaster	= 0

if (branch == "master")
	isMaster = 1

var out_contents =
	"#define SCM_REV_STR \"" + revision + "\"\n" +
	"#define SCM_DESC_STR \"" + describe + "\"\n" +
	"#define SCM_BRANCH_STR \"" + branch + "\"\n" +
	"#define SCM_IS_MASTER " + isMaster + "\n";

// check if file needs updating
if (out_contents == GetFileContents(outfile))
{
	WScript.Echo(outfile + " doesn't need updating (already at " + revision + ")");
}
else
{
	// needs updating - writeout current info
	oFS.CreateTextFile(outfile, true).Write(out_contents);
	WScript.Echo(outfile + " updated (" + revision + ")");
}
