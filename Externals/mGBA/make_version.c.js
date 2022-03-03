var wshShell		= new ActiveXObject("WScript.Shell")
var oFS				= new ActiveXObject("Scripting.FileSystemObject");

wshShell.CurrentDirectory   += "\\mgba";
var outfile                 = "../version.c";
var cmd_commit              = " describe --always --abbrev=40 --dirty";
var cmd_commit_short        = " describe --always --dirty";
var cmd_branch              = " symbolic-ref --short HEAD";
var cmd_rev                 = " rev-list HEAD --count";
var cmd_tag                 = " describe --tag --exact-match";

function GetGitExe()
{
	try
	{
		gitexe = wshShell.RegRead("HKCU\\Software\\GitExtensions\\gitcommand");
		wshShell.Exec(gitexe);
		return gitexe;
	}
	catch (e)
	{}

	for (var gitexe in {"git.cmd":1, "git":1, "git.bat":1})
	{
		try
		{
			wshShell.Exec(gitexe);
			return gitexe;
		}
		catch (e)
		{}
	}

	// last try - msysgit not in path (vs2015 default)
	msyspath = "\\Git\\cmd\\git.exe";
	gitexe = wshShell.ExpandEnvironmentStrings("%PROGRAMFILES(x86)%") + msyspath;
	if (oFS.FileExists(gitexe)) {
		return gitexe;
	}
	gitexe = wshShell.ExpandEnvironmentStrings("%PROGRAMFILES%") + msyspath;
	if (oFS.FileExists(gitexe)) {
		return gitexe;
	}

	WScript.Echo("Cannot find git or git.cmd, check your PATH:\n" +
		wshShell.ExpandEnvironmentStrings("%PATH%"));
	WScript.Quit(1);
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
		// file doesn't exist
		return "";
	}
}

// get version from version.cmake
var version_cmake   = GetFileContents("version.cmake");
var version_major   = version_cmake.match(/set\(LIB_VERSION_MAJOR (.*)\)/)[1];
var version_minor   = version_cmake.match(/set\(LIB_VERSION_MINOR (.*)\)/)[1];
var version_patch   = version_cmake.match(/set\(LIB_VERSION_PATCH (.*)\)/)[1];
var version_abi     = version_cmake.match(/set\(LIB_VERSION_ABI (.*)\)/)[1];
var version_string  = version_major + "." + version_minor + "." + version_patch;

// get info from git
var gitexe = GetGitExe();
var commit      	= GetFirstStdOutLine(gitexe + cmd_commit);
var commit_short    = GetFirstStdOutLine(gitexe + cmd_commit_short);
var branch  		= GetFirstStdOutLine(gitexe + cmd_branch);
var rev	        	= GetFirstStdOutLine(gitexe + cmd_rev);
var tag 	    	= GetFirstStdOutLine(gitexe + cmd_tag);
var binary_name     = "mgba";
var project_name    = "mGBA";

if (!rev)
    rev = -1;

if (tag)
{
	version_string = tag;
}
else if (branch)
{
	if (branch == "master")
		version_string = rev + "-" + commit_short;
	else
		version_string = branch + "-" + rev + "-" + commit_short;

	if (branch != version_abi)
        version_string = version_abi + "-" + version_string;
}

if (!commit)
	commit = "(unknown)";
if (!commit_short)
	commit_short = "(unknown)";
if (!branch)
	branch = "(unknown)";

var out_contents =
    "#include <mgba/core/version.h>\n" +
    "MGBA_EXPORT const char* const gitCommit = \"" + commit + "\";\n" +
    "MGBA_EXPORT const char* const gitCommitShort = \"" + commit_short + "\";\n" +
    "MGBA_EXPORT const char* const gitBranch = \"" + branch + "\";\n" +
    "MGBA_EXPORT const int gitRevision = " + rev + ";\n" +
    "MGBA_EXPORT const char* const binaryName = \"" + binary_name + "\";\n" +
    "MGBA_EXPORT const char* const projectName = \"" + project_name + "\";\n" +
    "MGBA_EXPORT const char* const projectVersion = \"" + version_string + "\";\n";

// check if file needs updating
if (out_contents == GetFileContents(outfile))
{
	WScript.Echo(project_name + ": " + outfile + " current at " + version_string);
}
else
{
	// needs updating - writeout current info
	oFS.CreateTextFile(outfile, true).Write(out_contents);
	WScript.Echo(project_name + ": " + outfile + " updated to " + version_string);
}
