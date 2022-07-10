set argument1=%1
set argument2=%2
if not exist %2 mkdir %2
cd %2
tar.exe -a -c -f out.zip output.dtm output.dtm.sav output.json
rename out.zip %1.cit
del output.dtm
del output.dtm.sav
del output.json