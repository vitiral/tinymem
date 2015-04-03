import pymakec as mk


directories = ['src', 'platform']
sources = mk.sourcefiles(directories)
directories.append('tests')
tests = mk.sourcefiles('tests')

if __name__ == '__main__':
    mk.compile(directories, sources, tests,
               clean=mk.cleanfiles(directories))
    mk.runtests('tests')
