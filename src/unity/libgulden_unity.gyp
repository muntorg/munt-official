{
    "targets": [
        {
            "target_name": "libgulden_unity_objc",
            "type": 'static_library',
            "dependencies": [
              "../../djinni/djinni/support-lib/support_lib.gyp:djinni_objc",
            ],
            "sources": [
              "<!@(python glob.py djinni/objc  '*.cpp' '*.mm' '*.m' '*.h' '*.hpp')",
              "<!@(python glob.py djinni/cpp   '*.cpp' '*.hpp')",
              "appmanager.h",
              "libinit.h",
              "signals.h",
              "unity_impl.h",
              "appmanager.cpp",
              "libinit.cpp",
              "unity_impl.cpp",
            ],
            "include_dirs": [
              "djinni/objc",
            ],
        },
    ],
}
