#!/usr/local/bin/python3

import polib
import os
import sys
import re

# This tool writes the contents of the "po" files to the required ".strings" format
# for iOS.

# Usage:
# python3 UpdateDolphinStrings.py <po files directory> <localizables directory>

def strip_string_for_ios(str):
  # Strip (&X) for Japanese
  str = re.sub("\(&(\S)\)", "", str)
  
  # Remove & in &X
  str = re.sub("&(\S)", "\g<1>", str)
  
  # Strip trailing : and ： (full-width)
  if str.endswith(':') or str.endswith('：'):
    str = str[:-1]
  
  return str

for root, dirs, files in os.walk(sys.argv[1]):
  for po_path in files:
    # Skip the template file
    if po_path.endswith('.pot'):
      continue
      
    language = os.path.splitext(os.path.basename(po_path))[0]
    
    # Manual overrides
    if language == "pt":
      language = "pt-BR"
    elif language == "zh_CN":
      language = "zh-Hans"
    
    strings_path = os.path.join(sys.argv[2], language + '.lproj', 'Dolphin.strings')
    
    # Skip unsupported languages
    if not os.path.exists(strings_path):
      continue
    
    print('Processing ' + language + ' at ' + strings_path)
    
    with open(strings_path, 'w') as strings_file:
      po = polib.pofile(os.path.join(sys.argv[1], po_path))
      
      for entry in po:
        msgstr = entry.msgstr
        if not entry.msgstr:
          msgstr = entry.msgid
        
        msgid = strip_string_for_ios(entry.msgid)
        msgstr = strip_string_for_ios(msgstr)
        
        strings_file.write('"' + msgid.replace(r'"', r'\"') + '" = "' + msgstr.replace(r'\"', r'\\"').replace(r'"', r'\"') + '";\n')
