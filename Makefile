CFLAGS=-Wall -O3 -g -std=c++11
CXXFLAGS=$(CFLAGS)
OBJECTS=ledwalld.o communicator.o led.o util.o models/request.pb.o
OBJECTS_TO_LINK=communicator.o led.o models/request.pb.o util.o  
BINARIES=ledwalld
LDFLAGS+=-lprotobuf

# Where our library resides. You mostly only need to change the
# RGB_LIB_DISTRIBUTION, this is where the library is checked out.
RGB_LIB_DISTRIBUTION=rpi-rgb-led-matrix
RGB_INCDIR=$(RGB_LIB_DISTRIBUTION)/include
RGB_LIBDIR=$(RGB_LIB_DISTRIBUTION)/lib
RGB_LIBRARY_NAME=rgbmatrix
RGB_LIBRARY=$(RGB_LIBDIR)/lib$(RGB_LIBRARY_NAME).a
LDFLAGS+=-L$(RGB_LIBDIR) -l$(RGB_LIBRARY_NAME) -lrt -lm -lpthread

#Image Magick Flags
MAGICK_CXXFLAGS=`GraphicsMagick++-config --cppflags --cxxflags`
MAGICK_LDFLAGS=`GraphicsMagick++-config --ldflags --libs`
AV_CXXFLAGS=`pkg-config --cflags  libavcodec libavformat libswscale libavutil`

all : $(BINARIES)

# Generate the protobuf sources so they match the installed protobuf version
%.pb.cc %.pb.h : %.proto
	protoc --cpp_out=. $<

$(OBJECTS) : models/request.pb.h

$(RGB_LIBRARY): FORCE
	$(MAKE) -C $(RGB_LIBDIR)

ledwalld : $(OBJECTS) $(RGB_LIBRARY)
	$(CXX) $< $(OBJECTS_TO_LINK) -o $@ $(LDFLAGS) $(MAGICK_LDFLAGS)

%.o : %.cc
	$(CXX) -I. -I$(RGB_INCDIR) $(CXXFLAGS) $(MAGICK_CXXFLAGS) -c -o $@ $<

clean:
	rm -f $(OBJECTS) $(BINARIES) models/request.pb.cc models/request.pb.h

FORCE:
.PHONY: FORCE
