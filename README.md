## Read-Compile-Run-Loop: tiny and powerful interactive C++ compiler (REPL)

This branch uses cli to give the same flow as cling.

    Currently the compilation time is annoying 

It uses the following special keywords:
- `.q`  exit.
- `.flags ...` set new flags.
- `.f ...` append flags.
- `.clean` clean loaded libs.

## The New Recipe 

- Text input is feed to clang.
- Traverse the parsed code for functions & variables definition "ignore errors".
- Add export prefix for them and put them in plugin.cpp.
- Every non-parsed text would be regarded as once in plugin.cpp.
- Append plugin.hpp with functions prototypes and extern variables.
- Load library with `RTLD_GLOBAL`, so variables can be reused.

## TODO

- [ ] resolve license: GNU General Public License
- [x] use libclang
- [x] replace tiny process with boost process
- [ ] rewrite test cases
- [x] smarter header generation for functions and variables
- [x] *support class,struct,enum,... def.
- [ ] allow redefinition (currently shadow subsequent variables except in the same buffer RTLD_DEEPBIND)
- [ ] test on windows
- [ ] check for errors in compilation 
- [ ] check for errors in compiler command
- [ ] add timeout for compilation
- [ ] fix parser int x = 0!!
- [x] fix set-flag lag
- [x] add an option to add link flags
- [ ] maybe use [zapcc](https://github.com/yrnkrn/zapcc) for better compilation time

## Copyright

Copyright (c) 2018 Viktor Kirilov<br />
Copyright (c) 2020 Islam Omar (io1131@fayoum.edu.eg)
