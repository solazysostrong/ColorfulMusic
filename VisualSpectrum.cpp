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

// global variables
GLboolean g_fullscreen = FALSE;




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
    glutInitDisplayMode( GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH );
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
    gluLookAt( 0.0f, 0.0f, 10.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f );
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
    x = -5;
    // compute increment
    xinc = ::fabs(x*2 / (g_windowSize / 2));

    // color
    glColor3f( .5, 1, .5 );
    
    // save transformation state
    glPushMatrix();
        // translate
        glTranslatef( 0, -2, 0 );
        // start primitive
        glBegin( GL_LINE_STRIP );
            // loop over buffer to draw spectrum
            for( int i = 0; i < g_windowSize/2; i++ )
            {
                // plot the magnitude,
                // with scaling, and also "compression" via pow(...)
                glVertex2f( x, 10*pow( cmp_abs(cbuf[i]), .5 ) );
                // increment x
                x += xinc;
            }
        // end primitive
        glEnd();
    // restore transformations
    glPopMatrix();
    
    // flush!
    glFlush( );
    // swap the double buffer
    glutSwapBuffers( );
}
