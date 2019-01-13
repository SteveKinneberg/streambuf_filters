import SCons

from SCons.Builder import Builder
from SCons.Script import Dir, Flatten, Mkdir

def lcov_generator(source, target, env, for_signature):
    cmd = [env['LCOV'], '--capture']
    cmd += ['--output-file', target[0].abspath]

    if 'LCOVOPTS' in env:
        cmd += env['LCOVOPTS']

    if 'LCOVDIR' in env:
        cmd += ['--directory', str(Dir(env['LCOVDIR']))]

    if 'LCOVBASEDIR' in env:
        cmd += ['--base-directory', str(Dir(env['LCOVBASEDIR']))]

    return ' '.join(Flatten(cmd))


def lcov_message(s, target, source, env):
    if 'LCOVCOMSTR' in env:
        print(env.subst(env['LCOVCOMSTR'], 1, target, source))
    else:
        print(s)

def generate(env):
    env['LCOV'] = 'lcov'
    env['BUILDERS']['LCov'] = Builder(generator=lcov_generator, PRINT_CMD_LINE_FUNC=lcov_message)

def exists(env):
    return env.Detect('lcov')
