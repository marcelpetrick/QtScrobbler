0. bring it down to zero build warnings. check this via the localPipeline as well. a single warning shall make the build fail. add this as flag.
1. handle the remaining steps from the qt6 porting. like signal and slot connections.
2. add clang tidy?
3. add clazy?
4. add doxygen - as check for undocumented public API. also make it generate the docs.
5. cppcheck?
6. automatic testing (right now zeri unit testing, therefore coverage is zero) - is there even a reason to make it work?


