paths_test = executable(
    'paths_test',
    [
        'paths_test.c',
        '../src/paths.c',
    ],
    include_directories: include_directories('../src'),
    install: false,
)

test('paths', paths_test)
