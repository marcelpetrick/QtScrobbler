fixed: 0. bring it down to zero build warnings. check this via the localPipeline as well. a single warning shall make the build fail. add this as flag.
fixed: 1. handle the remaining steps from the qt6 porting. like signal and slot connections.
fixed: 7. bump to cpp20; check for outdated constructs, which can be improved too. whole source code base.
2. add clang tidy?
3. add clazy?
4. add doxygen - as check for undocumented public API. also make it generate the docs.
5. cppcheck?
6. automatic testing (right now zeri unit testing, therefore coverage is zero) - is there even a reason to make it work?
8. check all dependencies:; cmake 3.16 - a bit outdated, or? check what is the most recent stable version for all dependencies. then raise them. we want to use modern stuff. does not have o be bbleeding edge, but should be viable and modern. no old crap.

