import array
import bisect
import imp
import marshal
import os
import re
import sys
import zlib 


from os.path import basename, dirname, exists, isdir, isfile, join as joinpath

import SCons


Import('*')

# Children need to see the environment
Export('env')

build_env = [(opt, env[opt]) for opt in export_vars]

from m5.util import compareVersions


class SourceMeta(type):
    '''Meta class for source files that keeps track of all files of a
    particular type and has a get function for finding all functions
    of a certain type that match a set of guards'''
    def __init__(cls, name, bases, dict):
        super(SourceMeta, cls).__init__(name, bases, dict)
        cls.all = []

    def get(cls, **guards):
        '''Find all files that match the specified guards.  If a source
        file does not specify a flag, the default is False'''
        for src in cls.all:
            for flag,value in guards.iteritems():
                # if the flag is found and has a different value, skip
                # this file
                if src.all_guards.get(flag, False) != value:
                    break
            else:
                yield src

class SourceFile(object):
    '''Base object that encapsulates the notion of a source file.
    This includes, the source node, target node, various manipulations
    of those.  A source file also specifies a set of guards which
    describing which builds the source file applies to.  A parent can
    also be specified to get default guards from'''
    __metaclass__ = SourceMeta
    def __init__(self, source, parent=None, **guards):
        self.guards = guards
        self.parent = parent

        tnode = source
        if not isinstance(source, SCons.Node.FS.File):
            tnode = File(source)

        self.tnode = tnode
        self.snode = tnode.srcnode()

        for base in type(self).__mro__:
            if issubclass(base, SourceFile):
                base.all.append(self)

    @property
    def filename(self):
        return str(self.tnode)

    @property
    def dirname(self):
        return dirname(self.filename)

    @property
    def basename(self):
        return basename(self.filename)

    @property
    def extname(self):
        index = self.basename.rfind('.')
        if index <= 0:
            # dot files aren't extensions
            return self.basename, None

        return self.basename[:index], self.basename[index+1:]

    @property
    def all_guards(self):
        '''find all guards for this object getting default values
        recursively from its parents'''
        guards = {}
        if self.parent:
            guards.update(self.parent.guards)
        guards.update(self.guards)
        return guards

    def __lt__(self, other): return self.filename < other.filename
    def __le__(self, other): return self.filename <= other.filename
    def __gt__(self, other): return self.filename > other.filename
    def __ge__(self, other): return self.filename >= other.filename
    def __eq__(self, other): return self.filename == other.filename
    def __ne__(self, other): return self.filename != other.filename



class Source(SourceFile):
    '''Add a c/c++ source file to the build'''
    def __init__(self, source, Werror=False, swig=False, **guards):
        '''specify the source file, and any guards'''
        super(Source, self).__init__(source, **guards)

        self.Werror = Werror
        self.swig = swig

class LexSource(SourceFile):
    '''Add a Protocol Buffer to build'''

    def __init__(self, source, **guards):
        '''Specify the source file, and any guards'''
        super(LexSource, self).__init__(source, **guards)

        # Get the file name and the extension
        modname,ext = self.extname

        # Currently, we stick to generating the C++ headers, so we
        # only need to track the source and header.
        self.flex_file = File(joinpath(self.dirname, modname + 'lex.cpp'))
        if 'opt' in guards:
            self.flex_option = guards['opt']
        else:
            self.flex_option = ''

class YaccSource(SourceFile):
    '''Add a Protocol Buffer to build'''

    def __init__(self, source, **guards):
        '''Specify the source file, and any guards'''
        super(YaccSource, self).__init__(source, **guards)

        # Get the file name and the extension
        modname,ext = self.extname

        # Currently, we stick to generating the C++ headers, so we
        # only need to track the source and header.
        self.bison_file = File(joinpath(self.dirname, modname + '.tab.cpp'))
        self.bison_header = File(joinpath(self.dirname, modname + '.tab.hpp'))
        if 'opt' in guards:
            self.bison_option = guards['opt']
        else:
            self.bison_option = ''









# Children should have access
Export('Source')
Export('YaccSource')
Export('LexSource')



env.Append(CPPPATH=Dir('.'))

# Workaround for bug in SCons version > 0.97d20071212
# Scons bug id: 2006 gem5 Bug id: 308
for root, dirs, files in os.walk(base_dir, topdown=True):
    Dir(root[len(base_dir) + 1:])


########################################################################
#
# Walk the tree and execute all SConscripts in subdirectories
#

here = Dir('.').srcnode().abspath
for root, dirs, files in os.walk(base_dir, topdown=True):
    if root == here:
        # we don't want to recurse back into this SConscript
        continue

    if 'SConscript' in files:
        build_dir = joinpath(env['BUILDDIR'], root[len(base_dir) + 1:])
        SConscript(joinpath(root, 'SConscript'), variant_dir=build_dir)

#for opt in export_vars:
#    env.ConfigFile(opt)








for bison in YaccSource.all:
    env.Command([bison.bison_file, bison.bison_header], bison.tnode,
                    MakeAction('$YACC '+bison.bison_option+' -o$TARGET -d $SOURCE',
                               Transform("YACC")))

    # Add the C++ source file
    Source(bison.bison_file, **bison.guards)

for flex in LexSource.all:
    env.Command(flex.flex_file, flex.tnode,
                    MakeAction('$LEX '+flex.flex_option+' -o$TARGET $SOURCE',
                               Transform("LEX")))

    # Add the C++ source file
    Source(flex.flex_file, **flex.guards)










########################################################################
#
# Define binaries.  Each different build type (debug, opt, etc.) gets
# a slightly different build environment.
#

# List of constructed environments to pass back to SConstruct
envList = [] 



# Function to create a new build environment as clone of current
# environment 'env' with modified object suffix and optional stripped
# binary.  Additional keyword arguments are appended to corresponding
# build environment vars.
def makeEnv(label, objsfx, strip = False, **kwargs):
    # SCons doesn't know to append a library suffix when there is a '.' in the
    # name.  Use '_' instead.
    libname = 'sesc_' + label
    exename = 'sesc.' + label

    new_env = env.Clone(OBJSUFFIX=objsfx, SHOBJSUFFIX=objsfx + 's')
    new_env.Label = label
    new_env.Append(**kwargs)

    # Add additional warnings here that should not be applied to
    # the SWIG generated code
    new_env.Append(CXXFLAGS='-Wmissing-declarations')
            
    if env['GCC']:
        # If gcc supports it, also warn for deletion of derived
        # classes with non-virtual desctructors. For gcc >= 4.7 we
        # also have to disable warnings about the SWIG code having
        # potentially uninitialized variables.
        if compareVersions(env['GCC_VERSION'], '4.7') >= 0:
            new_env.Append(CXXFLAGS='-Wdelete-non-virtual-dtor')

    werror_env = new_env.Clone()
    werror_env.Append(CCFLAGS='-Werror')

    def make_obj(source, static, extra_deps = None):
        '''This function adds the specified source to the correct
        build environment, and returns the corresponding SCons Object
        nodes'''

        if source.Werror:
            env = werror_env
        else:
            env = new_env

        if 'lib' in source.guards:
            l_env = env.Clone()
            for ll in source.guards['lib'].split():
                if ll in all_libs_cpppath:
                    l_env.Append(CPPPATH=all_libs_cpppath[ll])
            env = l_env

        if static:
            obj = env.StaticObject(source.tnode)
        else:
            obj = env.SharedObject(source.tnode)

        if extra_deps:
            env.Depends(obj, extra_deps)

        return obj

    static_objs = \
        [ make_obj(s, True) for s in Source.get(main=False, skip_lib=False) ]
    #shared_objs = \
    #    [ make_obj(s, False) for s in Source.get(main=False, skip_lib=False) ]

    # First make a library of everything but main() so other programs can
    # link against 
    static_lib = new_env.StaticLibrary(libname, static_objs)
    #shared_lib = new_env.SharedLibrary(libname, shared_objs)

    # Now link a stub with main() and the static library.
    main_objs = [ make_obj(s, True) for s in Source.get(main=True) ]

    progname = exename
    if strip:
        progname += '.unstripped'

    targets = new_env.Program(progname, main_objs + static_lib)
    #targets = new_env.Program(progname, main_objs + static_objs)

    if strip:
        cmd = 'strip $SOURCE -o $TARGET'
        targets = new_env.Command(exename, progname,
                    MakeAction(cmd, Transform("STRIP")))

    new_env.SESCBinary = targets[0]
    envList.append(new_env)

# Start out with the compiler flags common to all compilers,
# i.e. they all use -g for opt and -g -pg for prof
ccflags = {'debug' : [], 'opt' : [''], 'fast' : [], 'prof' : ['-g', '-pg'],
           'perf' : ['-g']}

# Start out with the linker flags common to all linkers, i.e. -pg for
# prof, and -lprofiler for perf. The -lprofile flag is surrounded by
# no-as-needed and as-needed as the binutils linker is too clever and
# simply doesn't link to the library otherwise.
ldflags = {'debug' : [], 'opt' : [], 'fast' : [], 'prof' : ['-pg'],
           'perf' : ['-Wl,--no-as-needed', '-lprofiler', '-Wl,--as-needed']}

# For Link Time Optimization, the optimisation flags used to compile
# individual files are decoupled from those used at link time
# (i.e. you can compile with -O3 and perform LTO with -O0), so we need
# to also update the linker flags based on the target.
if env['GCC']:
    ccflags['debug'] += ['-ggdb3', '-O0']
    ldflags['debug'] += ['-O0']
    # opt, fast, prof and perf all share the same cc flags, also add
    # the optimization to the ldflags as LTO defers the optimization
    # to link time
    for target in ['opt', 'fast', 'prof', 'perf']:
        ccflags[target] += ['-O3']
        ldflags[target] += ['-O3']
else:
    print 'Unknown compiler, please fix compiler options'
    Exit(1)

# To speed things up, we only instantiate the build environments we
# need.  We try to identify the needed environment for each target; if
# we can't, we fall back on instantiating all the environments just to
# be safe.
target_types = ['debug', 'opt', 'fast', 'prof', 'perf']
obj2target = {'do': 'debug', 'o': 'opt', 'fo': 'fast', 'po': 'prof',
              'gpo' : 'perf'}

def identifyTarget(t):
    ext = t.split('.')[-1]
    if ext in target_types:
        return ext
    if obj2target.has_key(ext):
        return obj2target[ext]
    match = re.search(r'/tests/([^/]+)/', t)
    if match and match.group(1) in target_types:
        return match.group(1)
    return 'all'

needed_envs = [identifyTarget(target) for target in BUILD_TARGETS]
if 'all' in needed_envs:
    needed_envs += target_types

env.Append(CPPDEFINES = ['LINUX', 'POSIX_MEMALIGN', 'MIPS_EMUL']) 

if env['SYSTEM'] == 'SMP':
    env.Append(CPPDEFINES = ['SESC_SMP']) 

if env['NETWORK'] == 'BOOKSIM':
    env.Append(CPPDEFINES = ['SESC_CMP', 'CHECK_STALL']) 

if env['MEMORY'] == 'DRAMSIM2':
    env.Append(CPPDEFINES = ['DRAMSIM2', 'NO_STORAGE', 'LOG_OUTPUT'])

env.Append(CPPPATH=['libsuc'])

# Debug binary
if 'debug' in needed_envs:
    makeEnv('debug', '.do',
            CCFLAGS = Split(ccflags['debug']),
            CPPDEFINES = ['DEBUG', 'DEBUG_SILENT'],
            LINKFLAGS = Split(ldflags['debug']))

# Optimized binary
if 'opt' in needed_envs:
    makeEnv('opt', '.o', strip=True,
            CCFLAGS = Split(ccflags['opt']),
            CPPDEFINES = [],
            LINKFLAGS = Split(ldflags['opt']))

# Profile binary
if 'prof' in needed_envs:
    makeEnv('prof', '.po', 
            CCFLAGS = Split(ccflags['prof']),
            CPPDEFINES = [],
            LINKFLAGS = Split(ldflags['prof']))

Return('envList')
