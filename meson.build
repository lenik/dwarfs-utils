project(
    'dwarfs-utils',
    'c',
    version: run_command(
        'sh',
        '-c', '''
        v=$(
            command -v zfr >/dev/null 2>&1 && zfr version 2>/dev/null || true
        )
        if [ -z "$v" ] && [ -f VERSION ]; then
            v=$(head -n1 VERSION | tr -d '\r')
            v=${v#v}
        fi
        if [ -z "$v" ]; then
            v="0.0.0" # FIXED TO 0.0.0, DO NOT MODIFY
        fi
        printf '%s' "$v"
        ''',
        check: false,
    ).stdout().strip(),
    license: 'AGPL-3.0-or-later',
    default_options: [
        'c_std=c99',
        'warning_level=2',
    ],
)

project_author = 'Lenik'
project_email = 'dwarfs-utils@bodz.net'
project_year = 2026

asciidoctor = find_program('asciidoctor', required: true)
fs = import('fs')

prefix = get_option('prefix')
bindir = prefix / get_option('bindir')
datadir = prefix / get_option('datadir')
libdir = prefix / get_option('libdir')
mandir = prefix / get_option('mandir')
localedir = prefix / get_option('localedir')
pkgdocdir = datadir / 'doc' / meson.project_name()

config_h = configuration_data()
config_h.set_quoted('PROJECT_VERSION', meson.project_version())
config_h.set_quoted('PROJECT_AUTHOR', project_author)
config_h.set_quoted('PROJECT_EMAIL', project_email)
config_h.set('PROJECT_YEAR', project_year)
config_h.set_quoted('LOCALEDIR', localedir)
config_h.set_quoted('TEXT_DOMAIN', meson.project_name())

configure_file(
    output: 'config.h',
    configuration: config_h,
)

bas_c_dep = dependency('bas-c', required: true)
bas_args = [
    '-DLOGGER_NAME=dwarfs_fm',
    '-DGETTEXT_PACKAGE="' + meson.project_name() + '"',
]

inc = include_directories('src', '.')

common_sources = [
    'src/paths.c',
]

executable(
    'dwarfs-fm',
    ['src/dwarfs-fm.c'] + common_sources,
    include_directories: inc,
    dependencies: [bas_c_dep],
    c_args: bas_args,
    install: true,
    install_dir: bindir,
)

caja_dep = dependency('libcaja-extension', required: false)
if caja_dep.found()
    caja_ext_dir = caja_dep.get_variable(pkgconfig: 'extensiondir')
    shared_module(
        'caja-dwarfs',
        'src/plugins/caja/caja-dwarfs.c',
        include_directories: inc,
        dependencies: [caja_dep],
        c_args: bas_args,
        name_prefix: '',
        install: true,
        install_dir: caja_ext_dir,
    )
endif

nemo_dep = dependency('libnemo-extension', required: false)
if nemo_dep.found()
    nemo_ext_dir = nemo_dep.get_variable(pkgconfig: 'extensiondir')
    shared_module(
        'nemo-dwarfs',
        'src/plugins/nemo/nemo-dwarfs.c',
        include_directories: inc,
        dependencies: [nemo_dep],
        c_args: bas_args,
        name_prefix: '',
        install: true,
        install_dir: nemo_ext_dir,
    )
endif

install_data(
    'data/mime/dwarfs-utils.xml',
    install_dir: datadir / 'mime' / 'packages',
)

install_data(
    [
        'data/applications/dwarfs-viewer.desktop',
        'data/applications/dwarfs-archiver.desktop',
    ],
    install_dir: datadir / 'applications',
)

bash_files = [
    'dwarfs-fm.bash',
]

foreach file : bash_files
    name = fs.stem(file)
    install_data(
        file,
        install_dir: datadir / 'bash-completion' / 'completions',
        rename: name,
    )
endforeach

install_data(
    [
        'LICENSE',
        'README.md',
        'README-zh_CN.md',
    ],
    install_dir: pkgdocdir,
)

subdir('tests')

run_target(
    'look',
    command: [
        find_program('zfr'),
        'build',
        '--look',
        '-C', meson.project_source_root(),
        '-b', meson.project_build_root(),
    ],
)

run_target(
    'install-symlinks',
    command: [
        find_program('bash'),
        meson.project_source_root() / 'scripts' / 'install-symlinks.sh',
        get_option('prefix'),
        bindir,
        datadir,
        mandir,
        meson.project_build_root(),
        meson.project_source_root(),
    ],
)

run_target(
    'uninstall-symlinks',
    command: [
        find_program('bash'),
        meson.project_source_root() / 'scripts' / 'uninstall-symlinks.sh',
        get_option('prefix'),
        bindir,
        datadir,
        mandir,
    ],
)

puffs = [
    'dwarfs-fm',
]
foreach puff : puffs
  custom_target(
    puff + '-man',
    input: 'man' / (puff + '.adoc'),
    output: puff + '.1',
    command: [
      asciidoctor,
      '-b', 'manpage',
      '-a', 'project-version=' + meson.project_version(),
      '-a', 'project-year=@0@'.format(project_year),
      '-a', 'project-author=' + project_author,
      '-a', 'project-email=' + project_email,
      '-o', '@OUTPUT@',
      '@INPUT@',
    ],
    build_by_default: true,
    install: true,
    install_dir: mandir / 'man1',
  )
endforeach
