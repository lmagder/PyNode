import sys
import sysconfig

flags = ['-I' + sysconfig.get_path('include'),
         '-I' + sysconfig.get_path('platinclude')
         ]

if sys.platform.startswith('linux'):
    pyver = sysconfig.get_config_var('VERSION')
    abiflags = getattr(sys, 'abiflags', '')
    so_name = 'libpython' + pyver + abiflags + '.so'
    flags.append('-DLINUX_SO_NAME=' + so_name)

print(' '.join(flags))

