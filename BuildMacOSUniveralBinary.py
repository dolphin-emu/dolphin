"""
The current tooling supported in CMake, Homebrew, and QT5 are insufficient for creating
MacOSX universal binaries automatically for applications like Dolphin which have more 
complicated build requirements (like different libraries, build flags and source files
for each target architecture). 

So instead, this script manages the conifiguration and compilation of distinct builds
and project files for each target architecture and then merges the two binaries into
a single universal binary. 

Running this script will:
1) Generate Xcode project files for the ARM build (if project files don't already exist)
2) Generate Xcode project files for the x64 build (if project files don't already exist)
3) Build the ARM project for the selected build_target
4) Build the x64 project for the selected build_target
5) Generates universal .app packages combining the ARM and x64 packages
6) Utilizes the lipo tool to combine the binary objects inside each of the packages
   into universal binaries
7) Code signs the final universal binaries using the specified codesign_identity
"""

##BEGIN CONFIG##
#Location of destination universal binary
dst_app = "universal/"
#Build Target (dolphin-emu to just build the emulator and skip the tests)
build_target = "ALL_BUILD"

#Locations to pkg config files for arm and x64 libraries
#The default values of these paths are taken from the default
#paths used for homebrew 
arm_pkg_config_path='/opt/homebrew/lib/pkgconfig'
x64_pkg_config_path='/usr/local/lib/pkgconfig'
 
#Locations to qt5 directories for arm and x64 libraries
#The default values of these paths are taken from the default
#paths used for homebrew 
arm_qt5_path='/opt/homebrew/opt/qt5'
x64_qt5_path='/usr/local/opt/qt5'

# Identity to use for code signing. "-" indicates that the app will not 
# be cryptographically signed/notarized but will instead just use a
# SHA checksum to verify the integrity of the app. This doesn't
# protect against malicious actors, but it does protect against
# running corrupted binaries and allows for access to the extended
# permisions needed for ARM builds

codesign_identity ='"-"'
    
##END CONFIG##

import glob
import sys
import os
import shutil
import filecmp                    
 

#Configure ARM project files if they don't exist 
if not os.path.exists("arm"):
  os.mkdir("arm");
  os.chdir("arm");

  os.system('PKG_CONFIG_PATH="'+arm_pkg_config_path+'" Qt5_DIR="'+arm_qt5_path+'" CMAKE_OSX_ARCHITECTURES=arm64 arch -arm64 cmake ../../ -G Xcode');
  os.chdir("..");       

#Configure x64 project files if they don't exist 
if not os.path.exists("x64"):
  os.mkdir("x64");
  os.chdir("x64");

  os.system('PKG_CONFIG_PATH="'+x64_pkg_config_path+'"  Qt5_DIR="'+x64_qt5_path+'" CMAKE_OSX_ARCHITECTURES=x86_64 arch -x86_64 cmake ../../ -G Xcode')
  os.chdir("..");       

#Build ARM and x64 projects 
os.system('xcodebuild -project arm/dolphin-emu.xcodeproj -target "'+build_target+'" -configuration Release');
os.system('xcodebuild -project x64/dolphin-emu.xcodeproj -target "'+build_target+'" -configuration Release');

#Merge ARM and x64 binaries into universal binaries

#Source binaries to merge together
src_app0 = "arm/Binaries/release"
src_app1 = "x64/Binaries/release"


if os.path.exists(dst_app): shutil.rmtree(dst_app)
os.mkdir(dst_app);

def lipo(path0,path1,dst):
  cmd = 'lipo -create -output "'+dst + '" "' + path0 +'" "' + path1+'"'
  print(cmd)
  os.system(cmd)
def recursiveMergeBinaries(src0,src1,dst):
  #loop over all files in src0
  for newpath0 in glob.glob(src0+"/*"):
    filename = os.path.basename(newpath0);
    newpath1 = os.path.join(src1,filename);
    new_dst_path = os.path.join(dst,filename);
    if not os.path.islink(newpath0):
      if os.path.exists(newpath1):
        if os.path.isdir(newpath1):
          os.mkdir(new_dst_path);
          #recurse into directories
          recursiveMergeBinaries(newpath0,newpath1,new_dst_path)
        else:
          if filecmp.cmp(newpath0,newpath1):
            #copy files that are the same
            shutil.copy(newpath0,new_dst_path);
          else:
            #lipo together files that are different
            lipo(newpath0,newpath1,new_dst_path)
      else:
        #copy files that don't exist in path1
        shutil.copy(newpath0,new_dst_path)
  
  #loop over files in src1 and copy missing things over to dst
  for newpath1 in glob.glob(src1+"/*"):
    filename = os.path.basename(newpath0);
    newpath0 = os.path.join(src0,filename);
    new_dst_path = os.path.join(dst,filename);
    if not os.path.exists(newpath0) and not os.path.islink(newpath1):
      shutil.copytree(newpath1,new_dst_path);  
  
  #fix up symlinks for path0
  for newpath0 in glob.glob(src0+"/*"):
    filename = os.path.basename(newpath0);
    new_dst_path = os.path.join(dst,filename);
    if os.path.islink(newpath0):
      relative_path = os.path.relpath(os.path.realpath(newpath0),src0)
      print(relative_path,new_dst_path)
      os.symlink(relative_path,new_dst_path);
  #fix up symlinks for path1
  for newpath1 in glob.glob(src1+"/*"):
    filename = os.path.basename(newpath1);
    new_dst_path = os.path.join(dst,filename);
    newpath0 = os.path.join(src0,filename);
    if os.path.islink(newpath1) and not os.path.exists(newpath0):
      relative_path = os.path.relpath(os.path.realpath(newpath1),src1)
      print(relative_path,new_dst_path)
      os.symlink(relative_path,new_dst_path);
                                            
  return;

#create univeral binary
recursiveMergeBinaries(src_app0,src_app1,dst_app); 
#codesign
os.system("codesign --deep --force -s "+codesign_identity+" " +dst_app +"/*");
