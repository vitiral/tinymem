import pymakec as mk


directories = ['src', 'platform']
sourcefiles = mk.sourcefiles(directories)
directories.append('tests')
testfiles = mk.sourcefiles(directories)

if __name__ == '__main__':
    mk.compile(sourcefiles, testfiles, cleanfiles=cleanfiles(directories))
    runtests('tests')
