CC = g++
CFLAGS = -std=c++11
LDFLAGS = -O3 -lm -lpthread -L/usr/X11R6/lib -ljpeg -lX11 -g 
FFFLAGS = -I .
TARGETS = seq farm map ff_farm ff_map farm_map ff_farm_map

all :
	$(CC) -o seq $(CFLAGS) seq.cpp $(LDFLAGS)
	$(CC) -o farm $(CFLAGS) farm.cpp $(LDFLAGS)
	$(CC) -o map $(CFLAGS) map.cpp $(LDFLAGS)
	$(CC) -o farm_map $(CFLAGS) farm_map.cpp $(LDFLAGS)
	$(CC) $(FFFLAGS) -o ff_map $(CFLAGS) ff_map.cpp $(LDFLAGS) 
	$(CC) $(FFFLAGS) -o ff_farm $(CFLAGS) ff_farm.cpp $(LDFLAGS)
	$(CC) $(FFFLAGS) -o ff_farm_map $(CFLAGS) ff_farm_map.cpp $(LDFLAGS)

clean:
	rm -f $(TARGETS)

seq: seq.cpp
	$(CC) -o seq $(CFLAGS) seq.cpp $(LDFLAGS)

farm: farm.cpp
	$(CC) -o farm $(CFLAGS) farm.cpp $(LDFLAGS)

map: map.cpp
	$(CC) -o map $(CFLAGS) map.cpp $(LDFLAGS)

farm_map: farm_map.cpp
	$(CC) -o farm_map $(CFLAGS) farm_map.cpp $(LDFLAGS)

ff_map: ff_map.cpp
	$(CC) $(FFFLAGS) -o ff_map $(CFLAGS) ff_map.cpp $(LDFLAGS)

ff_farm: ff_farm.cpp
	$(CC) $(FFFLAGS) -o ff_farm $(CFLAGS) ff_farm.cpp $(LDFLAGS)

ff_farm_map: ff_farm_map.cpp
	$(CC) $(FFFLAGS) -o ff_farm_map $(CFLAGS) ff_farm_map.cpp $(LDFLAGS)