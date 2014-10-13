#pragma once

//---------------------------------------------------------------
//Example of using OpenCL for creating particle system,
//which morphs between two 3D shapes - cube and face image
//
//Control keys: 1 - morph to cube, 2 - morph to face
//
//All drawn particles have equal brightness, so to achieve face-like
//particles configuration by placing different number of particles
//at each pixel and draw them in "addition blending" mode.
//
//Project is developed for openFrameworks 8.4_osx and is based
//on example-Particles example of ofxMSAOpenCL adoon.
//It uses addons ofxMSAOpenCL and ofxMSAPingPong.
//For simplicity this addons are placed right in the project's folder.
//
//The code and "ksenia.jpg" photo made by Kuflex.com, 2014:
//Denis Perevalov, Igor Sodazot and Ksenia Lyashenko.
//---------------------------------------------------------------


#include "ofMain.h"

class ofApp : public ofBaseApp{
    
public:
    void setup();
    void update();
    void draw();
    
    void morphToCube( bool setPos );       //Morphing to cube
    void morphToFace();                    //Morphing to face
    
    ofEasyCam cam; // add mouse controls for camera movement

    
    void keyPressed(int key);
    void keyReleased(int key);
    void mouseMoved(int x, int y );
    void mouseDragged(int x, int y, int button);
    void mousePressed(int x, int y, int button);
    void mouseReleased(int x, int y, int button);
    void windowResized(int w, int h);
    void dragEvent(ofDragInfo dragInfo);
    void gotMessage(ofMessage msg);
    
};
