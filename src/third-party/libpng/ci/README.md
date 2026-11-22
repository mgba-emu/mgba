Scripts for the Continuous Integration of the PNG Reference Library
===================================================================

Copyright Notice
----------------

Copyright (C) 2019 Cosmin Truta.

Use, modification and distribution are subject to the MIT License.
Please see the accompanying file `LICENSES/MIT.txt` or visit
<https://opensource.org/license/mit>

File List
---------

    README.md               ==>  This file
    ci_lint.sh              ==>  Lint the source code
    ci_verify_cmake.sh      ==>  Verify the build driven by CMakeLists.txt
    ci_verify_configure.sh  ==>  Verify the build driven by configure
    ci_verify_makefiles.sh  ==>  Verify the build driven by scripts/makefile.*
    ci_verify_version.sh    ==>  Verify the consistency of version definitions
    lib/ci.lib.sh           ==>  Shell library for the main ci_*.sh scripts
    libexec/ci_*.sh         ==>  Shell utilities for the main ci_*.sh scripts
    targets/*/ci_env.*.sh   ==>  Shell environments for cross-platform testing
