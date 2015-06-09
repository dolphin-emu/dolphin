/* ReadDolphinMap.idc

   Loads Dolphin .map files into IDA Pro.
   Carl Kenner, 2014
*/

#include <idc.idc>

static main(void)
{
	auto fh;
	auto fname;
	auto pusha, popa;
	auto start, end;
	auto success;
	auto count;
	auto line;
	auto ea; // column 1
	auto name; // column 30
	auto p;
	auto code;

	fname = AskFile(0,"*.map","Load a .map file from Dolphin emulator...");

	fh = fopen(fname, "r");
	if (fh == 0) {
		Message("Can't open %s\n", fname);
		return;
	}

	Message("Loading %s dolphin map file:\n", fname);

	for (count = 0; 1; count++) {
		line = readstr(fh);
		if (line == -1)
			break;
		if (strlen(line)>30 && line[0]!=" ") {
			ea = xtol(substr(line,0,8));
			name = substr(line,29,strlen(line)-1);
			if (substr(name,0,3)!="zz_") {
				if (!MakeNameEx(ea,name,SN_NOCHECK | SN_PUBLIC | SN_NON_AUTO |SN_NOWARN)) {
					MakeNameEx(ea,name+"_2",SN_NOCHECK | SN_PUBLIC | SN_NON_AUTO );
				}
				Message("ea='%x', name='%s'\n", ea, name);
			} else {
				MakeNameEx(ea,name,SN_NOCHECK | SN_PUBLIC | SN_AUTO | SN_WEAK | SN_NOWARN);
			}
		} else if (strlen(line)>30) {
			ea = xtol(substr(line,18,18+8));
			p = strstr(line, " \t");
			if (p>=30 && ea!=0) {
				name = substr(line,30,p);
				code = substr(line,p+2,strlen(line));
				SetFunctionCmt(ea, code, 0);
				if (!MakeNameEx(ea,name,SN_NOCHECK | SN_PUBLIC | SN_NON_AUTO |SN_NOWARN)) {
					MakeNameEx(ea,name+"_2",SN_NOCHECK | SN_PUBLIC | SN_NON_AUTO );
				}
			}
		}
	}
	Message("Dolphin map file done.\n");
}
