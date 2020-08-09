#!/usr/local/bin/python3

import os
import polib
import re
import sys

# This tool finds strings in storyboard .strings files which already have translations
# in the po files, and replaces the English strings accordingly.

# Usage:
# python3 UpdateUIStrings.py <po files directory> <UI directory>

loaded_pos = {}

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
    
    po = polib.pofile(os.path.join(sys.argv[1], po_path))
    
    po_entries = {}
    
    # Load the po and its strings into a dictionary
    for entry in po:
      msgstr = entry.msgstr
      if not entry.msgstr:
        msgstr = entry.msgid
        
      msgid = strip_string_for_ios(entry.msgid)
      msgstr = strip_string_for_ios(msgstr)
      
      po_entries[msgid] = msgstr
      
    loaded_pos[language] = po_entries
      
found_dirs = []

# Find all directories with a en.lproj
for root, dirs, files in os.walk(sys.argv[2]):
  for dir in dirs:
    if not dir.endswith('en.lproj'):
      continue
    
    found_dirs.append(os.path.dirname(os.path.join(root, dir)))

for dir in found_dirs:
  for language, po_entries in loaded_pos.items():
    # Skip English
    if language == 'en':
      continue
    
    lproj_dir = os.path.join(dir, language + '.lproj')
    
    # Check if this language exists in this directory
    if not os.path.isdir(lproj_dir):
      continue
    
    # Find all strings files
    for root, dirs, files in os.walk(lproj_dir):
      for strings_path in files:
        if not strings_path.endswith('.strings'):
          continue
          
        with open(os.path.join(root, strings_path), 'r+') as strings_file:
          content = strings_file.read()
          
          to_replace = []
          
          # Check if the string is something we have a translation for
          for match_obj in re.finditer(r'"(.*)" = "(.*)";', content):
            if match_obj.group(2) in loaded_pos['en']:
              to_replace.append(match_obj.group(2))
          
          # Replace all matches
          for replace in to_replace:
            final_string = po_entries[replace]
            if replace == final_string:
              continue
            
            content = content.replace('"' + replace + '";', '"' + final_string + '";')
            
          strings_file.seek(0)
          strings_file.write(content)
          strings_file.truncate()
