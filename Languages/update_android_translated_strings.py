from pathlib import Path
from lxml import etree
import xml.dom.minidom as minidom
import os
import glob
import polib
from update_android_source_strings import get_xml_string

base_strings_path = Path('Source').joinpath('Android').joinpath('app').joinpath('src').joinpath('main').joinpath('res')
base_xml_path = base_strings_path.joinpath('values').joinpath('strings.xml')
base_po_path = Path('Languages').joinpath('po')

# Get a tree with all the available base strings
base_strings_tree = etree.parse(os.path.abspath(base_xml_path))
base_strings_root = base_strings_tree.getroot()
base_strings = base_strings_root.findall('.//string')

# Put all of the .po files in a list
pattern = os.path.join(os.path.abspath(base_po_path), '*.po')
po_list = glob.glob(pattern)
po_list = [f for f in po_list if os.path.basename(f) != "en.po"]

for file in po_list:
    # Format directory name to match Android requirements
    directory_name = os.path.basename(file)
    print("Creating strings.xml for " + directory_name)
    base_index = directory_name.rindex('.')
    directory_name = directory_name[:base_index]
    directory_name = directory_name.replace('_', '-r')
    directory_name = "values-" + directory_name

    # Iterate through each po entry and string in our base strings.xml file
    # to find matching strings and add them to our new strings.xml file.
    translated_root = etree.Element('resources')
    po_file = polib.pofile(file)
    for string in base_strings:
        item = get_xml_string(string)
        for entry in po_file:
            if (item == entry.msgid):
                if (entry.msgstr == ""):
                    continue
                id = string.get('name')
                xml_item = etree.SubElement(translated_root, 'string')
                xml_item.text = entry.msgstr.replace("'", "\\'").replace('"', '\\"').strip()
                xml_item.set('name', id)
                continue

    parsed = minidom.parseString(etree.tostring(translated_root))

    # Create path and ignore if it already exists
    strings_dir = base_strings_path.joinpath(directory_name)
    strings_dir_abs = os.path.abspath(strings_dir)
    if not os.path.exists(strings_dir_abs):
        os.mkdir(strings_dir_abs)
    else:
        print(f"Directory '{strings_dir_abs}' already exists")

    # Format the xml text and write to a new copy of the file
    output_file = open(os.path.abspath(strings_dir.joinpath('strings.xml')), "wb")
    output_file.write(parsed.toprettyxml(encoding="utf-8"))
    output_file.close()
