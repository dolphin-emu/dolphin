# this can be used to upgrade disassemblies that aren't too annotated.
# won't do very well on the current zelda disasm.

import os
import sys

def GetPrefixLine(l, a):
  for s in a:
    if s[0:len(l)] == l:
      return s
  return ""
  
def GetComment(l):
  comment_start = l.find("//")
  if comment_start < 0:
    comment_start = l.find("->")
  if comment_start < 0:
    return ""
  
  while (l[comment_start-1] == ' ') or (l[comment_start-1] == '\t'):
    comment_start -= 1
    
  return l[comment_start:]


def main():
  old_lines = open("DSP_UC_Zelda.txt", "r").readlines()
  # for l in old_lines:
  #  print l
  new_lines = open("zeldanew.txt", "r").readlines()
  
  for i in range(0, len(old_lines)):
    prefix = old_lines[i][0:14]
    comment = GetComment(old_lines[i])
    new_line = GetPrefixLine(prefix, new_lines)
    if new_line:
      old_lines[i] = new_line[:-1] + comment[:-1] + "\n"
  
  for i in range(0, len(old_lines)):
    print old_lines[i],
   
  new_file = open("output.txt", "w")
  new_file.writelines(old_lines)
  
main()