libraryopt -- Shared Library Optimization
===================================================================

Introduction
----------------------
Library optimization means to examine all of the native executable
programs in a target file system directory and then rebuild the
shared object libraries of the target file system to include only
those objects need for the executables.
This reduces the shared object library size to a minimum.


Advantages:
-----------------------
The obvious advantage is to reduce the target file system size 
without having to hand-prune.
For example, i was able to save about 250 Kbytes (uncompressed)
for our system (200 Kbytes from libuClibc, our C library; and
50 Kbyes from libm, with a minor saving in libdl and no saving
in libcrypt).


Disadvantages:
-----------------------
-- If you add a dynamic link executable after the fact to your
target, it might not have the library code it needs.
	Workaround: staticly link such executables.
-- With every upgrade of the C library and compiler tools,
it will be necessary to rediscover how the shared libraries are
built and how to rebuild them; then the optinfo/* directories
will need an upgrade.
	Hint: after initial build, look in
	build/gcc-3.4.4-2.16.1/toolchain_build_mips/uClibc/Makerules
	and search for VERBOSE ... modify Makerules to force it to
	be verbose, do a make clean in it's directory, do a build
	again (capturing the output) and search for -EB .
And files may be at different locations etc. ... messy.
-- Add some time to build process.
-- More to fail...


Overview
--------------------------
Library optimization is performed as a part of the optimization
stage of the build process, see $(TOPDIR)/build/Makefile.
The build process prior to the optimization stage builds
$(INSTALL_ROOT) which is what the target file system will look like,
$(INSTALL_ROOT) is typically named with ".build" at end of name.
plus some additional files to be deleted during the optimization stage.
The optimization stage copies and converts $(INSTALL_ROOT) to
another directory tree named $(IMAGE_ROOT) which is closer to
the final target file system appareance; it typically is named
with ".optbuild" at end of name.
Unwanted files are removed in this process, and the library
optimizer is optionally invoked.
Executable and shared object files then get their symbol tables
stripped from within $(IMAGE_ROOT)..
If $(BUILD_LIBRARYOPT) is not set equal to "y", library optimization
is skipped, but other steps of the optimization stage are still done.


Requirements
--------------------------
-- All application libraries (not special libraries, see below) 
    must be installed in two forms somewhere beneath $(INSTALL_ROOT)
    (beneath $(INSTALL_ROOT)/lib preferred but not required):
        name.so -- shared object file
        name.a  -- archive file
-- There must NOT be two name.so files with same name in the tree.
-- Executables, archive and shared object files must NOT
    be stripped prior to running libopt!
-- $(INSTALL_ROOT) layout must be in a disk directory ready 
    to modify (at least it must include all executables plus the
    shared libraries to be modified).
-- Special libraries configuration:
    Special libraries are generally the C library, the
    gcc runtime support library, and a few others.
    Configure (fix after tools upgrade!) for each special library to
    be optimized by creating an appropriate optinfo subdirectory.

Diagnostics
------------------------------
Diagnostics are output to stdout and/or stderr and should be
captured in a log file during build.
The diagnostic:
    UNRESOLVED CODE
is often bogus (the library optimizer should be improved 
to eliminate these) but can indicate a serious problem.

Upgrading Tools
--------------------------
When tools / uClibc are upgraded, you must do the following.
-- In general, you do NOT need to remove or modify any existing
optinfo subdirectories.
-- With every upgrade of the C library and compiler tools,
it will be necessary to rediscover how the shared libraries are
built and how to rebuild them; then the optinfo/* directories
will need an upgrade.
	Hint: after initial build, look in
	build/gcc-3.4.4-2.16.1/toolchain_build_mips/uClibc/Makerules
	and search for VERBOSE ... modify Makerules to force it to
	be verbose, do a make clean in it's directory, do a build
	again (capturing the output) and search for -EB .
And files may be at different locations etc. ... messy.


History and Design
-------------------------
The library optimization relies on libraryopt-1.0.1 from Monta Vista
Software by way of sourceforge.
I made one small but vital bug fix in the libindex script, plus
added some print's for easier debugging; and added error checking.

The file libopttemp.mak is intended to be included into build/Makefile .
[Update: contents have been copied into build/Makefile now...
this file is for historical reference only now].
It's job is to set up a temporary directory to be used by the libopt
tool (which is finnicky about where it finds it's files) and
for it's own use.
This directory holds copies or symlinks to all of the development
tools for the target (gcc etc.), as well as the target libraries
that go with the development tools (C library etc.).
It also holds copies of per-library scripts copied from the optinfo
subdirectory.
At this stage we do NOT examine the target file system directory.

After the make instructions in libopttemp.mak have done their job,
instructions in build/Makefile run libopt to do the actual optimization.
At this point we need the actual file system directory.
For debugging purposes, it is helpful to make a copy and give
the copy to libopt.
As we've set it up, libopt will modify the copy in place.
Finally, we strip the executables (and any libraries that escaped
being stripped earlier).


Debugging Library Optimization
-----------------------------------------
Modify the optinfo/*/build scripts to do the following:
-- Rename the library file to .bak extension before recreating it.
-- Also create a shared library which leaves nothing out 
    (see commented out examples) with extension e.g. ".ref".
    (To ensure the same order, use the original .a file that was
    used in building the original .so file).
-- The target file system directory will now contain .bak and .ref files.
    These should be basically identical, depending on how they were made.
    You can do "....nm -D" or "...objdump -T" on them to examine the
    differences if any.


Author
-----------------------------
Ted Merrill, Sept. 2007
libraryopt-1.0.1 is from Monta Vista Software

