import os

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

def CheckPKG(context, name):
    context.Message( 'Checking for %s... ' % name )
    ret = context.TryAction('pkg-config --exists \'%s\'' % name)[0]
    context.Result( ret )
    return ret


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
        
        context.Result( ret )
        return ret
    
def GenerateRevFile(flavour, template, output):

    try:
        svnrev = os.popen('svnversion .').read().strip().split(':')[0]
    except:
        svnrev = ""
        
    revstr = svnrev + "-" + flavour 
    tmpstr = open(template, "r").read().replace("$WCREV$",revstr)
    template.close()

    outfile = open(output, 'w') 
    outfile.write(tmpstr +"\n")
    outfile.close()
    
    return
