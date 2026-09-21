# Makefile for Recoil Pattern Recorder & Analyzer
# Toolchain: MinGW-w64 (g++ / mingw32-make)

CXX = g++
CXXFLAGS = -std=c++20 -O2 -Wall -Wformat
INCLUDES = -Iinclude -Isrc -Ilibs/imgui -Ilibs/imgui/backends -Ilibs/implot -Ilibs/json
LDFLAGS = -mwindows -ld3d11 -ld3dcompiler -ldxgi -ldwmapi -limm32 -lgdi32 -lws2_32

BUILD_DIR = build
OBJ_DIR = $(BUILD_DIR)/obj

# Source files
IMGUI_SRCS = libs/imgui/imgui.cpp \
             libs/imgui/imgui_draw.cpp \
             libs/imgui/imgui_tables.cpp \
             libs/imgui/imgui_widgets.cpp \
             libs/imgui/backends/imgui_impl_win32.cpp \
             libs/imgui/backends/imgui_impl_dx11.cpp

IMPLOT_SRCS = libs/implot/implot.cpp \
              libs/implot/implot_items.cpp

APP_SRCS = src/main.cpp

ALL_SRCS = $(IMGUI_SRCS) $(IMPLOT_SRCS) $(APP_SRCS)
OBJS = $(patsubst %.cpp, $(OBJ_DIR)/%.o, $(ALL_SRCS))

TARGET = $(BUILD_DIR)/RecoilRecorder.exe

FIXPATH = $(subst /,\,$1)

all: $(TARGET)

$(TARGET): $(OBJS)
	@if not exist "$(call FIXPATH,$(BUILD_DIR))" mkdir "$(call FIXPATH,$(BUILD_DIR))"
	$(CXX) $(OBJS) -o $@ $(LDFLAGS)
	@echo Build complete: $(TARGET)

$(OBJ_DIR)/%.o: %.cpp
	@if not exist "$(call FIXPATH,$(dir $@))" mkdir "$(call FIXPATH,$(dir $@))"
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

clean:
	@if exist "$(call FIXPATH,$(BUILD_DIR))" rmdir /s /q "$(call FIXPATH,$(BUILD_DIR))"

TEST_TARGET = $(BUILD_DIR)/test_core.exe

test: $(TEST_TARGET)
	@echo Running core tests...
	@$(call FIXPATH,$(TEST_TARGET))

$(TEST_TARGET): tests/test_core.cpp
	@if not exist "$(call FIXPATH,$(BUILD_DIR))" mkdir "$(call FIXPATH,$(BUILD_DIR))"
	$(CXX) $(CXXFLAGS) $(INCLUDES) $< -o $@ -lws2_32

.PHONY: all clean test
