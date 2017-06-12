all:$(BIN)

$(BIN):$(OBJ)
		$(CC) -o $(BIN) $^ $(LC65XX_LD_FLAGS)
		@cp -rf $(BIN) ..
		@rm -f $(BIN)

.c.o:
		$(CC) -c $^ $(LC65XX_CFLAGS)

clean:
		rm -f $(BIN) $(OBJ) ../$(BIN)
