import os

#######################################################
# Optimize SCons startup
#######################################################
# Suppress default tool detection in the default environment
DefaultEnvironment(tools = [])
# Suppress searching for code in version control systems
SourceCode('.', None)

#######################################################
# Custom scons command line options
#######################################################
AddOption('--no-tests', dest = 'run-tests', action = 'store_false', default = True,
          help = "don't run automated tests")

#######################################################
# Custom configure tests
#######################################################
custom_tests = SConscript('SConsFiles/SConscript.custom_tests')
Export('custom_tests')

#######################################################
# Setup the base project environment
#######################################################
project_env = SConscript('SConsFiles/SConscript.project_env')
Export('project_env')

#######################################################
# Create host build environment
#######################################################
host_env = SConscript('SConsFiles/SConscript.host_env')
Export('host_env')

#######################################################
# Create target build environment
#######################################################
target_env = SConscript('SConsFiles/SConscript.target_env')
Export('target_env')

#######################################################
# Create target test build environment
#######################################################
target_test_env = SConscript('SConsFiles/SConscript.test_env', exports = {'base_env': target_env})
Export('target_test_env')

#######################################################
# Extra clean targets
#######################################################

#######################################################
# Build documentation
#######################################################
if os.path.exists('Doxyfile'):
    config = Configure(project_env)
    have_doxygen = config.CheckProg(project_env['DOXYGEN'])
    project_env = config.Finish()

    if have_doxygen:
        docs = project_env.Doxygen('Doxyfile')
        # Remove docs from default build
        project_env.Ignore(project_env['install_basedir'],
                           project_env['install_docdir'])
        project_env.Alias('docs', docs)
        project_env.Alias('all', 'docs')
        project_env['sysinstall_doc'] = project_env['install_docdir'];
    else:
        print('*** WARNING: Documentation not generated: Doxygen not found')

#######################################################
# Build the various parts
#######################################################
# Need to process each SConscript manually to setup the build directory paths
for sf in Glob('*/SConscript'):
    SConscript(sf, variant_dir = target_env['build_dir'] + '/%s' % sf.get_dir())

#######################################################
# Run automated tests
#######################################################
if GetOption('run-tests'):
    for prog in target_test_env['automated_tests']:
        test_run = target_test_env.RunTest(prog)
        if target_test_env['COVERAGE']:
            target_test_env.CoverageReport(prog, test_run)


#######################################################
# Install target files on to the system
#######################################################
sys_files = []
if project_env.has_key('sysinstall_bin') and project_env['sysinstall_bin']:
    sys_files.append(project_env.Install('$INSTALL_BIN_PREFIX', project_env['sysinstall_bin']))

if project_env.has_key('sysinstall_lib') and project_env['sysinstall_lib']:
    sys_files.append(project_env.Install('$INSTALL_LIB_PREFIX', project_env['sysinstall_lib']))

if project_env.has_key('sysinstall_inc') and project_env['sysinstall_inc']:
    sys_files.append(project_env.Install('$INSTALL_INCLUDE_PREFIX', project_env['sysinstall_inc']))

if project_env.has_key('sysinstall_doc') and project_env['sysinstall_doc']:
    sys_files.append(project_env.Install('$INSTALL_DOC_PREFIX', project_env['sysinstall_doc']))

project_env.Alias('install', sys_files)

#######################################################
# Check required build dependencies
#######################################################
if not all([ v for v in host_env['required'].values() ]):
    print('*** Missing required host build dependencies:')
    print('\n'.join([ '\t%s' % k for k,v in target_env['required'].items() if not v]))
    Exit(1)

if not all([ v for v in target_env['required'].values() ]):
    print('*** Missing required target build dependencies:')
    print('\n'.join([ '\t%s' % k for k,v in target_env['required'].items() if not v]))
    Exit(1)
