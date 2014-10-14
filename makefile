
CXX=g++
INCLUDES=
FLAGS=-D__MACOSX_CORE__ -O3 -c
LIBS=-framework CoreAudio -framework CoreMIDI -framework CoreFoundation \
	-framework IOKit -framework Carbon  -framework OpenGL \
	-framework GLUT -framework Foundation \
	-framework AppKit -lstdc++ -lm

OBJS=   RtAudio.o VisualSpectrum.o chuck_fft.o

VisualSpectrum: $(OBJS)
	$(CXX) -o VisualSpectrum $(OBJS) $(LIBS)

VisualSpectrum.o: VisualSpectrum.cpp RtAudio.h
	$(CXX) $(FLAGS) VisualSpectrum.cpp

RtAudio.o: RtAudio.h RtAudio.cpp RtError.h
	$(CXX) $(FLAGS) RtAudio.cpp

chuck_fft.o: chuck_fft.h chuck_fft.c
	$(CXX) $(FLAGS) chuck_fft.c

clean:
	rm -f *~ *# *.o VisualSpectrum
