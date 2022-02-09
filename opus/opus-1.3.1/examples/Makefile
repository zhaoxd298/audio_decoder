
INCLUDE = ../include/
CFLAGS = -logg -lopus 

opus_decoder:opus_decoder.c
	gcc opus_decoder.c -I${INCLUDE} ${CFLAGS} -o opus_decoder

clean:
	rm opus_decoder