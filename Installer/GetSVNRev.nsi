OutFile "GetSVNRev.exe"
SilentInstall silent

Section
 ; Create template for SubWCRev
 FileOpen $R0 "svnrev_template.txt" w
  FileWrite $R0 '!define PRODUCT_VERSION "$$WCREV$$"'
 FileClose $R0
 ; Make a file with only rev # in it
 Exec "..\Source\Core\Common\SubWCRev.exe ..\ svnrev_template.txt svnrev.txt"
SectionEnd