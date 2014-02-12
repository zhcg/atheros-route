all:$(BIN)

$(BIN):$(OBJ)
		$(CC) -o $(BIN) $^ $(PHONE_LD_FLAGS) ../lib/si32178.a ../lib/dflib.a
		@cp -rf $(BIN) ..
		@rm -f $(BIN)

.c.o:
		$(CC) -c $^ $(PHONE_CFLAGS)

clean:
		rm -f $(BIN) $(OBJ) ../$(BIN)
		
