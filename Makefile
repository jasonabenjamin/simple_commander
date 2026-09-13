# Simple Commander - Windows / MSYS2 MinGW-w64
# Builds the modular source tree as simple.exe with the application icon.

CXX = g++
WINDRES = windres
PKG_CONFIG = pkg-config

TARGET = simple.exe
ICON = SimpleCommander.ico
RESOURCE_RC = simple_commander.rc
RESOURCE_OBJ = simple_commander_res.o

CXXFLAGS = -std=c++17 -Wall -Wextra -O2 $(shell $(PKG_CONFIG) --cflags sdl2 SDL2_ttf)
LDLIBS = $(shell $(PKG_CONFIG) --libs sdl2 SDL2_ttf)

# state.cpp contains the shared global program state.
SOURCES = main.cpp state.cpp core.cpp views.cpp ui.cpp operations.cpp viewer.cpp editor.cpp commands.cpp
OBJECTS = $(SOURCES:.cpp=.o)

.PHONY: all clean run

all: $(TARGET)

$(TARGET): $(OBJECTS) $(RESOURCE_OBJ)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDLIBS)

%.o: %.cpp simple_commander.h
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(RESOURCE_RC): $(ICON)
	@printf '1 ICON "%s"\n' "$(ICON)" > $@

$(RESOURCE_OBJ): $(RESOURCE_RC) $(ICON)
	$(WINDRES) $(RESOURCE_RC) -O coff -o $@

run: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(TARGET) $(OBJECTS) $(RESOURCE_OBJ) $(RESOURCE_RC)
