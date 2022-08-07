{
  "targets": [
    {
      "target_name": "PyNode",
      "sources": [
        "src/main.cpp",
        "src/helpers.cpp",
        "src/pynode.cpp",
        "src/worker.cpp",
        "src/pywrapper.cpp",
        "src/jswrapper.cpp"
      ],
      'include_dirs': [
        "<!@(node -p \"require('node-addon-api').include\")"
      ],
      'dependencies': ["<!(node -p \"require('node-addon-api').gyp\")"],
      'cflags!': [ '-fno-exceptions' ],
      'cflags_cc!': [ '-fno-exceptions' ],
      'cflags+': [ '-g' ],
      'cflags_cc+': [ '-g' ],
      'xcode_settings': {
        'GCC_ENABLE_CPP_EXCEPTIONS': 'YES',
        'CLANG_CXX_LIBRARY': 'libc++',
        "CLANG_CXX_LANGUAGE_STANDARD":"c++20",
        'MACOSX_DEPLOYMENT_TARGET': '10.7',
      },
      'msbuild_settings': {
        'ClCompile': { 'LanguageStandard': "stdcpp20", },
      },
      "conditions": [
        ['OS=="mac"', {
          'cflags+': ['-fvisibility=hidden'],
          'xcode_settings': {
            'GCC_SYMBOLS_PRIVATE_EXTERN': 'YES', # -fvisibility=hidden
          }
        }],
        ['OS=="win"', {
          "variables": {
            "PY_HOME%": "<!(IF NOT DEFINED PY_HOME (\"%PYTHON%\" -c \"import sysconfig;print(sysconfig.get_paths()['data'])\") ELSE (echo %PY_HOME%))"
          },
          "include_dirs": [
            "<!(echo <(PY_HOME)\\include)"
          ],
          "msbuild_settings": {
            "Link": {
              "AdditionalLibraryDirectories": "<!(echo <(PY_HOME)\\libs)"
            },
          }
        }],
        ['OS!="win"', {
          'cflags+': ['-Wno-missing-field-initializers', "-std=c++20" ],
          "variables": {
            "PY_INCLUDE%": "<!(if [ -z \"$PY_INCLUDE\" ]; then echo $(\"$PYTHON\" build_include.py); else echo $PY_INCLUDE; fi)",
            "PY_LIBS%": "<!(if [ -z \"$PY_LIBS\" ]; then echo $(\"$PYTHON\" build_ldflags.py); else echo $PY_LIBS; fi)"
          },
          "include_dirs": [
            "<(PY_INCLUDE)"
          ],
          "libraries": [
            "<(PY_LIBS)",
          ]
        }]
      ]
    }
  ]
}
