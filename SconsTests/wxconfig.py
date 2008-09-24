#
# SCons python library: wxWidgets configuration
#
# \author Willem van Engen <wvengen@stack.nl>
# \date created 2006/10/16
# \version v 1.3.2.2 2006/11/24 14:26:47 willem Exp
#
import os, os.path
import re
import sys
import glob
import string

# Runs a system command and returns return value and output
def SystemBacktick(program):
    # Run and return result
    pipe = os.popen(program + ' 2>&1') # TODO: properly redirect stderr to stdout
    output = pipe.read()
    retcode = pipe.close() or 0
    return [retcode, output]

def SystemWXConfig(env, args):
    if env['PLATFORM'] == 'win32':
        return SystemBacktick(env['wxconfig']+' --wxcfg='+env['ENV']['WXCFG']+' '+args+env['wxconfig_postargs'])
    else:
        return SystemBacktick(env['wxconfig']+' '+args+env['wxconfig_postargs'])

# Check version of wx-config
# It succeeds with a warning if version check failed.
def CheckWXConfigVersion(context, version):
    releaseversion = SystemWXConfig(context.env,'--release')[1]
    try:
        if float(version) > float(releaseversion.strip()):
            return False
    except (ValueError, TypeError):
        context.Message('version check failed, but ok... ')
        return True
    return True

# Check if requested components exist in wxWidgets
def CheckWXConfigComponents(context, libraries):
    # set components, method depending on wxWidgets version
    if CheckWXConfigVersion(context, '2.6'):
        context.env['wxconfig_postargs'] += ' '+string.join(libraries,',')
        return SystemWXConfig(context.env, '--libs ')[0] == 0
    # version 2.4 or below, only gl is an optional component with special flag
    if 'gl' in libraries:
        context.env['wxconfig'] += ' --gl-libs'
        return SystemWXConfig(context.env, '--libs')[0] == 0

    # no optional components can be checked, it should be allright
    return True


# Find wx-config with suitable settings. It tries to use a debug configuration if requested,
# but reverts to release or default when that fails.
def CheckWXConfigWin(context, version, debug):
    context.Message('Checking for wxWidgets >= %s... '%version)

    # Some environment variables are required for wx-config to work, check them.
    if 'WXWIN' not in context.env['ENV']:
        # TODO: maybe try some default location like "C:\Program Files\wxWidgets-*" or registry
        context.Message('please set WXWIN in environment... ')
        return False

    # If there's no WXCFG, either there is only one config or we try to find out.
    if 'WXCFG' not in context.env['ENV']:
        # TODO: try to find one in some sensible order from alternatives
        # Current guess is: visual studio static, non-unicode

        # Try debugging version first if requested, else fallback to release
        if debug:
            context.env['ENV']['WXCFG'] = 'vc_lib\mswd'
            if SystemWXConfig(context.env,'--libs')[0] == 0:
                return CheckWXConfigVersion(context, version)

        # Non-debug
        context.env['ENV']['WXCFG'] = 'vc_lib\msw'
        if SystemWXConfig(context.env,'--libs')[0] == 0:
            # this is the only configuration: use it
            return CheckWXConfigVersion(context, version)

        context.Message('please set WXCFG in environment... ')
        return False

    # WXCFG is in environment, nice.
    # Try a debugging version if requested: postfix WXCFG with 'd'
    if debug:
        oldwxcfg = context.env['ENV']['WXCFG']
        context.env['ENV']['WXCFG'] += 'd'
        if SystemWXConfig(context.env,'--libs')[0] == 0:
            return CheckWXConfigVersion(context, version)
        # Failed: revert to plain WXCFG, use existing environment
        context.env['ENV']['WXCFG'] = oldwxcfg

    if SystemWXConfig(context.env,'--libs')[0] == 0:
        return CheckWXConfigVersion(context, version)

    # Everything failed ... 
    return False


def CheckWXConfigPosixFind(context, debug):
    # Find a wx-config compatible pathname
    # wx*-config --> wx*-[0-9]+\.[0-9]+-config / wx<platform>-<version>-config

    dbglist = []
    rellist = []
    cfgre = re.compile('.*?\/wx(\w+?)(d?)-(\d+\.\d+)-config')
    for dir in context.env['ENV']['PATH'].split(':'):
        for cfgprog in glob.glob(os.path.join(dir,'wx*-config')):
            m = cfgre.match(cfgprog)
            if m and not m.group(1) == 'base':
                # add to either debug or release list
                if m.group(2) == 'release':
                    rellist.append(cfgprog)
                else:
                    dbglist.append(cfgprog)

    # TODO: sort on version

    # Now pick the right one
    if debug and len(dbglist)>0:
        return dbglist[0]

    if len(rellist)>0:
        return rellist[0]

    # Too bad
    return False


def CheckWXConfigPosix(context, version, debug):
    # TODO: try several wx-config names
    context.Message('Checking for wxWidgets >= %s... '%version)
    
    # If supplied wx-config doesn't work, try to find another one
    if SystemWXConfig(context.env,'--libs')[0] != 0:
        wx_prog = CheckWXConfigPosixFind(context, debug)
        if not wx_prog:
            context.Message('not found... ')
            return False
        context.env['wxconfig'] = wx_prog

    if not debug:
        return CheckWXConfigVersion(context, version)

    # use `wx-config --debug` if it's in its help
    helpoutput = SystemWXConfig(context.env,'--help')[1]
    if helpoutput.find('--debug') != -1:
        context.Message('--debug')
        if SystemWXConfig(context.env,'--debug --libs')[0] == 0:
            context.env['wxconfig'] = context.env['wxconfig'] +' --debug'
            return CheckWXConfigVersion(context, version)

    # If it's plain wx-config, we may need to look for another one for debugging
    if context.env['wxconfig'] == 'wx-config':
        wx_prog = CheckWXConfigPosixFind(context, debug)
        if wx_prog:
            context.env['wxconfig']  = wx_prog

    # TODO: possibly warning message when using release instead of debug
    return CheckWXConfigVersion(context, version)


def CheckWXConfig(context, version, components, debug = False):
    context.env['wxconfig_postargs']= ''

    # Try to find it in path
    wx_prog = context.env.WhereIs('wx-config')
    if wx_prog == None:
        # You could supply wx-config.exe as a fallback option.
        #wx_prog = os.path.join('scons','wx-config')
        context.Message('wx-config not found...')
        return False
    context.env['wxconfig'] = wx_prog

    # Get wx-config invocation and check version
    if context.env['PLATFORM'] == 'win32':
        res = CheckWXConfigWin(context, version, debug)
    else:
        res = CheckWXConfigPosix(context, version, debug)

    # Make sure we have the required libraries
    if res:
        res = CheckWXConfigComponents(context, components)
        if not res:
            context.Message('not all components found ['+string.join(components,',')+']... ')

    context.Result(res)
    return res

def ParseWXConfig(env):

    # Windows doesn't work with ParseConfig (yet) :(
    if env['PLATFORM'] == 'win32':
        # Use wx-config, yay!
        # ParseConfig() on windows is broken, so the following is done instead
        cflags = SystemWXConfig(env,'--cxxflags')[1]
        env.AppendUnique(CPPFLAGS = cflags.strip().split(' '))
        libs = SystemWXConfig(env,'--libs')[1]
        env.AppendUnique(LINKFLAGS = libs.strip().split(' '))
    else:
        # Here ParseConfig should really work
        env.ParseConfig(env['wxconfig']+' --cxxflags --libs'+env['wxconfig_postargs'])
