#!/bin/sh
# OS X .dmg maker for distributable builds
# Courtesy of WntrMute
# How to use:
#  run scons so that a complete and valid build is made in the normal Binary path
#  run this script :)

trap "echo; exit" SIGINT SIGTERM

temp_dir="Dolphin-r`svn info | grep "Revision" | awk '{print $2}'`"

fix_shared_object_depends() {
	search_string=$1

	# Get list of files to work on
	file_list=`find $temp_dir/Dolphin.app -name *.dylib`

	# Loop over the files, and update the path
	for file in ${file_list}; do
		orig_paths=(`otool -L  ${file} | grep ${search_string} | awk '{print $1}'`)

		for orig_path in ${orig_paths[@]}; do
			if test "x${orig_path}" != x; then
				new_path=`echo ${orig_path} | xargs basename`
				echo "$file\t$orig_path"

				cp ${orig_path} $temp_dir/Dolphin.app/Contents/MacOS/${new_path}
				install_name_tool -change ${orig_path} @executable_path/${new_path} ${file}
			fi
		done
	done

	# wxw shoves all the paths into one string, so the looping is really just for dealing with wxw crap
	orig_paths=(`otool -L  $temp_dir/Dolphin.app/Contents/MacOS/Dolphin | grep ${search_string} | awk '{print $1}'`)
	
	for orig_path in ${orig_paths[@]}; do
		if test "x${orig_path}" != x; then
			new_path=`echo ${orig_path} | xargs basename`
			cp ${orig_path} $temp_dir/Dolphin.app/Contents/MacOS/${new_path}
			install_name_tool -change ${orig_path} @executable_path/${new_path} $temp_dir/Dolphin.app/Contents/MacOS/Dolphin
			echo "Fixing $orig_path"
		fi
	done
}

cd Binary 2>/dev/null
if [ $? != 0 ]; then echo "Did you build dolphin yet?"; exit; fi
rm -rf $temp_dir
mkdir -p $temp_dir/Dolphin.app
cp -r Darwin-i386/Dolphin.app $temp_dir
fix_shared_object_depends libwx
fix_shared_object_depends libSDL
fix_shared_object_depends libGLEW
fix_shared_object_depends libz

mkdir -p $temp_dir/Dolphin.app/Contents/Library/Frameworks/Cg.framework
cp /Library/Frameworks/Cg.framework/Cg $temp_dir/Dolphin.app/Contents/Library/Frameworks/Cg.framework/Cg

find $temp_dir -name .svn -exec rm -fr {} \; 2>/dev/null
rm $temp_dir.dmg 2>/dev/null
echo "Creating dmg"
hdiutil create -srcfolder $temp_dir -format UDBZ $temp_dir.dmg
rm -rf $temp_dir
