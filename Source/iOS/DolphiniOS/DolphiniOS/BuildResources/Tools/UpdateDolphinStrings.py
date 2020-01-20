#!/usr/local/bin/python3

import polib
import os
import sys

# This tool writes the contents of the "po" files to the required ".strings" format
# for iOS.

# Usage:
# python3 UpdateDolphinStrings.py <po files directory> <localizables directory>

for root, dirs, files in os.walk(sys.argv[1]):
  for po_path in files:
    # Skip the template file
    if po_path.endswith('.pot'):
      continue
      
    language = os.path.splitext(os.path.basename(po_path))[0]
    strings_path = os.path.join(sys.argv[2], language + '.lproj', 'Dolphin.strings')
    
    # Skip unsupported languages
    if not os.path.exists(strings_path):
      continue
    
    print('Processing ' + language + ' at ' + strings_path)
    
    with open(strings_path, 'w') as strings_file:
      po = polib.pofile(os.path.join(sys.argv[1], po_path))
      
      for entry in po:
        strings_file.write('"' + entry.msgid.replace('"', r'\"') + '" = "' + entry.msgstr.replace('"', r'\"') + '";\n')
