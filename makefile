
CXX=g++
INCLUDES=
FLAGS=-D__MACOSX_CORE__ -O3 -Wno-deprecated -c
LIBS=-framework CoreAudio -framework CoreMIDI -framework CoreFoundation \
	-framework IOKit -framework Carbon  -framework OpenGL \
	-framework GLUT -framework Foundation \
	-framework AppKit -lstdc++ -lm

OBJS=   RtAudio.o ColorfulMusic.o chuck_fft.o x-vector3d.o x-fun.o

ColorfulMusic: $(OBJS)
	$(CXX) -o ColorfulMusic $(OBJS) $(LIBS)

ColorfulMusic.o: ColorfulMusic.cpp RtAudio.h
	$(CXX) $(FLAGS) ColorfulMusic.cpp

RtAudio.o: RtAudio.h RtAudio.cpp RtError.h
	$(CXX) $(FLAGS) RtAudio.cpp

chuck_fft.o: chuck_fft.h chuck_fft.c
	$(CXX) $(FLAGS) chuck_fft.c

x-vector3d.o: x-vector3d.h x-vector3d.cpp
		$(CXX) $(FLAGS) x-vector3d.cpp

x-fun.o: x-fun.h x-fun.cpp
		$(CXX) $(FLAGS) x-fun.cpp


clean:
	rm -f *~ *# *.o ColorfulMusic
