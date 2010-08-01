import os
import platform

# taken from scons wiki
def CheckPKGConfig(context, version):
    context.Message( 'Checking for pkg-config version > %s... ' % version)
    ret = context.TryAction('pkg-config --atleast-pkgconfig-version=%s' % version)[0]
    context.Result( ret )
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
              \n""", '.cpp')
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
    if platform.system().lower() == 'windows':
        return 0
    ret = 1
    if not ConfigPKG(context, name.lower()):
        ret = CheckLib(context, name)

    context.Result(ret)
    return int(ret)

def CheckSDL(context, version):
    context.Message( 'Checking for SDL lib version > %s... ' % version)
    if platform.system().lower() == 'windows':
        return 1
    sdl_config = context.env.WhereIs('sdl-config')
    if sdl_config == None:
        ret = 0
    else:
        found_ver = os.popen('sdl-config --version').read().strip()
        required = [int(n) for n in version.split(".")]
        found = [int(n) for n in found_ver.split(".")]
        ret = (found >= required)

        context.Result(ret)
        if ret:
            context.env.ParseConfig('sdl-config --cflags --libs')
            ret = CheckLib(context, 'SDL')
        return int(ret)

def CheckPortaudio(context, version):
    found = 0
    if CheckPKG(context, 'portaudio'):
        context.Message( 'Checking for lib portaudio version > %s... ' % version)
        found = context.TryRun("""
              #include <portaudio.h>
              #include <stdio.h>
              int main(int argc, char **argv) {
                printf("%d", Pa_GetVersion());
                return 0;
              }
              \n""", '.cpp')[1]

    if found:
        ret = (version <= found)
    else:
        ret = 0

    context.Result(ret)
    return int(ret)

def GenerateRevFile(flavour, template, output):
    try:
        svnrev = os.popen('svnversion ' + os.path.dirname(template)).\
            read().strip().split(':')[0]
    except:
        svnrev = ''

    if flavour:
        svnrev += '-' + flavour

    if output:
        tmpstr = open(template, 'r').read().\
            replace("$WCMODS?$WCREV$M:$WCREV$$", svnrev)
        outfile = open(output, 'w')
        outfile.write(tmpstr + '\n')
        outfile.close()

    return svnrev
