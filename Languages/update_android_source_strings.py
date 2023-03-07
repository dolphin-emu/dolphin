from pathlib import Path
from lxml import etree
import os
import polib
import re

# Remove <string> tags from each string
# Normally when getting the body text, this would not include inner tags.
# This becomes a problem when parsing strings with clickable links.
# So instead of manually breaking the body down and reassembling it,
# here we just do some string manipulation to save some time.
def get_xml_string(xml_string):
    item = etree.tostring(xml_string, encoding='unicode')
    end_first_tag = item.index('>')
    first_half = item[end_first_tag+1:]
    start_second_tag = first_half.rindex('<')
    output = first_half[:start_second_tag]

    # Remove newline characters within strings that aren't explicitly '\n'
    output = re.sub(r'\n', ' ', output)
    output = ''.join(output)

    # Fix cases of broken escaped characters
    output = output.replace('\\n', '\n').replace("\\'", "'").replace('\\"', '"').replace('\\\\', '\\').strip()

    # Replace all white space with a single space unless it is escaped by
    # double quotation marks.
    output = re.sub(r'(?<!")\s+(?!")', ' ', output)
    output = ''.join(output)

    return output

if __name__ == "__main__":
    strings_path = Path('Source').joinpath('Android').joinpath('app').joinpath('src').joinpath('main').joinpath('res').joinpath('values').joinpath('strings.xml')
    pot_path = Path('Languages').joinpath('po').joinpath('dolphin-android.pot')

    tree = etree.parse(os.path.abspath(strings_path))
    root = tree.getroot()

    strings = root.findall('.//string')

    android_pot = polib.POFile()
    android_pot.metadata['fuzzy'] = False
    android_pot.metadata['Content-Type'] = 'text/plain; charset=UTF-8'

    for string in strings:
        # Skip strings that aren't translatable
        translatable = string.get('translatable')
        if (translatable is not None):
            if (translatable == 'false'):
                continue

        output = get_xml_string(string)

        entry = polib.POEntry(msgid=output)

        # Add comment above the msgid
        comment = string.get("i18n")
        if (comment is not None):
            entry.tcomment = comment

        android_pot.append(entry)

    android_pot.save(os.path.abspath(pot_path))
