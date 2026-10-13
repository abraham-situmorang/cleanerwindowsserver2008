# ============================================================================
# Cleaner Windows Server 2008 - Makefile
# Copyright (c) 2026 Abraham Situmorang - Nanoquity 2026 (www.nanoquity.com)
# ============================================================================

CXX      = g++
WINDRES  = windres
CXXFLAGS = -std=c++17 -O3 -static -mwindows -municode -DUNICODE -D_UNICODE -D_WIN32_WINNT=0x0601 -Wall -Wextra
LDFLAGS  = -lcomctl32 -lshlwapi -lshell32 -lole32 -lgdi32 -lcomdlg32 -ladvapi32 -s

TARGET   = CleanerWinServer2008.exe
SRCS     = src/main.cpp src/scanner.cpp
RES_OBJ  = src/resource.o

all: $(TARGET)

$(RES_OBJ): src/resource.rc src/resource.h src/app.manifest assets/app.ico
	$(WINDRES) src/resource.rc -O coff -o $(RES_OBJ)

$(TARGET): $(SRCS) src/scanner.h src/resource.h $(RES_OBJ)
	$(CXX) $(CXXFLAGS) $(SRCS) $(RES_OBJ) -o $(TARGET) $(LDFLAGS)

clean:
	rm -f $(RES_OBJ) $(TARGET)

.PHONY: all clean
