# This is where MINT, SESC and other directories are
ANLMACROS   := $(HOME)/sesc/apps/scripts/pthread.m4.stougie
MIPSCCPFX   := /mipsroot/cross-tools/bin/mips-unknown-linux-gnu-
X86CCDIR    := /usr

ifndef TARGET
TARGET= $(notdir $(shell 'pwd'))
endif

.PHONY: mipseb

mipseb: $(TARGET).mipseb

APPOBJDIR= .AppObject
APPPRFDIR= .AppProfile
APPOUTDIR= .AppOutput

SYSTEM	:= $(shell uname)

ifeq ($(SYSTEM),Linux)
  ExeTypes := mipseb mipsel mipseb64 mipsel64 mipseb-nopad x86
  ifdef ExeType
    DefaultTargets := $(patsubst %,$(TARGET).%,$(ExeType))
  else
    DefaultTargets := $(patsubst %,$(TARGET).%,$(ExeTypes))
  endif
  ifneq ($(filter $(ExeType),$(ExeTypes)),)
    # Macro processing
    M4FLAGS += $(M4FLAGS.XCC) $(ANLMACROS)
    # Optimization
#    GFLAGS += -g -O0
    GFLAGS  += -O3 -g

    #  GFLAGS  += -fno-delayed-branch -fno-omit-frame-pointer 

    # ISA and ABI
    GFLAGS  += -static
    ifneq ($(filter $(ExeType),mipseb mipsel mipseb64 mipsel64 mipseb-nopad),)
      # GFLAGS  += -mno-abicalls
      # GFLAGS  += -mno-abicalls
      # GFLAGS  += -fPIC
      # GFLAGS  += -fno-PIC
      GFLAGS  += -fno-delayed-branch -fno-optimize-sibling-calls
    endif
    ifneq ($(filter $(ExeType),mipseb mipseb-nopad mipsel mipseb-nopad),)
      GFLAGS  += -msplit-addresses
      GFLAGS  += -mabi=32  -march=mips4
    endif
    ifneq ($(filter $(ExeType),mipseb64 mipsel64),)
      GFLAGS  += -mabi=64  -march=mips4
    endif
    ifneq ($(filter $(ExeType),x86),)
      GFLAGS += -m32 -march=i386 -mtune=generic
    endif
    ifneq ($(filter $(ExeType),mipseb-nopad),)
      GFLAGS += -DNO_PADDING
    endif	
    GCFLAGS  += $(GFLAGS)
    LFLAGS   += $(GFLAGS) $(LFLAGS.Linux) -lpthread
    GCFLAGS  += -I$(APPOBJDIR)/$(ExeType)/
    CFLAGS   += $(GCFLAGS) $(CFLAGS.Linux)
    CPPFLAGS += $(GCFLAGS)
    FFLAGS   += $(GFLAGS)
    F90FLAGS += $(GFLAGS)
    M4   := m4
    ifneq ($(filter $(ExeType),mipseb mipseb64 mipseb-nopad),)
      CC   := $(MIPSCCPFX)gcc
      CPPC := $(MIPSCCPFX)g++
      FC   := $(MIPSCCPFX)gfortran
      F90C := $(MIPSCCPFX)gfortran
    endif
    ifneq ($(filter $(ExeType),mipsel mipsel64),)
      CC   := $(MIPSELCCPFX)gcc
      CPPC := $(MIPSELCCPFX)g++
      FC   := $(MIPSELCCPFX)gfortran
      F90C := $(MIPSELCCPFX)gfortran
    endif
    ifneq ($(filter $(ExeType),x86),)
      CC   := $(X86CCDIR)/bin/gcc
      CPPC := $(X86CCDIR)/bin/g++
      FC   := $(X86CCDIR)/bin/gfortran
      F90C := $(X86CCDIR)/bin/gfortran44
    endif
  endif
endif

ifneq ($(ExeType),)
  ifeq ($(filter $(ExeType),$(ExeTypes)),)
    $(error Can not make an executable of type $(ExeType) on a platform of type $(SYSTEM))
  endif
endif

CSRC_M    := $(wildcard Changed/*.c)
ANLCSRC_M := $(wildcard Changed/*.C)
CSRC_S    := $(filter-out $(patsubst Changed/%.c,Source/%.c,$(CSRC_M)),$(wildcard Source/*.c))
ANLCSRC_S := $(filter-out $(patsubst Changed/%.C,Source/%.C,$(ANLCSRC_M)),$(wildcard Source/*.C))
ifdef NOTCSRC
  CSRC_M := $(filter-out $(patsubst %,Changed/%,$(NOTCSRC)),$(CSRC_M))
  CSRC_S := $(filter-out $(patsubst %,Source/%,$(NOTCSRC)),$(CSRC_S))
endif
ifdef CSRC
  CSRC_M := $(CSRC_M) $(filter $(wildcard Changed/*),$(patsubst %,Changed/%,$(CSRC)))
  CSRC_S := $(CSRC_S) $(filter-out $(patsubst Changed/%,Source/%,$(CSRC_M)),\
            $(filter $(wildcard Source/*),$(patsubst %,Source/%,$(CSRC))))
endif

CPPSRC_M := $(wildcard Changed/*.cc)
CPPSRC_S := $(filter-out $(patsubst Changed/%.cc,Source/%.cc,$(CPPSRC_M)),$(wildcard Source/*.cc))
ifdef NOTCPPSRC
  CPPSRC_M := $(filter-out $(patsubst %,Changed/%,$(NOTCPPSRC)),$(CPPSRC_M))
  CPPSRC_S := $(filter-out $(patsubst %,Source/%,$(NOTCPPSRC)),$(CPPSRC_S))
endif
ifdef CPPSRC
  CPPSRC_M := $(CPPSRC_M) $(filter $(wildcard Changed/*),$(patsubst %,Changed/%,$(CPPSRC)))
  CPPSRC_S := $(CPPSRC_S) $(filter-out $(patsubst Changed/%,Source/%,$(CPPSRC_M)),\
                                       $(filter $(wildcard Source/*),$(patsubst %,Source/%,$(CPPSRC))))
endif

CHDR_M    := $(wildcard Changed/*.h)
ANLCHDR_M := $(wildcard Changed/*.H)
CHDR_S    := $(filter-out $(patsubst Changed/%.h,Source/%.h,$(CHDR_M)),$(wildcard Source/*.h))
ANLCHDR_S := $(filter-out $(patsubst Changed/%.H,Source/%.H,$(ANLCHDR_M)),$(wildcard Source/*.H))
ifdef NOTCHDR
  CHDR_M := $(filter-out $(patsubst %,Changed/%,$(NOTCHDR)),$(CHDR_M))
  CHDR_S := $(filter-out $(patsubst %,Source/%,$(NOTCHDR)),$(CHDR_S))
endif
ifdef CHDR
  CHDR_M := $(CHDR_M) $(filter $(wildcard Changed/*),$(patsubst %,Changed/%,$(CHDR)))
  CHDR_S := $(CHDR_S) $(filter-out $(patsubst Changed/%,Source/%,$(CHDR_M)),\
                                       $(filter $(wildcard Source/*),$(patsubst %,Source/%,$(CHDR))))
endif

FSRC_M := $(wildcard Changed/*.f Changed/*.F)
FSRC_S := $(filter-out $(patsubst Changed/%,Source/%,$(FSRC_M)),$(wildcard Source/*.f Source/*.F))
ifdef NOTFSRC
  FSRC_M := $(filter-out $(patsubst %,Changed/%,$(NOTFSRC)),$(FSRC_M))
  FSRC_S := $(filter-out $(patsubst %,Source/%,$(NOTFSRC)),$(FSRC_S))
endif
ifdef FSRC
  FSRC_M := $(FSRC_M) $(filter $(wildcard Changed/*),$(patsubst %,Changed/%,$(FSRC)))
  FSRC_S := $(FSRC_S) $(filter-out $(patsubst Changed/%,Source/%,$(FSRC_M)),\
            $(filter $(wildcard Source/*),$(patsubst %,Source/%,$(FSRC))))
endif

F90SRC_M := $(wildcard Changed/*.f90 Changed/*.F90)
F90SRC_S := $(filter-out \
              $(patsubst Changed/%,Source/%,$(F90SRC_M)),\
              $(wildcard Source/*.f90 Source/*.F90)\
            )
ifdef NOTF90SRC
  F90SRC_M := $(filter-out $(patsubst %,Changed/%,$(NOTF90SRC)),$(F90SRC_M))
  F90SRC_S := $(filter-out $(patsubst %,Source/%,$(NOTF90SRC)),$(F90SRC_S))
endif
ifdef F90SRC
  F90SRC_M := $(F90SRC_M) $(filter $(wildcard Changed/*),$(patsubst %,Changed/%,$(F90SRC)))
  F90SRC_S := $(F90SRC_S) $(filter-out $(patsubst Changed/%,Source/%,$(F90SRC_M)),\
            $(filter $(wildcard Source/*),$(patsubst %,Source/%,$(F90SRC))))
endif


OTHER_M := $(filter-out \
             $(CSRC_M) $(ANLCSRC_M) $(CPPSRC_M) $(CHDR_M) $(ANLCHDR_M) $(FSRC_M) $(F90SRC_M),\
             $(wildcard Changed/*)\
           )
OTHER_S := $(filter-out \
             $(CSRC_S) $(ANLCSRC_S) $(CPPSRC_S) $(CHDR_S) $(ANLCHDR_S) $(FSRC_S) $(F90SRC_S),\
             $(wildcard Source/*)\
           )
OTHER_S := $(filter-out \
             $(patsubst Changed/%,Source/%,\
                $(CSRC_M) $(ANLCSRC_M) $(CPPSRC_M) $(CHDR_M) $(ANLCHDR_M) $(FSRC_M) $(F90SRC_M) $(OTHER_M)\
             ),\
             $(OTHER_S)\
           )

ifneq ($(strip $(DefaultTargets)),)
  .PHONY: DefaultTargets
  DefaultTargets: $(DefaultTargets)

  $(patsubst %,$(TARGET).%,$(ExeTypes)) : $(TARGET).% : $(APPOBJDIR)/%/$(TARGET)
	cp $^ $@
endif

ifndef ExeType
  PossibleExes := $(patsubst %,$(APPOBJDIR)/%/$(TARGET),$(ExeTypes))
  $(PossibleExes): $(APPOBJDIR)/%/$(TARGET): \
                   Makefile \
                   $(CSRC_S) $(CSRC_M) $(ANLCSRC_S) $(ANLCSRC_M) \
                   $(CPPSRC_S) $(CPPSRC_M) $(CHDR_S) $(CHDR_M) $(ANLCHDR_S) $(ANLCHDR_M) \
                   $(FSRC_S) $(FSRC_M) $(F90SRC_S) $(F90SRC_M) $(OTHER_S) $(OTHER_M)
	@$(MAKE) --no-print-directory ExeType=$(patsubst $(APPOBJDIR)/%/$(TARGET),%,$@) $@
endif

ifdef ExeType
  CSRC_T    := $(patsubst Changed/%,$(APPOBJDIR)/$(ExeType)/%,$(CSRC_M)) \
               $(patsubst Source/%,$(APPOBJDIR)/$(ExeType)/%,$(CSRC_S))
  ANLCSRC_T := $(patsubst Changed/%.C,$(APPOBJDIR)/$(ExeType)/%.c,$(ANLCSRC_M)) \
               $(patsubst Source/%.C,$(APPOBJDIR)/$(ExeType)/%.c,$(ANLCSRC_S))
  CPPSRC_T  := $(patsubst Changed/%,$(APPOBJDIR)/$(ExeType)/%,$(CPPSRC_M)) \
               $(patsubst Source/%,$(APPOBJDIR)/$(ExeType)/%,$(CPPSRC_S))
  CHDR_T    := $(patsubst Changed/%,$(APPOBJDIR)/$(ExeType)/%,$(CHDR_M)) \
               $(patsubst Source/%,$(APPOBJDIR)/$(ExeType)/%,$(CHDR_S))
  ANLCHDR_T := $(patsubst Changed/%.H,$(APPOBJDIR)/$(ExeType)/%.h,$(ANLCHDR_M)) \
               $(patsubst Source/%.H,$(APPOBJDIR)/$(ExeType)/%.h,$(ANLCHDR_S))
  FSRC_T    := $(patsubst Changed/%,$(APPOBJDIR)/$(ExeType)/%,$(FSRC_M)) \
               $(patsubst Source/%,$(APPOBJDIR)/$(ExeType)/%,$(FSRC_S))
  F90SRC_T  := $(patsubst Changed/%,$(APPOBJDIR)/$(ExeType)/%,$(F90SRC_M)) \
               $(patsubst Source/%,$(APPOBJDIR)/$(ExeType)/%,$(F90SRC_S))
  OTHER_T   := $(patsubst Changed/%,$(APPOBJDIR)/$(ExeType)/%,$(OTHER_M)) \
               $(patsubst Source/%,$(APPOBJDIR)/$(ExeType)/%,$(OTHER_S))

  COBJ	    := $(patsubst %,%.o,$(CSRC_T) $(ANLCSRC_T))
  CPPOBJ    := $(patsubst %,%.o,$(CPPSRC_T))
  FOBJ	    := $(patsubst %,%.o,$(FSRC_T))
  F90OBJ    := $(patsubst %,%.o,$(F90SRC_T))
  OBJS	    := $(COBJ) $(CPPOBJ) $(FOBJ) $(F90OBJ)

  ifndef LINK
    ifneq ($(strip $(CPPOBJ)),)
      LINK=$(CPPC)
    else
      ifneq ($(strip $(F90OBJ)),)
        LINK=$(F90C)
      else
        ifneq ($(strip $(FOBJ)),)
          LINK=$(FC)
        else
          LINK=$(CC)
        endif
      endif
    endif
  endif

  $(APPOBJDIR)/$(ExeType)/$(TARGET): Makefile $(APPOBJDIR)/$(ExeType)/.placeholder $(OBJS)
	$(LINK) $(LPREFIX) $(OBJS) $(LFLAGS) -o $@

  $(COBJ): %.o : % Makefile $(CHDR_T) $(ANLCHDR_T) $(OTHER_T)
	$(CC) -c -o $@ $(CFLAGS) $<

  $(CPPOBJ): %.o : % Makefile $(CHDR_T) $(OTHER_T)
	$(CPPC) -c -o $@ $(CPPFLAGS) $<

  $(FOBJ): %.o : % Makefile $(OTHER_T)
	$(FC) -c -o $@ $(FFLAGS) $<

  $(F90OBJ): %.o : % Makefile $(OTHER_T)
	$(F90C) -J$(APPOBJDIR)/$(ExeType)/ -c -o $@ $(F90FLAGS) $<

  $(patsubst Changed/%,$(APPOBJDIR)/$(ExeType)/%,$(CSRC_M) $(CPPSRC_M) $(CHDR_M) $(FSRC_M) $(F90SRC_M) $(OTHER_M)): \
    $(APPOBJDIR)/$(ExeType)/%: Changed/%
  $(patsubst Changed/%.C,$(APPOBJDIR)/$(ExeType)/%.c,$(ANLCSRC_M)): $(APPOBJDIR)/$(ExeType)/%.c: Changed/%.C
  $(patsubst Changed/%.H,$(APPOBJDIR)/$(ExeType)/%.h,$(ANLCHDR_M)): $(APPOBJDIR)/$(ExeType)/%.h: Changed/%.H
  $(patsubst Source/%,$(APPOBJDIR)/$(ExeType)/%,$(CSRC_S) $(CPPSRC_S) $(CHDR_S) $(FSRC_S) $(F90SRC_S) $(OTHER_S)): \
    $(APPOBJDIR)/$(ExeType)/%: Source/%
  $(patsubst Source/%.C,$(APPOBJDIR)/$(ExeType)/%.c,$(ANLCSRC_S)): $(APPOBJDIR)/$(ExeType)/%.c: Source/%.C
  $(patsubst Source/%.H,$(APPOBJDIR)/$(ExeType)/%.h,$(ANLCHDR_S)): $(APPOBJDIR)/$(ExeType)/%.h: Source/%.H
  $(CSRC_T) $(ANLCSRC_T) $(CPPSRC_T) $(CHDR_T) $(ANLCHDR_T) $(FSRC_T) $(F90SRC_T) $(OTHER_T): \
    Makefile $(APPOBJDIR)/$(ExeType)/.placeholder

  ifneq ($(strip $(CSRC_T) $(CPPSRC_T) $(CHDR_T)),)
    $(CSRC_T) $(CPPSRC_T) $(CHDR_T): %:
	@cat $< > $@
	@echo >> $@
	@echo /\* Generated from ../$< \*/ >> $@
  endif

  ifneq ($(strip $(ANLCSRC_T) $(ANLCHDR_T)),)
    $(ANLCSRC_T) $(ANLCHDR_T): %:
	$(M4) $(M4FLAGS) $< > $@
	@echo >> $@
	@echo /\* Generated from ../$< \*/ >> $@
  endif

  ifneq ($(strip $(OTHER_T)),)
    $(OTHER_T): %:
	@cat $< > $@
  endif

  ifneq ($(strip $(FSRC_T)),)
     $(FSRC_T): %:
	@cat $< > $@
	@echo >> $@
	@echo C Generated from ../$< >> $@
  endif

  ifneq ($(strip $(F90SRC_T)),)
     $(F90SRC_T): %:
	@cat $< > $@
	@echo >> $@
	@echo ! Generated from ../$< >> $@
  endif

  $(APPOBJDIR)/$(ExeType)/.placeholder: $(APPOBJDIR)/.placeholder

  ifneq ($(strip $(F90SRC_T)),)
    # Fortran 90 Dependences

  $(APPOBJDIR)/$(ExeType)/.F90depend: $(F90SRC_T)
	@perl -e \
	'\
	  use File::Basename;\
	  my %WhereMod;\
	  open(OutFile,"> $@")\
	    or die "Cannot open depend file $@";\
	  foreach $$FileName (split(/ /,"$(strip $(F90SRC_M) $(F90SRC_S))")){\
	    open(InFile,"< $$FileName")\
	      or die "Cannot open F90 file $$FileName";\
	    my $$BaseName=basename($$FileName);\
	    while(<InFile>){\
	      if(/^[\s]*module[\s]+([^\s]*)/i){\
	        my $$ModName=lc($$1);\
	        print OutFile "$(APPOBJDIR)/$(ExeType)/$$ModName.mod: $(APPOBJDIR)/$(ExeType)/$$BaseName.o\n\n";\
                $$WhereMod{$$ModName}=$$BaseName;\
	      }\
	    }\
	    close(InFile);\
	  }\
	  foreach $$FileName (split(/ /,"$(strip $(F90SRC_M) $(F90SRC_S))")){\
	    open(InFile,"< $$FileName")\
	      or die "Cannot open F90 file $$FileName";\
	    my $$BaseName=basename($$FileName);\
	    while(<InFile>){\
              foreach $$SrcLine (split(/;/,$$_)){\
	        if($$SrcLine=~/^[\s]*use[\s]+([^\s,]+)/i){\
	          my $$ModName=lc($$1);\
	          if($$WhereMod{$$ModName} ne $$BaseName){\
                    print OutFile "$(APPOBJDIR)/$(ExeType)/$$BaseName.o: $(APPOBJDIR)/$(ExeType)/$$ModName.mod\n\n";\
	          }\
	        }\
	      }\
	    }\
	    close(InFile);\
	  }\
	  close(OutFile);\
	'

    -include $(APPOBJDIR)/$(ExeType)/.F90depend
  endif

endif

$(patsubst %,%/.placeholder,$(APPOBJDIR) $(APPOUTDIR) $(APPPRFDIR) $(APPOBJDIR)/$(ExeType)): %/.placeholder:
	@if [ ! -d $(patsubst %/.placeholder,%,$@) ] ; then \
	  mkdir $(patsubst %/.placeholder,%,$@) ; \
	fi
	@if [ ! -r $(patsubst %/.placeholder,%,$@)/.placeholder ] ; then \
	  echo > $(patsubst %/.placeholder,%,$@)/.placeholder ; \
	fi

.PHONY: clean
clean:
  ifneq ($(wildcard $(APPOBJDIR) $(APPOUTDIR) $(APPPRFDIR)),)
	@rm -r $(wildcard $(APPOBJDIR) $(APPOUTDIR) $(APPPRFDIR))
  endif
	@echo > /dev/null
