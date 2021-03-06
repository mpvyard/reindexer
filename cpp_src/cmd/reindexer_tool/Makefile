
CC_FILES  := $(shell find . -type f -name '*.cc') ../../pprof/backtrace.cc

OBJ_FILES := $(patsubst ./%.cc, .build/%.o, $(CC_FILES))

LIBDIR    := ../..
REINDEXER_TOOL := .build/reindexer_tool

CXXFLAGS  := -I$(LIBDIR) -I$(LIBDIR)/vendor -std=c++11 -g -Wall -Wpedantic -Wextra
LDFLAGS   :=  -L$(LIBDIR)/.build -lreindexer -lleveldb -lsnappy -lpthread  


ifeq ($(DEBUG_BUILD),1)
CXXFLAGS    := $(CXXFLAGS) -fsanitize=address -O0
LDFLAGS     := $(LDFLAGS) -fsanitize=address
else
CXXFLAGS    := $(CXXFLAGS) -DNDEBUG -O3 -Ofast -flto
CFLAGS      := $(CFLAGS) -DNDEBUG -O3  -Ofast -flto
#LDFLAGS     := $(LDFLAGS) -flto
#EXTRA_CFLAGS := -DREINDEX_SINGLETHREAD -DREINDEX_NOTIMING -DNDEBUG -flto
endif

all: $(REINDEXER_TOOL)

lib:
$(LIBDIR)/.build/libreindexer.a:
	make -j4 -s -C $(LIBDIR)

.build/%.o: ./%.cc
	@mkdir -p $(dir $@)
	@echo CXX $<
	@$(CXX) $(CXXFLAGS) -c $< -o $@

$(LIBDIR)/.build/libreindexer.a: lib

$(REINDEXER_TOOL): $(OBJ_FILES)  $(LIBDIR)/.build/libreindexer.a
	@echo LD $@
	@$(CXX) $^ $(LDFLAGS) -o $@ 

clean:
	rm -rf .build

.depend: $(CC_FILES)
	@$(CXX) -MM $(CXXFLAGS) $^ | sed "s/^\(.*\): \(.*\)\.\([cp]*\) /\.build\/\2.o: \2.\3 /" >.depend

-include .depend
