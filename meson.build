project('toolfox', 'c')

glib_dep = dependency('glib-2.0')
gio_dep = dependency('gio-2.0')
executable('toolfox', 'src/toolfox.c', dependencies : [glib_dep, gio_dep], install : true)
executable('toolfox-redirect-internal-helper', 'src/toolfox-redirect-internal-helper.c',
           dependencies : [glib_dep], install : true)
