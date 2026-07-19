CC             ="clang"
BIN_OUTPUT     ="$(pwd)bin/"
BIN_NAME       ="bf"
SRC            ="$(pwd)src"
CC_FLAGS       ="-Wextra" "-Wall" "-std=gnu99"
DEBUGGER       =""
INVOC_ARGS     =""

.PHONY: compile

compile:
	@mkdir -p $(BIN_OUTPUT)
	@$(CC) -o $(BIN_OUTPUT)$(BIN_NAME) -I $(SRC) $(SRC)/*.c $(CC_FLAGS)

run:
	@if [ $(DEBUGGER) = "" ]; then $(BIN_OUTPUT)$(BIN_NAME) $(INVOC_ARGS); else $(DEBUGGER) $(BIN_OUTPUT)$(BIN_NAME); fi

clean:
	@rm -r $(BIN_OUTPUT)

sequence: compile run
