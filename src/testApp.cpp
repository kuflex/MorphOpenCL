
#include "testApp.h"
#include "MSAOpenCL.h"
#include "ofxIniSettings.h"
#include "ofxSyphon.h"


int NUM_PARTICLES = 1000000; //Считывается из ini

struct Params {
    int w, h, fps;
    int n;
    
    float startArea;
    float size;
    float alpha;
    
    float mass0, mass1;
    int aN;
    float aMass0, aMass1;
    float aG0, aG1;
    float attrDisableRad;
    
    void load() {
        ofxIniSettings ini;
        ini.load("settings.ini");
        string scr = "Screen.";
        string part = "Particles.";
        
        w       = ini.get( scr + "w", 1280 );
        h       = ini.get( scr + "h", 720 );
        fps     = ini.get( scr + "fps", 60 );
        n       = ini.get( part + "n", 3000000 );
        startArea = ini.get( part + "startArea", 1.0f );
        size    = ini.get( part + "size", 2.0f );
        alpha   = ini.get( part + "alpha", 16.0f );
        mass0   = ini.get( part + "mass0", 10.0f );
        mass1   = ini.get( part + "mass1", 10.0f );
        aN      = ini.get( part + "aN", 6 );
        aMass0  = ini.get( part + "aMass0", 40.0f );
        aMass1  = ini.get( part + "aMass1", 40.0f );
        aG0     = ini.get( part + "aG0", 4.0f );
        aG1     = ini.get( part + "aG1", 4.0f );
        attrDisableRad = ini.get( part + "attrDisableRad", 50.0f );
    }

};

Params param;



//Выравнивание типов OpenCL - все типы в памяти в массиве должны идти выровненно по своему размеру.
//Например, float3 - он как float4 на GPU - только через 4*4 байта. Поэтому, надо "dummy" поля делать.

typedef struct{
	float4 vel;
	float4 target;
	float mass;
	float dummy1;
	float dummy2;
	float dummy3;
} Particle;


typedef struct {
    float4 pos;
    float mass;
    float G;
    float dummy1;
	float dummy2;
} Attractor;


float2				mousePos;
float2				dim;

msa::OpenCL			opencl;

msa::OpenCLBufferManagedT<Particle>	particles; // vector of Particles on host and corresponding clBuffer on device
msa::OpenCLBufferManagedT<float4> particlePos; // vector of particle positions on host and corresponding clBuffer on device
msa::OpenCLBufferManagedT<Attractor> attractors; // vector of particle positions on host and corresponding clBuffer on device
int attrN = 6;
//1;

GLuint vbo;

ofFbo fbo;
ofxSyphonServer syphon;

//--------------------------------------------------------------
int tw, th, tN;
vector<int> tPnt;


void prepareTargets()
{
    ofPixels pix;
    ofLoadImage(pix, "ksenia.jpg");
    //строим распределение
    int w = pix.getWidth();
    int h = pix.getHeight();


    //распределение
    int sum = 0;
    for (int y=0; y<h; y++) {
        for (int x=0; x<w; x++) {
            sum += pix.getColor(x, y).getBrightness();
        }
    }
    int N = sum;
    tPnt.resize( N );
    
    int c = 0;
    for (int y=0; y<h; y++) {
        for (int x=0; x<w; x++) {
            int v = pix.getColor(x, y).getBrightness();
            for (int i=0; i<v; i++) {
                tPnt[c++] = x + w * y;
            }
        }
    }
    tw = w;
    th = h;
    tN = N;
}


//--------------------------------------------------------------
void testApp::setup(){
    
    //Параметры
    param.load();
    
    //Экран
    ofSetWindowTitle("Gravity");
    ofSetWindowShape( param.w, param.h );
    ofSetFrameRate( param.fps );
	ofSetVerticalSync(false);
    
    ofBackground(0, 0, 0);
	ofSetLogLevel(OF_LOG_VERBOSE);
    
    //Камера
	cam.setDistance(600);
    
    //Целевые точки
    prepareTargets();

    //Частицы

    dim.x = ofGetWidth();
	dim.y = ofGetHeight();
    NUM_PARTICLES = param.n;
    attrN = param.aN;

    fbo.allocate( dim.x, dim.y );
    syphon.setName("Screen");

    

	opencl.setupFromOpenGL();
    
    // create vbo
    glGenBuffersARB(1, &vbo);
	glBindBufferARB(GL_ARRAY_BUFFER_ARB, vbo);
	glBufferDataARB(GL_ARRAY_BUFFER_ARB, sizeof(float4) * NUM_PARTICLES, 0, GL_DYNAMIC_COPY_ARB);
	glBindBufferARB(GL_ARRAY_BUFFER_ARB, 0);
    
    // init host and CL buffers
    particles.initBuffer( NUM_PARTICLES );
    particlePos.initFromGLObject( vbo, NUM_PARTICLES );
    attractors.initBuffer( attrN );

    toSphere( true );
    restartAttractors();
    
	
	opencl.loadProgramFromFile("Particle.cl");
	opencl.loadKernel("updateParticle");

	opencl.kernel("updateParticle")->setArg(0, particles.getCLMem());
	opencl.kernel("updateParticle")->setArg(1, particlePos.getCLMem());
	opencl.kernel("updateParticle")->setArg(2, mousePos);
	opencl.kernel("updateParticle")->setArg(3, dim);
	opencl.kernel("updateParticle")->setArg(4, attractors.getCLMem());
	opencl.kernel("updateParticle")->setArg(5, attrN);
	opencl.kernel("updateParticle")->setArg(6, param.attrDisableRad);
    
	
}

//--------------------------------------------------------------
//огрубление с шагом size
float coarse( float x, float size ) {
    return int( x / size + 0.5 ) * size;
}

//--------------------------------------------------------------
void testApp::toSphere( bool setPos )
{
    for(int i=0; i<NUM_PARTICLES; i++) {
		Particle &p = particles[i];
		p.vel.set(0, 0, 0, 0);
		p.mass = ofRandom( param.mass0, param.mass1 );
        float w = param.startArea * dim.x;
        float h = param.startArea * dim.y;
		
        float a = ofRandom( 0, M_TWO_PI );
        float b = ofRandom( 0, M_TWO_PI );
        float x = cos(a) * cos(b);
        float y = sin(a) * cos(b);
        float z = sin(b);
        ofPoint pnt( x, y, z );
        
        
        pnt *= 100;
        pnt.y += -100;
        pnt.x = coarse( pnt.x, 5 ) + ofRandom( -0.5, 0.5 );
        pnt.y = coarse( pnt.y, 10 )  + ofRandom( -0.5, 0.5 );
        pnt.z = coarse( pnt.z, 10 ) + ofRandom( -0.5, 0.5 );
        
        
        if ( setPos ) {
            particlePos[i].set( pnt.x, pnt.y, pnt.z, 0 );
        }
        
        p.target.set( pnt.x, pnt.y, pnt.z, 0 );
        
        //particlePos[i].set( ofRandom( -w/2, w/2 ), ofRandom( -h/2, h/2 ), ofRandom( -0, 0), 0 );
	}

    particles.writeToDevice();
    if ( setPos ) {
        particlePos.writeToDevice();
    }
}

//--------------------------------------------------------------
void testApp::toFace()
{
    
    //установка в частицы
    for(int i=0; i<NUM_PARTICLES; i++) {
		Particle &p = particles[i];
		p.vel.set(0, 2, 0, 0);    //увеличиваем скорость
        
        
        //целевая точка
        int q = ofRandom( 0, tN );
        q = min( q, tN-1 );
        int ind = tPnt[ q ];
        float x = (ind % tw) - tw/2;
        float y = (ind / tw) - th/2;
        
        float scl = 2;
        x *= scl;
        y *= -scl;
        
        x += ofRandom( -scl/2, scl/2 );
        y += ofRandom( -scl/2, scl/2 );
        
        //x = coarse( x, 3 );
        //y = coarse( y, 3 );
        
        
        float noisex = 2.5;
        float noisey = 0.5; //1.0; //1.5;
        float noisez = 5.0;//1.0;
        x += ofRandom( -noisex, noisex );
        y += ofRandom( -noisey, noisey );
        
        //на цилиндр
        float Rad = tw * scl * 0.4;
        float z = sqrt( fabs( Rad * Rad - x * x ) ) - Rad;
        
        z += ofRandom( -noisez, noisez );
        
        
        p.target.set( x, y, z, 0 );
        //p.target.set( 0, 100, 0, 0 );
        
        
    }
    
    
    particles.writeToDevice();
}

//--------------------------------------------------------------
void testApp::restartAttractors()
{
	for(int i=0; i<attrN; i++) {
        Attractor &a = attractors[i];
        a.pos.set( ofRandom( -dim.x/2, dim.x/2 ), ofRandom( -dim.y/2, dim.y/2 ), ofRandom( -200, 200 ), 0 );
        a.mass = ofRandom( param.aMass0, param.aMass1 );
        a.G = ofRandom( param.aG0, param.aG1 ); //4; //0.05;
    }
    attractors.writeToDevice();
}

//--------------------------------------------------------------
void testApp::update(){
	mousePos.x = ofGetMouseX();
	mousePos.y = ofGetMouseY();
	
	opencl.kernel("updateParticle")->setArg(2, mousePos);
	opencl.kernel("updateParticle")->setArg(3, dim);
	opencl.kernel("updateParticle")->run1D(NUM_PARTICLES);

    
	opencl.finish();
    
    //Рисование частиц
    fbo.begin();
    
    cam.begin();
    ofBackground( 0 );
    
    ofSetColor( param.alpha );
    ofEnableBlendMode( OF_BLENDMODE_ADD );
    glPointSize( param.size );
    
    
	glBindBufferARB(GL_ARRAY_BUFFER_ARB, vbo);
	glEnableClientState(GL_VERTEX_ARRAY);
	glVertexPointer(3, GL_FLOAT, sizeof( float4 ), 0);
	glDrawArrays(GL_POINTS, 0, NUM_PARTICLES);
	glBindBufferARB(GL_ARRAY_BUFFER_ARB, 0);
	
    cam.end();
    fbo.end();
    
    syphon.publishTexture( &fbo.getTextureReference() );

}

//--------------------------------------------------------------
void testApp::draw(){
    
    
    ofEnableAlphaBlending();
    ofBackground( 0 );
    ofSetColor( 255 );
    fbo.draw(0, 0);
    
    //attractors
/*    cam.begin();
    ofSetColor( 255, 0, 0 );
    ofFill();
    for (int i=0; i<attrN; i++) {
        ofCircle( attractors[i].pos, attractors[i].mass / 5 );
    }
    cam.end();*/
	
	glColor3f(1, 1, 1);
	string info = "a-attractors, Space-particles, Enter-screenshot, mouse-move attr. Syphon enabled. Particles: " + ofToString(NUM_PARTICLES) + " FPS: " + ofToString(ofGetFrameRate());
	ofDrawBitmapString(info, 20, 20);
}


//--------------------------------------------------------------
void testApp::keyPressed(int key)
{
    if ( key == ' ' ) {
        toSphere( false );
    }
    if ( key == 'a' ) {
        toFace();
    }
//    if ( key == 'a' ) {
//        restartAttractors();
//    }
    if ( key == OF_KEY_RETURN ) {
        ofPixels pixels;
        fbo.readToPixels( pixels );
        
        int num = ofRandom(0, 1000);
        string file = ofToString( num );
        while (file.length() < 5 ) { file = "0" + file; }
        
        ofSaveImage( pixels, "screenshots/" + file + ".png" );
    }
}

//--------------------------------------------------------------
void testApp::mouseMoved(int x, int y ){
    
}

//--------------------------------------------------------------
bool drag = false;
int dragId = -1;
ofPoint dragP;

void testApp::mouseDragged(int x, int y, int button){
    if( drag ) {
        ofPoint p( x, y );
        attractors[dragId].pos = dragP + p;
        attractors.writeToDevice();
    }
}

//--------------------------------------------------------------
void testApp::mousePressed(int x, int y, int button){
    ofPoint p( x, y );
    for ( int i=0; i<attrN; i++ ) {
        if ( p.distance( attractors[i].pos ) < 10 ) {
            drag = true;
            dragId = i;
            dragP = attractors[i].pos - p;
        }
    }
}

//--------------------------------------------------------------
void testApp::mouseReleased(int x, int y, int button){
	drag = false;
    mouseDragged(x, y, button);
}

//--------------------------------------------------------------

