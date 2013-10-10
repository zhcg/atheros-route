TARGET = ../bin		# for Linux environment

INC = .

all: otp_gen add_checksum

otp_gen: otp_gen.c
	gcc -o ../bin/otp_gen -I$(INC) otp_gen.c

add_checksum: add_checksum.c
	gcc -o ../bin/add_checksum -I$(INC) add_checksum.c

clean:
	rm -f ../bin/otp_gen ../bin/add_checksum
