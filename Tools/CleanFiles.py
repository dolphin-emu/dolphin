
import codecs
import os
import glob

standard_sections = [
    "Core",
    "EmuState",
    "OnLoad",
    "OnFrame",
    "ActionReplay",
    "Video",
    "Video_Settings",
    "Video_Enhancements",
    "Video_Hacks",
    "Speedhacks",
]

standard_comments = {
    "Core": "Values set here will override the main dolphin settings.",
    "EmuState": "The Emulation State. 1 is worst, 5 is best, 0 is not set.",
    "OnLoad": "Add memory patches to be loaded once on boot here.",
    "OnFrame": "Add memory patches to be applied every frame here.",
    "ActionReplay": "Add action replay cheats here.",
    "Video": "",
    "Video_Settings": "",
    "Video_Enhancements": "",
    "Video_Hacks": "",
    "Speedhacks": "",
}

def normalize_comment(line):
    line = line.strip().lstrip('#').lstrip()
    if line:
        return "# %s" % (line,)
    else:
        return ""

def normalize_ini_file(in_, out):
    sections = {}
    current_section = None
    toplevel_comment = ""
    wants_comment = False

    for line in in_:
        line = line.strip()

        # strip utf8 bom
        line = line.lstrip(u'\ufeff')

        if line.startswith('#'):
            line = normalize_comment(line)
            if current_section is None:
                toplevel_comment += line
                continue

        if line.startswith('['):
            end = line.find(']')
            section_name = line[1:end]
            if section_name not in standard_sections:
                continue
            current_section = []
            sections[section_name] = current_section
            wants_comment = False
            continue

        if current_section is None and line:
            raise ValueError("invalid junk")

        if current_section is None:
            continue

        if line.startswith('#') and not wants_comment:
            continue

        current_section.append(line)
        if line:
            wants_comment = True

    out.write(toplevel_comment.strip() + "\n\n")

    for section in standard_sections:
        lines = '\n'.join(sections.get(section, "")).strip()
        comments = standard_comments[section]

        if not lines and not comments:
            continue

        out.write("[%s]\n" % (section,))
        if comments:
            out.write("# %s\n" % (comments,))
        if lines:
            out.write(lines)
            out.write('\n')
        out.write('\n')

def main():
    base_path = os.path.dirname(__file__)
    pattern = os.path.join(base_path, "../Data/User/GameConfig/??????.ini")
    for name in glob.glob(pattern):
        in__name = name
        out_name = name + '.new'
        in_ = codecs.open(in__name, 'r', 'utf8')
        out = codecs.open(out_name, 'w', 'utf8')
        normalize_ini_file(in_, out)
        os.rename(out_name, in__name)

if __name__ == "__main__":
    main()
