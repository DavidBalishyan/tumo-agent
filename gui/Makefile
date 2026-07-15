CC       := gcc
RAYLIB   := vendor/raylib
SRC      := src
BUILD    := build
CFLAGS   := -O2 -std=c11 -Wall -Wextra -I$(RAYLIB)/include -Ivendor/cjson
LIBS     := $(RAYLIB)/lib/libraylib.a -lGL -lm -lpthread -ldl -lrt -lX11
OBJ      := $(addprefix $(BUILD)/, main.o chat.o bridge.o ui.o paths.o cJSON.o)
OUT      := tumo-gui

# The binary is the only build product that lives outside build/.
$(OUT): $(OBJ)
	$(CC) -o $(OUT) $(OBJ) $(LIBS)

# One rule for our own translation units, one for the vendored cJSON.
$(BUILD)/%.o: $(SRC)/%.c | $(BUILD) $(RAYLIB)/include/raylib.h
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/cJSON.o: vendor/cjson/cJSON.c | $(BUILD) $(RAYLIB)/include/raylib.h
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD):
	mkdir -p $(BUILD)

# Recompile a unit when a header it includes changes.
$(BUILD)/chat.o:   $(SRC)/chat.h
$(BUILD)/paths.o:  $(SRC)/paths.h
$(BUILD)/bridge.o: $(SRC)/bridge.h $(SRC)/chat.h $(SRC)/paths.h vendor/cjson/cJSON.h
$(BUILD)/ui.o:     $(SRC)/ui.h $(SRC)/chat.h $(SRC)/paths.h $(SRC)/bridge.h
$(BUILD)/main.o:   $(SRC)/chat.h $(SRC)/bridge.h $(SRC)/ui.h $(SRC)/paths.h
$(BUILD)/cJSON.o:  vendor/cjson/cJSON.h

# The vendored deps aren't checked in; nudge the user to fetch them.
$(RAYLIB)/include/raylib.h:
	@echo "Dependencies missing. Run ./fetch-deps.sh first." && exit 1

# Build then launch. The GUI finds the project root from its own path, so it
# can be started from anywhere.
run: $(OUT)
	./$(OUT)

clean:
	rm -rf $(BUILD) $(OUT)

.PHONY: run clean
