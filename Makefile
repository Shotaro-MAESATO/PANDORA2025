GCC = g++ -std=c++17 -O3
#TARGET = deco.mpv_FIRfilter
TARGET = deco.mpv
#TARGET = deco.mpv_online
OBJ = $(TARGET).o
CLASSOBJ = MyclassDict.o
ROOTFLAGS = $(shell root-config --cflags)
ROOTLIBS = $(shell root-config --libs) -lRHTTP
#DEBUG = -Wall
CFLAGS= $(DEBUG) $(ROOTFLAGS) -fPIC

$(TARGET): $(OBJ) $(CLASSOBJ)
	$(GCC) $(TARGET).cpp $(ROOTFLAGS) $(DEBUG) $(ROOTLIBS) -o $(TARGET) -w
	$(GCC) -shared $(DEBUG) $(CLASSOBJ) -o libMyclass.so

Myclass:
	@echo "Generating dictionary $@..."
	rootcling -f $(@)Dict.cpp -c LinkDef.h

.cpp.o:
	$(GCC) -c $< $(CFLAGS)

clean:
	rm -f ana_sakra *.o
	rm -f deco_mpv *.o
