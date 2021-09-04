

# Global Python includes
import os
import re
import subprocess
import sys

from os import mkdir, environ
from os.path import abspath, basename, dirname, expanduser, normpath
from os.path import exists,  isdir, isfile
from os.path import join as joinpath, split as splitpath

# SCons includes
import SCons
import SCons.Node

extra_python_paths = [
    Dir('scripts/python').srcnode().abspath, # gem5 includes
    ]

sys.path[1:1] = extra_python_paths

from m5.util import compareVersions, readCommand
from m5.util.terminal import get_termcap

print "SESC Simulator"

help_texts = {
    "options" : "",
    "global_vars" : "",
    "local_vars" : ""
}

Export("help_texts")

def AddLocalOption(*args, **kwargs):
    col_width = 30

    help = "  " + ", ".join(args)
    if "help" in kwargs:
        length = len(help)
        if length >= col_width:
            help += "\n" + " " * col_width
        else:
            help += " " * (col_width - length)
        help += kwargs["help"]
    help_texts["options"] += help + "\n"

    AddOption(*args, **kwargs)

AddLocalOption('--colors', dest='use_colors', action='store_true',
               help="Add color to abbreviated scons output")
AddLocalOption('--no-colors', dest='use_colors', action='store_false',
               help="Don't add color to abbreviated scons output")
AddLocalOption('--verbose', dest='verbose', action='store_true',
               help='Print full tool command lines')

termcap = get_termcap(GetOption('use_colors'))

use_vars = set([ 'AS', 'AR', 'CC', 'CXX', 'HOME', 'LD_LIBRARY_PATH',
                 'LIBRARY_PATH', 'PATH', 'PKG_CONFIG_PATH', 'PYTHONPATH',
                 'RANLIB', 'SWIG' ])



use_env = {}
for key,val in os.environ.iteritems():
    if key in use_vars:
        use_env[key] = val

main = Environment(ENV=use_env)
main.Decider('MD5-timestamp')
main.root = Dir(".")      
main.srcdir = Dir("src")  

main_dict_keys = main.Dictionary().keys()

# Check that we have a C/C++ compiler
if not ('CC' in main_dict_keys and 'CXX' in main_dict_keys):
    print "No C++ compiler installed (package g++ on Ubuntu and RedHat)"
    Exit(1)

# Check that swig is present
if not 'SWIG' in main_dict_keys:
    print "swig is not installed (package swig on Ubuntu and RedHat)"
    Exit(1)

#Default(environ.get('SESC_DEFAULT_BINARY', 'build/SMP_BUS/sesc.opt'))
#Default(environ.get('SESC_DEFAULT_BINARY', 'build/SMP_BOOKSIM/sesc.opt'))
Default(environ.get('SESC_DEFAULT_BINARY', ['build/SMP_BOOKSIM/sesc.opt',
                                            'build/SMP_BOOKSIM_DRAMSIM2/sesc.opt',
                                                'build/SMP_BUS/sesc.opt']))

def rfind(l, elt, offs = -1):
    for i in range(len(l)+offs, 0, -1):
        if l[i] == elt:
            return i
    raise ValueError, "element not found"

def makePathListAbsolute(path_list, root=GetLaunchDir()):
    return [abspath(joinpath(root, expanduser(str(p))))
            for p in path_list]

BUILD_TARGETS[:] = makePathListAbsolute(BUILD_TARGETS)

variant_paths = []
build_root = None

for t in BUILD_TARGETS:
    path_dirs = t.split('/')
    try:
        build_top = rfind(path_dirs, 'build', -2)
    except:
        print "Error: no non-leaf 'build' dir found on target path", t
        Exit(1)
    this_build_root = joinpath('/',*path_dirs[:build_top+1])
    if not build_root:
        build_root = this_build_root
    else:
        if this_build_root != build_root:
            print "Error: build targets not under same build root\n"\
                  "  %s\n  %s" % (build_root, this_build_root)
            Exit(1)
    variant_path = joinpath('/',*path_dirs[:build_top+2])
    if variant_path not in variant_paths:
        variant_paths.append(variant_path)

if not isdir(build_root):
    mkdir(build_root)
main['BUILDROOT'] = build_root
Export('main')

main.SConsignFile(joinpath(build_root, "sconsign"))
main.SetOption('duplicate', 'soft-copy')

base_dir = main.srcdir.abspath
Export('base_dir')


def strip_build_path(path, env):
    path = str(path)
    variant_base = env['BUILDROOT'] + os.path.sep
    if path.startswith(variant_base):
        path = path[len(variant_base):]
    elif path.startswith('build/'):
        path = path[6:]
    return path


# Generate a string of the form:
#   common/path/prefix/src1, src2 -> tgt1, tgt2
# to print while building.
class Transform(object):
    # all specific color settings should be here and nowhere else
    tool_color = termcap.Normal
    pfx_color = termcap.Yellow
    srcs_color = termcap.Yellow + termcap.Bold
    arrow_color = termcap.Blue + termcap.Bold
    tgts_color = termcap.Yellow + termcap.Bold

    def __init__(self, tool, max_sources=99):
        self.format = self.tool_color + (" [%8s] " % tool) \
                      + self.pfx_color + "%s" \
                      + self.srcs_color + "%s" \
                      + self.arrow_color + " -> " \
                      + self.tgts_color + "%s" \
                      + termcap.Normal
        self.max_sources = max_sources

    def __call__(self, target, source, env, for_signature=None):
        # truncate source list according to max_sources param
        source = source[0:self.max_sources]
        def strip(f):
            return strip_build_path(str(f), env)
        if len(source) > 0:
            srcs = map(strip, source)
        else:
            srcs = ['']
        tgts = map(strip, target)
        # surprisingly, os.path.commonprefix is a dumb char-by-char string
        # operation that has nothing to do with paths.
        com_pfx = os.path.commonprefix(srcs + tgts)
        com_pfx_len = len(com_pfx)
        if com_pfx:
            # do some cleanup and sanity checking on common prefix
            if com_pfx[-1] == ".":
                # prefix matches all but file extension: ok
                # back up one to change 'foo.cc -> o' to 'foo.cc -> .o'
                com_pfx = com_pfx[0:-1]
            elif com_pfx[-1] == "/":
                # common prefix is directory path: OK
                pass
            else:
                src0_len = len(srcs[0])
                tgt0_len = len(tgts[0])
                if src0_len == com_pfx_len:
                    # source is a substring of target, OK
                    pass
                elif tgt0_len == com_pfx_len:
                    # target is a substring of source, need to back up to
                    # avoid empty string on RHS of arrow
                    sep_idx = com_pfx.rfind(".")
                    if sep_idx != -1:
                        com_pfx = com_pfx[0:sep_idx]
                    else:
                        com_pfx = ''
                elif src0_len > com_pfx_len and srcs[0][com_pfx_len] == ".":
                    # still splitting at file extension: ok
                    pass
                else:
                    # probably a fluke; ignore it
                    com_pfx = ''
        # recalculate length in case com_pfx was modified
        com_pfx_len = len(com_pfx)
        def fmt(files):
            f = map(lambda s: s[com_pfx_len:], files)
            return ', '.join(f)
        return self.format % (com_pfx, fmt(srcs), fmt(tgts))

Export('Transform')

# enable the regression script to use the termcap
main['TERMCAP'] = termcap

if GetOption('verbose'):
    def MakeAction(action, string, *args, **kwargs):
        return Action(action, *args, **kwargs)
else:
    MakeAction = Action
    main['CCCOMSTR']        = Transform("CC")
    main['CXXCOMSTR']       = Transform("CXX")
    main['ASCOMSTR']        = Transform("AS")
    main['SWIGCOMSTR']      = Transform("SWIG")
    main['ARCOMSTR']        = Transform("AR", 0)
    main['LINKCOMSTR']      = Transform("LINK", 0)
    main['RANLIBCOMSTR']    = Transform("RANLIB", 0)
    main['M4COMSTR']        = Transform("M4")
    main['SHCCCOMSTR']      = Transform("SHCC")
    main['SHCXXCOMSTR']     = Transform("SHCXX")
Export('MakeAction')

CXX_version = readCommand([main['CXX'],'--version'], exception=False)
CXX_V = readCommand([main['CXX'],'-V'], exception=False)

main['GCC'] = CXX_version and CXX_version.find('g++') >= 0
if main['GCC']:
    #main.Append(CCFLAGS=['-pipe'])
    main.Append(CCFLAGS=['-fno-strict-aliasing'])
    main.Append(CCFLAGS=['-momit-leaf-frame-pointer'])
    #main.Append(CCFLAGS=['-march=nocona'])
    main.Append(CCFLAGS=['-funroll-loops'])
    main.Append(CCFLAGS=['-fsched-interblock'])
    main.Append(CCFLAGS=['-ffast-math'])
    main.Append(CCFLAGS=['-freg-struct-return'])
    # Enable -Wall and then disable the few warnings that we
    # consistently violate
    main.Append(CCFLAGS=['-Wall', '-Wno-unused', '-Wno-sign-compare'])
    #main.Append(CCFLAGS=['-Wall', '-Wno-sign-compare', '-Wundef'])
    # We always compile using C++11, but only gcc >= 4.7 and clang 3.1
    # actually use that name, so we stick with c++0x
    main.Append(CXXFLAGS=['-std=c++0x'])
    # Add selected sanity checks from -Wextra
    main.Append(CXXFLAGS=['-Wmissing-field-initializers',
                          '-Woverloaded-virtual'])
    # Check for a supported version of gcc, >= 4.4 is needed for c++0x
    # support. See http://gcc.gnu.org/projects/cxx0x.html for details
    gcc_version = readCommand([main['CXX'], '-dumpversion'], exception=False)
    if compareVersions(gcc_version, "4.4") < 0:
        print 'Error: gcc version 4.4 or newer required.'
        print '       Installed version:', gcc_version
        Exit(1)

    main['GCC_VERSION'] = gcc_version

    # Check for versions with bugs
    if not compareVersions(gcc_version, '4.4.1') or \
       not compareVersions(gcc_version, '4.4.2'):
        print 'Info: Tree vectorizer in GCC 4.4.1 & 4.4.2 is buggy, disabling.'
        main.Append(CCFLAGS=['-fno-tree-vectorize'])
else:
    Exit(1)



# Set up common yacc/bison flags (needed for Ruby)
main['YACCFLAGS'] = '-d'
main['YACCHXXFILESUFFIX'] = '.hh'



# Check for YACC
if not main.has_key('YACC'):
    print 'Error: YACC utility not found.'
    Exit(1)

# Check for LEX
if not main.has_key('LEX'):
    print 'Error: LEX utility not found.'
    Exit(1)






conf = Configure(main,
                 conf_dir = joinpath(build_root, '.scons_config'),
                 log_file = joinpath(build_root, 'scons_config.log'))


main = conf.Finish()


sticky_vars = Variables(args=ARGUMENTS)
Export('sticky_vars')

# Sticky variables that should be exported
export_vars = []
Export('export_vars')

# For Ruby
all_protocols = []
Export('all_protocols')
all_systems = []
Export('all_systems')
all_networks = []
Export('all_networks')
all_memory = []
Export('all_memory')
protocol_dirs = []
Export('protocol_dirs')
slicc_includes = []
Export('slicc_includes')

all_libs = []
Export('all_libs')
all_libs_cpppath = dict()
Export('all_libs_cpppath')

# Walk the tree and execute all SConsopts scripts that wil add to the
# above variables
if not GetOption('verbose'):
    print "Reading SConsopts"
for bdir in [ base_dir ]:
    if not isdir(bdir):
        print "Error: directory '%s' does not exist" % bdir
        Exit(1)
    for root, dirs, files in os.walk(bdir):
        if 'SConsopts' in files:
            if GetOption('verbose'):
                print "Reading", joinpath(root, 'SConsopts')
            SConscript(joinpath(root, 'SConsopts'))


sticky_vars.AddVariables(
    #EnumVariable('PROTOCOL', 'Coherence protocol', 'None',
    #              all_protocols),
    EnumVariable('SYSTEM', 'System', 'None',
                  all_systems),
    EnumVariable('NETWORK', 'Network', 'None',
                  all_networks),
    EnumVariable('MEMORY', 'Memory Model', 'None',
                  all_memory),
    )

# These variables get exported to #defines in config/*.hh (see src/SConscript).
export_vars += [ 'SYSTEM', 'NETWORK', 'MEMORY'] #'PROTOCOL', 



for variant_path in variant_paths:
    print "Building in", variant_path

    # Make a copy of the build-root environment to use for this config.
    env = main.Clone()
    env['BUILDDIR'] = variant_path

    # variant_dir is the tail component of build path, and is used to
    # determine the build parameters (e.g., 'ALPHA_SE')
    (build_root, variant_dir) = splitpath(variant_path)

    # Set env variables according to the build directory config.
    sticky_vars.files = []
    # Variables for $BUILD_ROOT/$VARIANT_DIR are stored in
    # $BUILD_ROOT/variables/$VARIANT_DIR so you can nuke
    # $BUILD_ROOT/$VARIANT_DIR without losing your variables settings.

    '''
    current_vars_file = joinpath(build_root, 'variables', variant_dir)
    if isfile(current_vars_file):
        sticky_vars.files.append(current_vars_file)
        print "Using saved variables file %s" % current_vars_file
    else:
        # Build dir-specific variables file doesn't exist.

        # Make sure the directory is there so we can create it later

        opt_dir = dirname(current_vars_file)
        if not isdir(opt_dir):
            mkdir(opt_dir)

        opts_dir = joinpath(main.root.abspath, 'build_opts')
        default_vars_files = [joinpath(opts_dir, variant_dir)]
        existing_files = filter(isfile, default_vars_files)
        if existing_files:
            default_vars_file = existing_files[0]
            sticky_vars.files.append(default_vars_file)
            print "Variables file %s not found,\n  using defaults in %s" \
                  % (current_vars_file, default_vars_file)
        else:
            print "Error: cannot find variables file %s or " \
                  "default file(s) %s" \
                  % (current_vars_file, ' or '.join(default_vars_files))
            Exit(1)
    '''

    opts_dir = joinpath(main.root.abspath, 'build_opts')
    default_vars_files = [joinpath(opts_dir, variant_dir)]
    existing_files = filter(isfile, default_vars_files)
    if existing_files:
        default_vars_file = existing_files[0]
        sticky_vars.files.append(default_vars_file)
        print "Using variable in %s" \
              % (default_vars_file)
    else:
        print "Error: cannot find variables file %s" \
              % (default_vars_files)
        Exit(1)

    # Apply current variable settings to env
    sticky_vars.Update(env)

    help_texts["local_vars"] += \
        "Build variables for %s:\n" % variant_dir \
                 + sticky_vars.GenerateHelpText(env)

    # Save sticky variable settings back to current variables file
    #sticky_vars.Save(current_vars_file, env)

    # The src/SConscript file sets up the build rules in 'env' according
    # to the configured variables.  It returns a list of environments,
    # one for each variant build (debug, opt, etc.)
    envList = SConscript('src/SConscript', variant_dir = variant_path,
                         exports = 'env')



# base help text
Help('''
Usage: scons [scons options] [build variables] [target(s)]

Extra scons options:
%(options)s

Global build variables:
%(global_vars)s

%(local_vars)s
''' % help_texts)
