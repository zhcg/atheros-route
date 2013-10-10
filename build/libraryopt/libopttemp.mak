
#
# libopttemp.mak -- WAS included into $(TOPDIR)/build/Makefile .
# --> has been copied into build/Makefile, this is for historical
#  reference only now! <---------------------------
#
#
# Makes a temporary directory $(TOPDIR)/libopt.temp
# that the library optimizer needs in order to do it's work.
#
# The library optimizer (libopt) is a program (python script) that
# creates shared object libraries that contain only the objects actually
# needed for a set of target executables.
# We are using libraryopt-1.0.1 from http://libraryopt.sourceforge.net/
# which was released from Monta Vista Software.
# The script requires a particular layout of tools, configuration files,
# and object files (which is what Monta Vista uses).
# Rather than modify the script, we accomodate it and produce a
# temporary directory $(TOPDIR)/libopt.temp that contains the needed
# files in the required tree.
#
# Prerequisites: the environmental variables that are checked or created
# from the main build/Makefile are used here (without checking).
# For each shared library to be worked on, there must be
# $(TOPDIR)/build/libraryopt/optinfo/<libraryname> subdirectory;
# see $(TOPDIR)/build/libraryopt/optinfo/README.txt for details.
#



LIBOPTINFOSRC=$(TOPDIR)/build/libraryopt/optinfo
LIBOPTSCRIPTSRC=$(TOPDIR)/build/libraryopt/libraryopt-1.0.1

# defined in build/Makefile: LIBOPTTEMP=$(TOPDIR)/build/libopt.temp
# libopt requires target/usr/lib/optinfo below LIBOPTTEMP
LIBOPTTEMPINFO=$(LIBOPTTEMP)/target/usr/lib/optinfo
LIBOPTTOOLSRC=$(TOPDIR)/build/$(TOOLCHAIN)/$(TOOLARCH)
LIBOPTTOOLSRC2=$(TOPDIR)/build/$(TOOLCHAIN)/toolchain_$(TOOLARCH)/uClibc

libopt_temp : # depends on tools all done!
	rm -rf $(LIBOPTTEMP)
	mkdir -p $(LIBOPTTEMP)
	# libopt expects tools in bin, all with same prefix,
	# including libopt and libindex scripts themselves.
	mkdir -p $(LIBOPTTEMP)/bin
	ln -s $(LIBOPTTOOLSRC)/bin/* $(LIBOPTTEMP)/bin/.
	ln -s $(LIBOPTSCRIPTSRC)/src/libopt $(LIBOPTTEMP)/bin/$(TOOLPREFIX)libopt
	ln -s $(LIBOPTSCRIPTSRC)/src/libindex $(LIBOPTTEMP)/bin/$(TOOLPREFIX)libindex
	# and for our own convenience we build a lib directory with all
	# of the various files we will need
	mkdir -p $(LIBOPTTEMP)/lib
	ln -s $(LIBOPTTOOLSRC)/lib/* $(LIBOPTTEMP)/lib/.
	# libgcc.a is hard to find. E.g. it can be found at:
	#  build/gcc-3.4.4-2.16.1/build_mips/lib/gcc/mips-linux-uclibc/3.4.4/libgcc.a
	ln -s $(LIBOPTTOOLSRC)/lib/gcc/*/*/libgcc.a $(LIBOPTTEMP)/lib/.
	ln -s $(LIBOPTTOOLSRC2)/*/*_so.a $(LIBOPTTEMP)/lib/.
	ln -s $(LIBOPTTOOLSRC2)/*/*/*_so.a $(LIBOPTTEMP)/lib/.
	ln -s $(LIBOPTTOOLSRC2)/lib/interp.os $(LIBOPTTEMP)/lib/.
	mkdir -p $(LIBOPTTEMPINFO)
	set -e ; \
        app_sofiles=`find $(INSTALL_ROOT) -name '*.so' -print` ; \
	for app_sofile in $$app_sofiles ; do \
          sobject=`basename $$app_sofile` ; \
          soname=`basename $$sobject .so` ; \
          aobject=`dirname $$app_sofile`/$$soname.a ; \
	  if [ -L $$app_sofile ] ; then true; else \
	    echo Looking at $$app_sofile ... ; \
	    if [ -d $(LIBOPTINFOSRC)/$$sobject ] ; then \
	      echo Creating libopt temp info for special shared object file $$sobject ; \
	      cp -a $(LIBOPTINFOSRC)/$$sobject $(LIBOPTTEMPINFO)/. ; \
	      cp -a $$app_sofile $(LIBOPTTEMPINFO)/$$sobject/.;  \
              ln -s $(LIBOPTTEMP)/lib $(LIBOPTTEMPINFO)/$$sobject/required; \
	      (cd $(LIBOPTTEMPINFO)/$$sobject && ./prebuild $(LIBOPTTEMP)/bin/$(TOOLPREFIX)libindex $(LIBOPTTOOLSRC)/lib) ; \
	    elif [ -f $$aobject ] ; then \
	      echo Creating libopt temp info for application shared object file $$sobject ; \
	      cp -a $(LIBOPTINFOSRC)/generic $(LIBOPTTEMPINFO)/$$sobject ; \
	      cp -a $$app_sofile $(LIBOPTTEMPINFO)/$$sobject/.;  \
              ln -s $(LIBOPTTEMP)/lib $(LIBOPTTEMPINFO)/$$sobject/required; \
              mkdir $(LIBOPTTEMPINFO)/$$sobject/apps; \
              for other_so_file in $$app_sofiles ; do \
                other_so=`basename $$other_so_file`  ;   \
                if [ $$other_so != $$sobject ] ; then \
                  ln -s $$other_so_file $(LIBOPTTEMPINFO)/$$sobject/apps/. ; \
                fi ; \
              done ; \
              ln -s $$aobject $(LIBOPTTEMPINFO)/$$sobject/. ; \
	      (cd $(LIBOPTTEMPINFO)/$$sobject && ./prebuild $(LIBOPTTEMP)/bin/$(TOOLPREFIX)libindex $$sobject) ; \
	      (cd $(LIBOPTTEMPINFO)/$$sobject && $(LIBOPTTEMP)/bin/$(TOOLPREFIX)objdump -p $$sobject | awk '/^ *NEEDED/{print $$2}' >needed ) ; \
            else echo Skipping $$sobject ; \
	    fi; \
	  fi; \
        done
	


