CXX = g++
CXXFLAGS ?= -g -Wall -W -ansi # -pedantic
LDFLAGS ?= 
SED = sed
MV = mv
RM = rm

.SUFFIXES: .o .cpp

lib = libUnitTest++.a

src = \
 AssertException.cpp \
	Test.cpp \
	Checks.cpp \
	TestRunner.cpp \
	TestResults.cpp \
	TestReporter.cpp \
	TestReporterStdout.cpp \
	ReportAssert.cpp \
	TestList.cpp \
	TimeConstraint.cpp \
	TestDetails.cpp \
	MemoryOutStream.cpp \
	DeferredTestReporter.cpp \
	DeferredTestResult.cpp \
	XmlTestReporter.cpp \
	CurrentTest.cpp \
 Posix/SignalTranslator.cpp \
	Posix/TimeHelpers.cpp

objects = $(patsubst %.cpp, %.o, $(src))
dependencies = $(subst .o,.d,$(objects))

define make-depend
  $(CXX) $(CXXFLAGS) -M $1 | \
  $(SED) -e 's,\($(notdir $2)\) *:,$(dir $2)\1: ,' > $3.tmp
  $(SED) -e 's/#.*//' \
      -e 's/^[^:]*: *//' \
      -e 's/ *\\$$//' \
      -e '/^$$/ d' \
      -e 's/$$/ :/' $3.tmp >> $3.tmp
  $(MV) $3.tmp $3
endef


all: $(lib)

$(lib): $(objects) 
	@echo Creating $(lib) library...
	@ar cr $(lib) $(objects)
    
clean:
	-@$(RM) $(objects) $(dependencies) $(lib) 2> /dev/null

%.o : %.cpp
	@echo $<
	@$(call make-depend,$<,$@,$(subst .o,.d,$@))
	@$(CXX) $(CXXFLAGS) -c $< -o $(patsubst %.cpp, %.o, $<)


ifneq "$(MAKECMDGOALS)" "clean"
-include $(dependencies)
endif
