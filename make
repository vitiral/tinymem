#!/usr/bin/python
import os
import glob
import subprocess
pjoin = os.path.join

CFLAGS = "-g -O2 -Wall -Wextra -Isrc -rdynamic -DNDEBUG "
cc = 'cc '


def get_sources(path):
    paths = glob.glob(pjoin(path, '*.c'))
    paths.extend(glob.glob(pjoin(path, '*.h')))
    return paths


sources = get_sources('src')
tests = get_sources('tests')
tfiles = [t[:-2] for t in tests if t.endswith('.c')]


if __name__ == '__main__':
    str_sources = ' '.join(sources + tests)
    str_outputs = ' '.join(tfiles)
    call = "{cc} {cflags} {sources} -o {outputs}".format(
        cc=cc, cflags=CFLAGS, sources=str_sources, outputs=str_outputs)
    subprocess.check_call("make clean", shell=True)
    subprocess.check_call(call, shell=True)
    subprocess.check_call("sh tests/runtests.sh", shell=True)
