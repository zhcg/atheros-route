generic build and prebuild -- 
use for "application" shared object libraries that:
 -- are installed in target /lib directory.
 -- have a corresponding .a file also installed in target /lib
            (but removed in the optimization process).
 -- are named with simple ".so" extension
 -- the .a file is named same except with ".a" instead of ".so" at end.
 -- have no special build needs.
 
The .so files may depend upon other application .so files.

