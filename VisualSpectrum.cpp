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
void reshapeFunc( GLsizei width, GLsizei height );
void keyboardFunc( unsigned char, int, int );
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
#define HISTORY_SIZE 100
#define WALL_LINES_SIZE 200

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

// global variables
GLboolean g_fullscreen = FALSE;
float g_maxAmp = -1;
float g_maxAmp_old = -1;
int g_maxAmpIndex = -1;
int g_maxAmpIndex_old = -1;

Vector3D* g_wall_lines;

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
    memmove(&g_fftBufs[i][0], &g_fftBufs[i-1][0], g_bufferSize * sizeof(float));
  }
  for (int i = 0; i < g_bufferSize; i++) {
    g_fftBufs[0][i] = cmp_abs(current[i]);
  }
}

void checkMaxAmplitudeIndex() {
  g_maxAmp_old = g_maxAmp;
  g_maxAmpIndex_old = g_maxAmpIndex;

  SAMPLE maxAmp = -1;
  int maxIndex = -1;
  for (int i = 0; i < g_bufferSize; i++) {
    if (g_fftBufs[0][i] > maxAmp) {
      maxAmp = g_fftBufs[0][i];
      maxIndex = i;
    }
  }
  
  g_maxAmp = maxAmp;
  g_maxAmpIndex = maxIndex;
}

bool isFrequencyChanged() {
  return (fabs(g_maxAmpIndex - g_maxAmpIndex_old) > PITCH_CHANGE_THRESHOLD); 
}

bool isAmplitudeChanged() {
  return (fabs(g_maxAmp - g_maxAmpIndex_old > AMPLITUDE_CHANGE_THRESHOLD));
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
    
    
    g_wall_lines = new Vector3D[WALL_LINES_SIZE];
    for (int i = 0; i < WALL_LINES_SIZE; i++) {
      if (i > WALL_LINES_SIZE/2) {
        if (i % 2) {
          g_wall_lines[i].set(5, XFun::rand2f(-10, 10), 10);
        } else {
          g_wall_lines[i].set(5, g_wall_lines[i-1].y, 10);
        }
      } else {
        if (i % 2) {
          g_wall_lines[i].set(-5, XFun::rand2f(-10, 10), 10);
        } else {
          g_wall_lines[i].set(-5, g_wall_lines[i-1].y, 9);
        }
      }
    }


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
    // set the mouse function - called on mouse stuff
    glutMouseFunc( mouseFunc );
    

    // set clear color
    glClearColor( 0, 0, 0, 1 );

    // enable color material
    glEnable( GL_COLOR_MATERIAL );
    // enable depth test
    glEnable( GL_DEPTH_TEST );
    glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_BLEND);
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
    gluLookAt( 0.0f, 5.0f, 10.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f );
}




//-----------------------------------------------------------------------------
// Name: help( )
// Desc: print usage
//-----------------------------------------------------------------------------
void help()
{
    cerr << "----------------------------------------------------" << endl;
    cerr << "NAME-O-PROGRAM (v1.0)" << endl;
    cerr << "AUTHOR" << endl;
    cerr << "http://website/" << endl;
    cerr << "----------------------------------------------------" << endl;
    cerr << "'h' - print this help message" << endl;
    cerr << "'s' - toggle fullscreen" << endl;
    cerr << "'q' - quit visualization" << endl;
    cerr << "----------------------------------------------------" << endl;
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



GLfloat r = 0;
Vector3D wall_color, waterfall_color;
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
    
    // line width
    glLineWidth( 1.0 );
    // define a starting point
    GLfloat x = -5;
    // compute increment
    GLfloat xinc = ::fabs(x*2 / g_bufferSize);
    
    // color
    glColor3f( .5, .5, 1 );
    
    // save transformation state
    glPushMatrix();
        // translate
        glTranslatef( 0, 2, 0 );
        // start primitive
        glBegin( GL_LINE_STRIP );
            // loop over buffer
            for( int i = 0; i < g_bufferSize; i++ )
            {
                // plot
                glVertex2f( x, 3*g_buffer[i] );
                // increment x
                x += xinc;
            }
        // end primitive
        glEnd();
    // pop
    glPopMatrix();
    
    // copy into the fft buf
    memcpy( g_fftBuf, g_buffer, sizeof(SAMPLE)*g_bufferSize );
    // apply window to buf
    apply_window( g_fftBuf, g_window, g_windowSize );
    // take forward FFT (time domain signal -> frequency domain signal)
    rfft( g_fftBuf, g_windowSize/2, FFT_FORWARD );
    // cast the result to a buffer of complex values (re,im)
    complex * cbuf = (complex *)g_fftBuf;
    
    
    // define a starting point
    // compute increment
    xinc = ::fabs(x*2 / (g_windowSize / 2));

    // color
    glLineWidth(2);


    if (XFun::rand2f(0,10) > 9) {
      wall_color.set(XFun::rand2f(0.1,0.9),XFun::rand2f(0.1,0.9),XFun::rand2f(0.1,0.9));
    }
    glColor4f(wall_color.x, wall_color.y, wall_color.z, 1);

    glPushMatrix();
      for (int i = 0; i < WALL_LINES_SIZE; i+=2) {
        glBegin(GL_LINES);
        //g_wall_lines[i+1].z = XFun::rand2f(1,10);
        g_wall_lines[i+1].z -= 5*XFun::rand2f(0.1,1);
        Vector3D p1 = g_wall_lines[i];
        Vector3D p2 = g_wall_lines[i+1];
        glVertex3f(p1.x, p1.y, p1.z);
        glVertex3f(p2.x, p2.y, p2.z);
        glEnd();

        if (p2.z < -500) g_wall_lines[i+1].z = 9;
        
      }
    glPopMatrix();
   /* 
    glPushMatrix();
    glTranslatef(-5, 13, 0);
    glScalef(0.01, 30, 1000);
    glutSolidCube(1);
    glPopMatrix();

    glPushMatrix();
    glTranslatef(5, 13, 0);
    glScalef(0.01, 30, 1000);
    glutSolidCube(1);
    glPopMatrix();
*/
    for (int i = 0; i < 500; i++) {
      
    }
    if (!(g_t % 100)) { 

      waterfall_color.set(
        XFun::rand2f(0.5,1),
        XFun::rand2f(0.5,1),
        XFun::rand2f(0.5,1)
      );
    }
    for (int i = HISTORY_SIZE-1; i >= 0; i--) {
      glColor4f(waterfall_color.x, waterfall_color.y, waterfall_color.z, 0.7);

      x = -5;

      // save transformation state
      glPushMatrix();
          // translate
          glTranslatef( 0, -2, 0 );
          // start primitive
              // loop over buffer to draw spectrum
              for( int j = 0; j < g_windowSize/2; j++ )
              {
                  x += xinc;
                  glPushMatrix();
                  glBegin(GL_LINES);
                  // plot the magnitude,
                  // with scaling, and also "compression" via pow(...)
                  glVertex3f( x, 0, 3-i);
                  glVertex3f( x, 50*pow( g_fftBufs[i][j], 0.5 ), 3-i);
                  glEnd();
                  // increment x
                  glPopMatrix();
              }

          // end primitive
      // restore transformations
      glPopMatrix();
    }
    shiftRightFftBufs(cbuf);
    glDepthMask(GL_TRUE);
    
    // flush!
    glFlush( );
    // swap the double buffer
    glutSwapBuffers( );
    g_t += 1;
}
