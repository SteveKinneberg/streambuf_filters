import SCons

from SCons.Builder import Builder
from SCons.Script import Dir, Flatten, Mkdir

def genhtml_generator(source, target, env, for_signature):
    cmd = [env['GENHTML']]

    if 'GENHTMLOPTS' in env:
        cmd += env['GENHTMLOPTS']

    cmd += [str(source[0])]
    cmd += ['--output-directory', str(target[0])]

    return ' '.join(Flatten(cmd))


def genhtml_message(s, target, source, env):
    if 'GENHTMLCOMSTR' in env:
        print(env.subst(env['GENHTMLCOMSTR'], 1, target, source))
    else:
        print(s)


def generate(env):
    env['GENHTML'] = 'genhtml'
    env['BUILDERS']['GenHtml'] = Builder(generator=genhtml_generator, PRINT_CMD_LINE_FUNC=genhtml_message)


def exists(env):
    return env.Detect('genhtml')
