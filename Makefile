#
# Makefile for CDataFile2
#

EXE := test/cdfTest.out
OUTDIR := test
INTDIR := $(OUTDIR)/obj

VPATH := src test
CFLAGS += -Isrc
OBJS := $(notdir $(wildcard $(addsuffix /*.cpp, $(VPATH) ) ) )
OBJS := $(addprefix $(INTDIR)/, $(OBJS:.cpp=.o) )

#-------------------------

.PHONY : all

all : $(EXE)

$(EXE) : $(OBJS)
	$(CXX) -o $@ $(LFLAGS) $^

$(INTDIR)/%.o : %.cpp | $(INTDIR)
	$(CXX) -o $@  $(CFLAGS) -c $<

# Create output and intermed dirs
$(INTDIR) :
	mkdir -p $@
