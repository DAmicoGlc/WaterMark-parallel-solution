CC = g++
CFLAGS = -std=c++11
LDFLAGS = -O3 -lm -lpthread -L/usr/X11R6/lib -lX11 -ljpeg -g
FFFLAGS = -I .
TARGETS = seq_test farm_test map_test farm_map_test ff_farm_test ff_map_test ff_farm_map_test provaBlack

all :
	$(CC) -o seq_test $(CFLAGS) seq_test.cpp $(LDFLAGS)
	$(CC) -o farm_test $(CFLAGS) farm_test.cpp $(LDFLAGS)
	$(CC) -o map_test $(CFLAGS) map_test.cpp $(LDFLAGS)
	$(CC) -o farm_map_test $(CFLAGS) farm_map_test.cpp $(LDFLAGS)
	$(CC) $(FFFLAGS) -o ff_farm_test $(CFLAGS) ff_farm_test.cpp $(LDFLAGS)
	$(CC) $(FFFLAGS) -o ff_map_test $(CFLAGS) ff_map_test.cpp $(LDFLAGS)
	$(CC) $(FFFLAGS) -o ff_farm_map_test $(CFLAGS) ff_farm_map_test.cpp $(LDFLAGS)

clean:
	rm -f $(TARGETS)

seq_test: seq_test.cpp
	$(CC) -o seq_test $(CFLAGS) seq_test.cpp $(LDFLAGS)

farm_test: farm_test.cpp
	$(CC) -o farm_test $(CFLAGS) farm_test.cpp $(LDFLAGS)

map_test: map_test.cpp
	$(CC) -o map_test $(CFLAGS) map_test.cpp $(LDFLAGS)

farm_map_test: farm_map_test.cpp
	$(CC) -o farm_map_test $(CFLAGS) farm_map_test.cpp $(LDFLAGS)

ff_farm_test: ff_farm_test.cpp
	$(CC) $(FFFLAGS) -o ff_farm_test $(CFLAGS) ff_farm_test.cpp $(LDFLAGS)

ff_map_test: ff_map_test.cpp
	$(CC) $(FFFLAGS) -o ff_map_test $(CFLAGS) ff_map_test.cpp $(LDFLAGS)

ff_farm_map_test: ff_farm_map_test.cpp
	$(CC) $(FFFLAGS) -o ff_farm_map_test $(CFLAGS) ff_farm_map_test.cpp $(LDFLAGS)

provaBlack: provaBlack.cpp
	$(CC) -o provaBlack $(CFLAGS) provaBlack.cpp $(LDFLAGS)