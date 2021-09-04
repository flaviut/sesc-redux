#This is simply a path where this file and its cohorts is located
MyMakefilePath=$(HOME)/usr/local/include/make
unexport MyMakefilePath

# This Makefile has three modes of operation, but the default is
#   the only one that  should be visible to the user.
#
# Default mode: 
#   First do everything in MakingDIRS mode, then everything in MakingTARGET mode
# MakingDIRS mode:
#   Attempt to build whatever can be built in each sub-directory listed in DIRS
#   If DIRS is not defined, default is all directories that contain Makefiles
# MakingTARGET mode:
#   Attempt to build whatever the target can be build in this directory
#   The type of target is determined by TypeOfTARGET
#   Currently, the default TypeOfTARGET is "None", which is built by doing nothing
#   Other values of TypeOfTARGET cause the corresponding Makefile.$(TypeOfTARGET).mk
#     to be included, which should result in making the correct target

# Default mode is that we are not in a MakingDIRS mode nor in a MakingTARGET mode
ifndef MakingDIRS
  MakingDIRS=0
endif
unexport MakingDIRS
ifndef MakingTARGET
  MakingTARGET=0
endif
unexport MakingTARGET

ifeq ($(MakingDIRS) $(MakingTARGET),0 0)

  # This is the default target if DIRS is non-empty
  # As such, it catches MAKE with a specified target when DIRS is non-empty
  # First, it stores the target in MakeTarget and does MAKE for MakeSubDirs
  # This first step does the MAKE on subdirectories
  # Second, if TARGET is non-empty, it calls MAKE in the current directory
  #  with the same parameters as the current call to MAKE, except that DIRS
  #  is set to an empty string. This prevents that call from building DIRS again
  .DEFAULT:
	@$(MAKE) --no-print-directory MakeTarget=$@ MakingDIRS=1 MakingTARGET=0 MakeSubDirs
	@$(MAKE) --no-print-directory MakingTARGET=1 MakingDIRS=0 $@

  # This is the first non-default target in the file if DIRS is non-empty
  # As such, this catches the target-less MAKE when DIRS is non-empty
  # It proceeds in much the same way as the default target
  .PHONY: MakeTargetLess
  MakeTargetLess:
	@$(MAKE) --no-print-directory MakeTarget= MakingDIRS=1 MakingTARGET=0 MakeSubDirs
	@$(MAKE) --no-print-directory MakingTARGET=1 MakingDIRS=0
else
  ifeq ($(MakingDIRS) $(MakingTARGET),1 1)
    $(error MakingDIRS and MakingTARGET must never both be 1)
  endif
  ifeq ($(MakingDIRS),1)
    # DIRS is the list of subdirectories where MAKE needs to be run
    # If DIRS is not defined, the default is a list containing all immediate
    #   subdirectories that contain a Makefile
    ifndef DIRS
      DIRS=$(strip $(sort $(patsubst %/Makefile,%,$(wildcard */Makefile))))
    endif
    unexport DIRS
    ifneq ($(DIRS),)
      # If DIRS is non-empty, MAKE tries to build MakeSubDirs
      # The original target is in MakeTarget
      # We proceed by invoking MAKE in each of the subdirs with the same parameters
      .PHONY: MakeSubDirs
      MakeSubDirs: $(DIRS)

      .PHONY: $(DIRS)
      $(DIRS):
      ifneq ($(MakeTarget),)
	@echo Making target $(MakeTarget) in directory $@
      else
	@echo Making default target in directory $@
      endif
	-@$(MAKE) MakingDIRS=0 MakingTARGET=0 -C $@ $(MakeTarget)
      ifneq ($(MakeTarget),)
	@echo Done making target $(MakeTarget) in directory $@
      else
	@echo Done making default target in directory $@
      endif
    else
      .PHONY: MakeSubDirs
      MakeSubDirs:
	@echo > /dev/null
    endif
  endif
  ifeq ($(MakingTARGET),1)
    ifndef TypeOfTARGET
      TypeOfTARGET=None
    endif
    unexport TypeOfTARGET
    ifeq ($(TypeOfTARGET),None)
      .DEFAULT:
	@echo > /dev/null

      .PHONY: CatchNoTarget
      CatchNoTarget:
	@echo > /dev/null
    else
      include $(MyMakefilePath)/Makefile.$(TypeOfTARGET).mk
    endif
  endif
endif
