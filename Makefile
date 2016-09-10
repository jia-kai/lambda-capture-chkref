# $File: Makefile
# $Date: Sat Sep 10 16:26:56 2016 +0800
# $Author: jiakai <jia.kai66@gmail.com>

TARGET := chkref.so

SRC_EXT := cpp
override OPTFLAG ?= -O2

override CXXFLAGS += \
   	-ggdb \
	-Wall -Wextra -Wnon-virtual-dtor -Wno-unused-parameter -Winvalid-pch \
	-Wno-unused-local-typedefs \
	-Isrc $(shell llvm-config --cxxflags) -std=c++14 $(OPTFLAG)
override BUILD_DIR ?= build
override LDFLAGS += -pthread
override V ?= @

CXXSOURCES := $(shell find src -name "*.$(SRC_EXT)")
OBJS := $(addprefix $(BUILD_DIR)/,$(CXXSOURCES:.$(SRC_EXT)=.o))
DEPFILES := $(OBJS:.o=.d)


all: $(TARGET)

-include $(DEPFILES)

$(BUILD_DIR)/%.o: %.$(SRC_EXT)
	@echo "[cxx] $< ..."
	@mkdir -pv $(dir $@)
	@$(CXX) $(CXXFLAGS) -MM -MT "$@" "$<"  > "$(@:.o=.d)"
	$(V)$(CXX) -c $< -o $@ $(CXXFLAGS)


$(TARGET): $(OBJS)
	@echo "Linking ..."
	$(V)$(CXX) $(OBJS) -o $@ $(LDFLAGS) -shared

clean:
	rm -rf $(BUILD_DIR) $(TARGET)

run: $(TARGET)
	clang++ -std=c++14 test.cpp -c -o /dev/null $(shell ./cmdline.sh)

.PHONY: all clean run

# vim: ft=make

