//-----------------------------------------------------------------------------
// name: VisualSpectrum.cpp
// desc: hello FFT spectrum, real-time
//
// author: Ge Wang (ge@ccrma.stanford.edu)
//   date: fall 2014
//   uses: RtAudio by Gary Scavone
//-----------------------------------------------------------------------------
#include "RtAudio.h"
#include "chuck_fft.h"
#include <math.h>
#include <stdlib.h>
#include <iostream>
#include "x-vector3d.h"
#include "x-fun.h"

using namespace std;

#ifdef __MACOSX_CORE__
#include <GLUT/glut.h>
#include <OpenGL/gl.h>
#include <OpenGL/glu.h>
#include <OpenGL/OpenGL.h>
#else
#include <GL/gl.h>
#include <GL/glu.h>
#include <GL/glut.h>
#endif



//-----------------------------------------------------------------------------
// function prototypes
//-----------------------------------------------------------------------------
void help();
void initGfx();
void idleFunc();
void displayFunc();
void update(int);
Vector3D getMixedRandomColor(Vector3D);
void reshapeFunc( GLsizei width, GLsizei height );
void keyboardFunc( unsigned char, int, int );
void specialKeyboardFunc(int, int, int );
void mouseFunc( int button, int state, int x, int y );

// our datetype
#define SAMPLE float
// corresponding format for RtAudio
#define MY_FORMAT RTAUDIO_FLOAT32
// sample rate
#define MY_SRATE 44100
// number of channels
#define MY_CHANNELS 1
// for convenience
#define MY_PIE 3.14159265358979
#define HISTORY_SIZE 150
#define GRAVITY 3.0
#define TIMER_MS 25
#define NUM_FREQ_SEGMENTS 14

#define AMPLITUDE_CHANGE_THRESHOLD 2
#define PITCH_THRESHOLD 0.8
#define AMPLITUDE_HIGH_THRESHOLD 4
#define AMPLITUDE_EXPLODE_THRESHOLD 6

float STEP_TIME = 0.01;
float NUM_PARTICLES = 1000;
float PARTICLE_SIZE = 0.1;

enum DISPLAY_MODE { WATER_FALL=0, WOBBLE=1, PARTICLES=2 };

// width and height
long g_width = 1024;
long g_height = 720;
long g_last_width = g_width;
long g_last_height = g_height;
// global buffer
SAMPLE * g_buffer = NULL;
// fft buffer
SAMPLE * g_fftBuf = NULL;
long g_bufferSize;
// window
SAMPLE * g_window = NULL;
long g_windowSize;
float ** g_fftBufs = NULL;
float ** g_simpleBufs = NULL;
Vector3D g_color = Vector3D(0.5, 0.5, 1);
Vector3D *g_colors = new Vector3D[HISTORY_SIZE];

// global variables
GLboolean g_fullscreen = FALSE;
DISPLAY_MODE g_displayMode = WATER_FALL;
float g_maxAmp = -1;
float g_maxAmp_old = -1;
int g_maxAmpIndex = -1;
int g_maxAmpIndex_old = -1;
int g_maxFreqIndex = 0;
bool isColorful = false;
bool isAmplitudeHighEnabled = false;
bool isSpiral = false;
bool isTornado = false;
bool isBothEnabled = false;

bool isAmplitudeChanged() {
  return (g_maxAmp - g_maxAmpIndex_old > AMPLITUDE_CHANGE_THRESHOLD);
}

bool isAmplitudeHigh() {
  return g_maxAmp > AMPLITUDE_HIGH_THRESHOLD;
}

//-----------------------------------------------------------------------------
// Particle System
//-----------------------------------------------------------------------------

struct Particle {
  Vector3D location;
  Vector3D color;
  float scale;
  Vector3D rotation;
  float rot_radius;
  float rot_degrees;
  Vector3D velocity;
  float age; // The amount of time this particle has been alive
  float lifespan; // Total amount of time this particle is to live.
};

Vector3D rotate(Vector3D v, Vector3D axis, float degrees) {
  axis.normalize();
  float radians = degrees * MY_PIE / 180;
  float s = sin(radians);
  float c = cos(radians);
  return v * c + axis * (axis * v) * (1 - c) + (v ^ axis) * s;
}

Vector3D rotatedParticlePos(Vector3D pos) {
  return rotate(pos, Vector3D(1, 0, 0), -30);
}

bool compareParticles(Particle *p1, Particle *p2) {
  return rotatedParticlePos(p1->location).y < rotatedParticlePos(p2->location).y;
}
  
class ParticleEngine {
  private:
    Particle particles[20000];
    float timeUntilNextStep;
    Vector3D color;
    int particleCounter;
    float particleRotation;

    Vector3D currentVelocity() {
      float angle = XFun::rand2f(0, 6.28);
      return Vector3D(2 * cos(angle), 2.0f, 2 * sin(angle));
    }

    void createNewParticle(Particle *p) {
      p->location = Vector3D(0, 0, 0);
      p->velocity = currentVelocity() + Vector3D(XFun::rand2f(-0.2, 2), 
                      XFun::rand2f(-0.2, 2), XFun::rand2f(-0.2, 2));
      p->age = 0;
      p->lifespan = XFun::rand2f(0, 2) + 1;
      if (isColorful) 
        p->color = getMixedRandomColor(g_color);
      else 
        p->color = g_color;
      p->rot_radius = XFun::rand2f(g_maxAmp-1, g_maxAmp+2);
      p->rot_degrees = XFun::rand2f(0, 6.28);
      p->scale = PARTICLE_SIZE;
    }

    void step() {
      for (int i = 0; i < NUM_PARTICLES; i++) {
        Particle *p = particles + i;

        if (isSpiral) {
          p->rot_degrees += STEP_TIME;
          if (isTornado) {
            p->rot_radius += 5 * STEP_TIME;
          } else {
            p->rot_radius += XFun::rand2f(-10*STEP_TIME, 10*STEP_TIME);
          }
          p->location.set(
              p->rot_radius * cos(p->rot_degrees), 
              p->location.y += 10 * STEP_TIME, 
              p->rot_radius * sin(p->rot_degrees)
          );
        } else {
          p->location += p->velocity * STEP_TIME;
          /*
          if (isAmplitudeChanged()) 
            p->location += p->velocity * STEP_TIME * 3; 
            */
        }

        //p->velocity += Vector3D(0, -GRAVITY * STEP_TIME, 0);
        p->age += STEP_TIME;
        if (p->age >= p->lifespan) createNewParticle(p);
      }
    }
  public:
    ParticleEngine() {
      timeUntilNextStep = 0;
      particleCounter = 0;
      particleRotation = 0.0;
      for (int i = 0; i < NUM_PARTICLES; i++) {
        createNewParticle(particles+i);
      }
      for (int i = 0; i < 1 / STEP_TIME; i++) {
        step();
      }
    }

    void changeColor(Vector3D newColor) {
      color.set(
          (newColor.x + color.x) / 2, 
          (newColor.y + color.y) / 2, 
          (newColor.z + color.z) / 2
        );
    }

    void advance(float dt) {
      while (dt > 0) {
        if (timeUntilNextStep < dt) {
          dt -= timeUntilNextStep;
          step();
          timeUntilNextStep = STEP_TIME;
        }
        else {
          timeUntilNextStep = STEP_TIME;
          dt = 0;
        }
      }
    }

    void render() {
      vector<Particle*> particleVector;
      for (int i = 0; i < NUM_PARTICLES; i++) {
        particleVector.push_back(particles + i);
      }
      sort(particleVector.begin(), particleVector.end(), compareParticles);
      glBegin(GL_QUADS);
      for (int i = 0; i < particleVector.size(); i++) {
        Particle *p = particleVector[i];
        glColor4f(p->color.x, p->color.y, p->color.z, (1 - p->age/p->lifespan)+0.2);

        float size = p->scale;
        if (isAmplitudeHighEnabled && isAmplitudeHigh()) size *= 2;
        Vector3D pos = rotatedParticlePos(p->location);

        glTexCoord2f(0, 0);
        glVertex3f(pos.x - size, pos.y - size, pos.z);
        glTexCoord2f(0, 1);
        glVertex3f(pos.x - size, pos.y + size, pos.z);
        glTexCoord2f(1, 1);
        glVertex3f(pos.x + size, pos.y + size, pos.z);
        glTexCoord2f(1, 0);
        glVertex3f(pos.x + size, pos.y - size, pos.z);
      }
      glEnd();
    }
};

ParticleEngine *g_particleEngine;


void initializeFftBufs() {
  g_fftBufs = new float*[HISTORY_SIZE];
  for (int i = 0; i < HISTORY_SIZE; i++) {
    g_fftBufs[i] = new float[g_bufferSize];
    for (int j = 0; j < g_bufferSize; j++) {
      g_fftBufs[i][j] = 0;
    }
  }
}

void shiftRightFftBufs(complex* current) {
  for (int i = HISTORY_SIZE-1; i > 0; i--) {
    g_colors[i].set(g_colors[i-1].x, g_colors[i-1].y, g_colors[i-1].z);
    memmove(&g_fftBufs[i][0], &g_fftBufs[i-1][0], g_bufferSize * sizeof(float));
  }
  float max = 0;
  for (int i = 0; i < g_windowSize/2; i++) {
    g_colors[0].set(g_color.x, g_color.y, g_color.z);
    g_fftBufs[0][i] = 30 * pow(cmp_abs(current[i]),0.5);
  }
}

Vector3D getMixedRandomColor(Vector3D mixColor) {
  float r = XFun::rand2f(0, 1);
  float g = XFun::rand2f(0, 1);
  float b = XFun::rand2f(0, 1);

  r = 0.4 * r + 0.6 * mixColor.x;
  g = 0.4 * g + 0.6 * mixColor.y;
  b = 0.4 * b + 0.6 * mixColor.z;

  return Vector3D(r, g, b);
}

void computeAmplitudeAndFrequency() {
  g_maxAmp_old = g_maxAmp;
  g_maxAmpIndex_old = g_maxAmpIndex;
  g_maxAmp = -1;
  g_maxAmpIndex = -1;
  g_maxFreqIndex = -1;

  SAMPLE maxAmp = -1;
  int maxIndex = -1;
  for (int i = 0; i < g_windowSize; i++) {
    if (g_fftBufs[0][i] > maxAmp) {
      maxAmp = g_fftBufs[0][i];
      maxIndex = i;
    }
    if (g_fftBufs[0][i] > PITCH_THRESHOLD) {
      g_maxFreqIndex = i;
    }
  }

  g_maxAmp = maxAmp;
  //cout << "MaxAmp: " << g_maxAmp << endl;
  g_maxAmpIndex = maxIndex;
}

Vector3D getFreqColor() {
  int f = (g_maxFreqIndex * 1.0 / (g_windowSize/2)) * NUM_FREQ_SEGMENTS; 
  switch (f) {
    case 0:
      return Vector3D(0.8, 0.8, 0.8);
    case 1:
      return Vector3D(1.0, 1.0, 1.0);
    case 2:
      return Vector3D(1, 0.9, 0.3);
    case 3:
      return Vector3D(1, 0.8, 0);
    case 4:
      return Vector3D(1, 0.6, 0.2);
    case 5:
      return Vector3D(1, 0.4, 0.0);
    case 6:
      return Vector3D(1, 0.2, 0.0);
    default:
      return Vector3D(1, 0, 0);
  }

}


//-----------------------------------------------------------------------------
// name: callme()
// desc: audio callback
//-----------------------------------------------------------------------------
int callme( void * outputBuffer, void * inputBuffer, unsigned int numFrames,
    double streamTime, RtAudioStreamStatus status, void * data )
{
  // cast!
  SAMPLE * input = (SAMPLE *)inputBuffer;
  SAMPLE * output = (SAMPLE *)outputBuffer;

  // fill
  for( int i = 0; i < numFrames; i++ )
  {
    // assume mono
    g_buffer[i] = input[i];
    // zero output
    output[i] = 0;
  }

  return 0;
}




//-----------------------------------------------------------------------------
// name: main()
// desc: entry point
//-----------------------------------------------------------------------------
int main( int argc, char ** argv )
{
  // instantiate RtAudio object
  RtAudio audio;
  // variables
  unsigned int bufferBytes = 0;
  // frame size
  unsigned int bufferFrames = 1024;

  // check for audio devices
  if( audio.getDeviceCount() < 1 )
  {
    // nopes
    cout << "no audio devices found!" << endl;
    exit( 1 );
  }

  // initialize GLUT
  glutInit( &argc, argv );
  // init gfx
  initGfx();

  // let RtAudio print messages to stderr.
  audio.showWarnings( true );

  // set input and output parameters
  RtAudio::StreamParameters iParams, oParams;
  iParams.deviceId = audio.getDefaultInputDevice();
  iParams.nChannels = MY_CHANNELS;
  iParams.firstChannel = 0;
  oParams.deviceId = audio.getDefaultOutputDevice();
  oParams.nChannels = MY_CHANNELS;
  oParams.firstChannel = 0;

  // create stream options
  RtAudio::StreamOptions options;

  // go for it
  try {
    // open a stream
    audio.openStream( &oParams, &iParams, MY_FORMAT, MY_SRATE, &bufferFrames, &callme, (void *)&bufferBytes, &options );
  }
  catch( RtError& e )
  {
    // error!
    cout << e.getMessage() << endl;
    exit( 1 );
  }

  // compute
  bufferBytes = bufferFrames * MY_CHANNELS * sizeof(SAMPLE);
  // allocate global buffer
  g_bufferSize = bufferFrames;
  initializeFftBufs();
  g_buffer = new SAMPLE[g_bufferSize];
  g_fftBuf = new SAMPLE[g_bufferSize];
  memset( g_buffer, 0, sizeof(SAMPLE)*g_bufferSize );
  memset( g_fftBuf, 0, sizeof(SAMPLE)*g_bufferSize );

  // allocate buffer to hold window
  g_windowSize = bufferFrames;
  g_window = new SAMPLE[g_windowSize];
  // generate the window
  hanning( g_window, g_windowSize );

  // print help
  help();

  // go for it
  try {
    // start stream
    audio.startStream();

    g_particleEngine = new ParticleEngine();
    glutTimerFunc(TIMER_MS, update, 0);
    // let GLUT handle the current thread from here
    glutMainLoop();

    // stop the stream.
    audio.stopStream();
  }
  catch( RtError& e )
  {
    // print error message
    cout << e.getMessage() << endl;
    goto cleanup;
  }

cleanup:
  // close if open
  if( audio.isStreamOpen() )
    audio.closeStream();

  // done
  return 0;
}


//-----------------------------------------------------------------------------
// Name: reshapeFunc( )
// Desc: called when window size changes
//-----------------------------------------------------------------------------
void initGfx()
{
  // double buffer, use rgb color, enable depth buffer
  glutInitDisplayMode( GLUT_DOUBLE | GLUT_RGBA | GLUT_DEPTH | GLUT_ALPHA);
  // initialize the window size
  glutInitWindowSize( g_width, g_height );
  // set the window postion
  glutInitWindowPosition( 100, 100 );
  // create the window
  glutCreateWindow( "VisualSine" );

  // set the idle function - called when idle
  glutIdleFunc( idleFunc );
  // set the display function - called when redrawing
  glutDisplayFunc( displayFunc );
  // set the reshape function - called when client area changes
  glutReshapeFunc( reshapeFunc );
  // set the keyboard function - called on keyboard events
  glutKeyboardFunc( keyboardFunc );
  glutSpecialFunc(specialKeyboardFunc);
  // set the mouse function - called on mouse stuff
  glutMouseFunc( mouseFunc );


  // set clear color
  glClearColor( 0, 0, 0, 1 );

  // enable color material
  glEnable( GL_COLOR_MATERIAL );
  // enable depth test
  glEnable( GL_DEPTH_TEST );
  glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}


//-----------------------------------------------------------------------------
// Name: reshapeFunc( )
// Desc: called when window size changes
//-----------------------------------------------------------------------------
void reshapeFunc( GLsizei w, GLsizei h )
{
  // save the new window size
  g_width = w; g_height = h;
  // map the view port to the client area
  glViewport( 0, 0, w, h );
  // set the matrix mode to project
  glMatrixMode( GL_PROJECTION );
  // load the identity matrix
  glLoadIdentity( );
  // create the viewing frustum
  gluPerspective( 45.0, (GLfloat) w / (GLfloat) h, 1.0, 300.0 );
  // set the matrix mode to modelview
  glMatrixMode( GL_MODELVIEW );
  // load the identity matrix
  glLoadIdentity( );
  // position the view point
  gluLookAt( 0.0f, 5.8f, 10.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f );
}

//-----------------------------------------------------------------------------
// Name: help( )
// Desc: print usage
//-----------------------------------------------------------------------------
void help()
{
  cerr << "----------------------------------------------------" << endl;
  cerr << "SUNDANCE DOTS(v1.0)" << endl;
  cerr << "ALEXANDER HSU" << endl;
  cerr << "https://ccrma.stanford.edu/~kzm/hw2.html" << endl;
  cerr << "----------------------------------------------------" << endl;
  cerr << "----------------------------------------------------" << endl;
  cerr << "MODES" << endl;
  cerr << "'w' - display waterfall spectrum" << endl;
  cerr << "'p' - display flowing particles" << endl;
  cerr << "----------------------------------------------------" << endl;
  cerr << "COMMANDS" << endl;
  cerr << "'s' - toggle fullscreen" << endl;
  cerr << "'q' - quit visualization" << endl;
  cerr << "'h' - print this help message" << endl;
  cerr << "'c' - toggle 'colorful' mode" << endl;
  cerr << "'o' - toggle particles spiral mode" << endl;
  cerr << "'t' - toggle particles spiral tornado mode" << endl;
  cerr << "'a' - toggle high amplitude detection" << endl;
  cerr << "',' - make particles smaller" << endl;
  cerr << "'.' - make particles bigger" << endl;
  cerr << "ARROW_UP' - make particles faster" << endl;
  cerr << "ARROW_DOWN' - make particles slower" << endl;
  cerr << "ARROW_LEFT' - make less particles" << endl;
  cerr << "ARROW_RIGHT' - make more particles" << endl;
  cerr << "----------------------------------------------------" << endl;
}



void specialKeyboardFunc(int key, int x, int y) {
  switch (key) {
    case GLUT_KEY_UP:
      STEP_TIME *= 2;
      break;
    case GLUT_KEY_DOWN:
      STEP_TIME /= 2;
      break;
    case GLUT_KEY_RIGHT:
      if (NUM_PARTICLES <= 10000) 
        NUM_PARTICLES *= 2;
      break;
    case GLUT_KEY_LEFT:
      if (NUM_PARTICLES > 1)
        NUM_PARTICLES /= 2;
      break;
  }
    
}

//-----------------------------------------------------------------------------
// Name: keyboardFunc( )
// Desc: key event
//-----------------------------------------------------------------------------
void keyboardFunc( unsigned char key, int x, int y )
{
  
  switch( key )
  {
    case 'q': // quit
      exit(1);
      break;

    case 'h': // print help
      help();
      break;

    case 's': // toggle fullscreen
      {
        // check fullscreen
        if( !g_fullscreen )
        {
          g_last_width = g_width;
          g_last_height = g_height;
          glutFullScreen();
        }
        else
          glutReshapeWindow( g_last_width, g_last_height );

        // toggle variable value
        g_fullscreen = !g_fullscreen;
      }
      break;

    case 'p':
      g_displayMode = PARTICLES;
      break;

    case 'w':
      g_displayMode = WATER_FALL;
      break;

    case 'c':
      isColorful = !isColorful;
      break;

    case 'o':
      g_displayMode = PARTICLES;
      isSpiral = !isSpiral;
      break;

    case 't':
      g_displayMode = PARTICLES;
      isSpiral = true;
      isTornado = !isTornado;
      break;

    case 'a':
      isAmplitudeHighEnabled = !isAmplitudeHighEnabled;
      break;
    case '.':
      if (PARTICLE_SIZE < 0.2)
        PARTICLE_SIZE *= 2;
      break;
    case ',':
      if (PARTICLE_SIZE > 0.02)
        PARTICLE_SIZE /= 2;
      break;
    case 'b':
      isBothEnabled = !isBothEnabled;
      NUM_PARTICLES = 100;
      break;
  }

  // trigger redraw
  glutPostRedisplay( );
}




//-----------------------------------------------------------------------------
// Name: mouseFunc( )
// Desc: handles mouse stuff
//-----------------------------------------------------------------------------
void mouseFunc( int button, int state, int x, int y )
{
  if( button == GLUT_LEFT_BUTTON )
  {
    // when left mouse button is down
    if( state == GLUT_DOWN )
    {
    }
    else
    {
    }
  }
  else if ( button == GLUT_RIGHT_BUTTON )
  {
    // when right mouse button down
    if( state == GLUT_DOWN )
    {
    }
    else
    {
    }
  }
  else
  {
  }

  glutPostRedisplay( );
}




//-----------------------------------------------------------------------------
// Name: idleFunc( )
// Desc: callback from GLUT
//-----------------------------------------------------------------------------
void idleFunc( )
{
  // render the scene
  glutPostRedisplay( );
}

void drawWaterFallMode()
{
  // define a starting point
  GLfloat x = -5;
  // compute increment
  GLfloat xinc = ::fabs(x*2 / g_bufferSize);
  xinc = fabs(x*2 / (g_windowSize / 2));
  glLineWidth(3);


  for (int i = HISTORY_SIZE-1; i >= 0; i--) {
    glColor4f(g_colors[i].x, g_colors[i].y, g_colors[i].z, 0.7);
    x = -5;
    // save transformation state
    glPushMatrix();
    // translate
    glTranslatef( 0, -2, 0 );
    // start primitive
    // loop over buffer to draw spectrum
    for( int j = 0; j < g_windowSize/2; j++ )
    {
      
      if (isColorful) {
        Vector3D lineColor = getMixedRandomColor(g_colors[i]);
        glColor4f(lineColor.x, lineColor.y, lineColor.z, 0.9);
      }
      x += xinc;
      glPushMatrix();
      glBegin(GL_LINES);
      // plot the magnitude,
      // with scaling, and also "compression" via pow(...)
      GLfloat z = 5 - i * 0.4;
      glVertex3f( x, 0, z);
      if (isAmplitudeHighEnabled && isAmplitudeHigh())
        glVertex3f( x, 1.4 * g_fftBufs[i][j], z);
      else 
        glVertex3f( x, g_fftBufs[i][j], z);
      glEnd();
      // increment x
      glPopMatrix();
    }

    // end primitive
    // restore transformations
    glPopMatrix();
  }
}

void update(int value) {
  if (g_displayMode == PARTICLES) {
    g_particleEngine->advance(TIMER_MS / 1000.0f);
    glutPostRedisplay();
  }
  glutTimerFunc(TIMER_MS, update, 0);
}

void drawParticlesMode() {
  glPushMatrix();
  g_particleEngine->render();
  glPopMatrix();
}

void drawWobbleMode() {

}
long g_t = 0;
//-----------------------------------------------------------------------------
// Name: displayFunc( )
// Desc: callback function invoked to draw the client area
//-----------------------------------------------------------------------------
void displayFunc( )
{
  // local state
  static GLfloat zrot = 0.0f, c = 0.0f;

  // clear the color and depth buffers
  glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );


  // copy into the fft buf
  memcpy(g_fftBuf, g_buffer, sizeof(SAMPLE)*g_bufferSize);
  // apply window to buf
  apply_window(g_fftBuf, g_window, g_windowSize);
  // take forward FFT (time domain signal -> frequency domain signal)
  rfft(g_fftBuf, g_windowSize/2, FFT_FORWARD);
  // cast the result to a buffer of complex values (re,im)
  complex *cbuf = (complex *)g_fftBuf;
  shiftRightFftBufs(cbuf);
  computeAmplitudeAndFrequency();
  Vector3D newColor = getFreqColor();
  g_color.set(newColor.x, newColor.y, newColor.z);

  switch (g_displayMode) {
    case WATER_FALL:
      if (isBothEnabled) drawParticlesMode();
      drawWaterFallMode();
      break;
    case PARTICLES:
      drawParticlesMode();
      if (isBothEnabled) drawWaterFallMode();
      break;
    case WOBBLE:
      drawWobbleMode();
      break;
  }

  // flush!
  glFlush( );
  // swap the double buffer
  glutSwapBuffers( );
  g_t += 1;
}
