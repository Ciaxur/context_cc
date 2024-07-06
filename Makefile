TARGET := app
CC := g++
OPTIMIZATIONS := -O3 -g
CC_VERSION := c++17
FLAGS := -std=$(CC_VERSION) -lpthread -lfmt -Wall -Wextra -Wno-unused-parameter $(OPTIMIZATIONS)
INCLUDES := $(patsubst %,-I%, \
	./includes/ \
	$(wildcard ./include/**/) \
)

# Source files
SRCS 	:= $(wildcard ./*.cc ./*.cpp)
_OBJS := $(SRCS:.cc=.o)
OBJS 	:= $(_OBJS:.cpp=.o) $(GLFW_OBJS)

all: check $(TARGET)

# Main build binary target.
# $@ -> $(TARGET)
# $^ -> All pre-requisites, which are $(OBJS)
$(TARGET): $(OBJS)
	$(CC) $(INCLUDES) $(FLAGS) $^ -o $@

# Rule to compile source files to object files. This allows re-compiling modified source files.
# $< -> The %.cc's resolved filename
%.o: %.cc
	$(CC) $(INCLUDES) $(FLAGS) -c -o $@ $<
%.o: %.cpp
	$(CC) $(INCLUDES) $(FLAGS) -c -o $@ $<

clean:
	rm $(OBJS) $(TARGET)

# Runs static analysis on all source files.
check:
	cppcheck \
		--error-exitcode=1 \
		--enable=all \
		--suppress=missingIncludeSystem \
		--suppress=checkersReport \
		--language=c++ \
		--std=$(CC_VERSION) \
		$(SRCS)

valgrind: $(TARGET)
	valgrind ./$(TARGET)

debug:
	$(info Sources:)
	$(foreach src,$(SRCS),$(info - $(src)))

	$(info Objects:)
	$(foreach obj,$(OBJS),$(info - $(obj)))

	$(info Includes:)
	$(foreach file,$(INCLUDES),$(info - $(file)))

	$(info Flags:)
	$(foreach flag,$(FLAGS),$(info - $(flag)))

help:
	@echo "Available targets:"
	@echo "  all   				: Builds everything"
	@echo "  check  			: Runs static analysis on all source files"
	@echo "  clean  			: Cleans up all built objects, cache, and binaries"
	@echo "  debug  			: Prints debug menu, which contains resolved variables"
	@echo "  help   			: Prints this menu"
