CARGO        ?= cargo
CARGO_TARGET_DIR ?= target
BIN_OUTPUT   ?= bin
BIN_NAME     ?= bf
FEATURES     ?=
TARGET       ?=
CARGO_FLAGS  ?=
DEBUGGER     ?=
INVOC_ARGS   ?=

FEATURE_ARGS := $(if $(strip $(FEATURES)),--features "$(FEATURES)",)
TARGET_ARGS  := $(if $(strip $(TARGET)),--target "$(TARGET)",)
TARGET_PATH  := $(CARGO_TARGET_DIR)/$(if $(strip $(TARGET)),$(TARGET)/,)release/bf

.PHONY: compile run test feature-test parity-test clean sequence

compile:
	@CARGO_TARGET_DIR="$(CARGO_TARGET_DIR)" $(CARGO) build --release $(TARGET_ARGS) $(FEATURE_ARGS) $(CARGO_FLAGS)
	@mkdir -p "$(BIN_OUTPUT)"
	@cp "$(TARGET_PATH)" "$(BIN_OUTPUT)/$(BIN_NAME)"

run: compile
	@if [ -z "$(DEBUGGER)" ]; then \
		"$(BIN_OUTPUT)/$(BIN_NAME)" $(INVOC_ARGS); \
	else \
		$(DEBUGGER) "$(BIN_OUTPUT)/$(BIN_NAME)" $(INVOC_ARGS); \
	fi

test:
	@CARGO_TARGET_DIR="$(CARGO_TARGET_DIR)" $(CARGO) test --all-targets $(FEATURE_ARGS) $(CARGO_FLAGS)

feature-test:
	@CARGO="$(CARGO)" CARGO_TARGET_DIR="$(CARGO_TARGET_DIR)" tests/feature-matrix.sh

parity-test:
	@CARGO="$(CARGO)" tests/differential.sh

clean:
	@CARGO_TARGET_DIR="$(CARGO_TARGET_DIR)" $(CARGO) clean
	@rm -rf -- "$(BIN_OUTPUT)"

sequence: run
