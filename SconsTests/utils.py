import os
import platform

# methods that should be added to env
def filterWarnings(self, flags):
    return ' '.join(
        flag
        for flag in flags
        if not flag.startswith('-W')
        )

# taken from scons wiki
def CheckPKGConfig(context, version):
    context.Message( 'Checking for pkg-config version > %s... ' % version)
    ret = context.TryAction('pkg-config --atleast-pkgconfig-version=%s' % version)[0]
    context.Result( ret )
    return ret

def CheckFramework(context, name):
    ret = 0
    if (platform.system().lower() == 'darwin'):
        context.Message( '\nLooking for framework %s... ' % name )
        lastLINKFLAGS = context.env['LINKFLAGS']
        context.env.Append(LINKFLAGS = [ '-framework', name ])
        ret = context.TryLink("""
              int main(int argc, char **argv) {
                return 0;
              }
              """, '.c')
        if not ret:
            context.env.Replace(LINKFLAGS = lastLINKFLAGS)

    return ret

# TODO: We should use the scons one instead
def CheckLib(context, name):
    context.Message( 'Looking for lib %s... ' % name )
    lastLIBS = context.env['LIBS']
    context.env.Append(LIBS = [name])
    ret = context.TryLink("""
              int main(int argc, char **argv) {
                return 0;
              }
              """,'.c')
    if not ret:
        context.env.Replace(LIBS = lastLIBS)

    return ret

def ConfigPKG(context, name):
    context.Message( '\nUsing pkg-config for %s... ' % name )
    ret = context.TryAction('pkg-config --exists \'%s\'' % name)[0]
    context.Result(  ret )
    if ret:
        context.env.ParseConfig('pkg-config --cflags --libs \'%s\'' % name)
    return int(ret)
    
def CheckPKG(context, name):
    context.Message( 'Checking for %s... ' % name )
    ret = 1
    if not CheckFramework(context, name):
        if not ConfigPKG(context, name.lower()):
            ret = CheckLib(context, name) 

    context.Result(ret)
    return int(ret)



def CheckSDL(context, version):
    context.Message( 'Checking for sdl lib version > %s... ' % version)
    sdl_config = context.env.WhereIs('sdl-config')
    if sdl_config == None:
        ret = 0
    else:
        found_ver = os.popen('sdl-config --version').read().strip()
        required = [int(n) for n in version.split(".")]
        found  = [int(n) for n in found_ver.split(".")]
        ret = (found >= required)
        
        context.Result(ret)
        if ret:
            context.env.ParseConfig('sdl-config --cflags --libs')
        return int(ret)
    
def CheckPortaudio(context, version):
    context.Message( 'Checking for lib portaudio version > %s... ' % version)
    context.env.Append(LIBS = 'portaudio')
    found  = context.TryRun("""
              #include <portaudio.h>
              #include <stdio.h>
              int main(int argc, char **argv) {
                printf("%d", Pa_GetVersion());
                return 0;
              }
              """, '.c')[1]

    if found:
        ret = (version <= found)
    else:
        ret = 0

    context.Result(ret)
    return int(ret)


    
def GenerateRevFile(flavour, template, output):

    try:
        svnrev = os.popen('svnversion .').read().strip().split(':')[0]
    except:
        svnrev = ""
        
    revstr = svnrev + "-" + flavour 
    tmpstr = open(template, "r").read().replace("$WCREV$",revstr)

    outfile = open(output, 'w') 
    outfile.write(tmpstr +"\n")
    outfile.close()
    
    return revstr
