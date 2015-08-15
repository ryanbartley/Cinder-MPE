#include "cinder/app/App.h"
#include "cinder/app/RendererGl.h"
#include "cinder/gl/gl.h"
#include "cinder/gl/GlslProg.h"

#include "cinder/Rand.h"
#include "cinder/Rect.h"

#include "Client.h"

#include "Ball.h"

using namespace ci;
using namespace ci::app;
using namespace std;

class BouncingBallApp : public App {
public:
	void setup() override;
	void mouseDown( MouseEvent event ) override;
	void draw() override;
	void update() override {}
	// Notice that we've gotten rid of update. Any of our
	// update features should be set in updateFrame, because
	// it will be called whenever the server has told us
	// to update the frame. Instead we'll create a function
	// called updateFrame.
	void updateFrame( uint64_t frameNum );
	
	void setupGl();
	
	void reset();
	void dataMessage( const std::string &message, const uint32_t id );
	
	mpe::ClientRef			mMpeClient;
	std::vector<::Sphere>	mSpheres;
	Rand					mRand;
	gl::BatchRef			mSphereBatch;
	gl::GlslProgRef			mGlsl;
	CameraPersp				mCam;
};

void BouncingBallApp::setup()
{
	randSeed( 5 );
	
	// Construct the name of the settings file, CLIENT_ID is a preprocessor variable
	// that changes for each build.
	auto settingsFile = "settings." + to_string( CLIENT_ID ) + ".json";
	// Initialize and setup the MPE Client
	mMpeClient = mpe::Client::create( loadAsset( settingsFile ) );
	// Pass the function callbacks to MPE Client
	mMpeClient->setDataMessageCallback( &BouncingBallApp::dataMessage, this );
	mMpeClient->setUpdateFrameCallback( &BouncingBallApp::updateFrame, this );
	mMpeClient->setResetCallback( &BouncingBallApp::reset, this );
	
	// Initialize some gl specific stuff
	gl::viewport( mMpeClient->getGlWindowInfo() );
	gl::scissor( mMpeClient->getGlWindowInfo() );

	setupGl();
	
	// Initialize the resettable member functions using the reset function.
	reset();
	
	gl::enableDepthRead();
	gl::enableDepthWrite();
}

void BouncingBallApp::setupGl()
{
	// Create our sphere that we can draw multiple times
	mSphereBatch = gl::Batch::create( geom::Sphere().radius( kDefaultRadius ), gl::getStockShader( gl::ShaderDef().lambert() ) );
}

void BouncingBallApp::reset()
{
	mSpheres.clear();
	
	mSpheres.push_back( ::Sphere( vec2( mMpeClient->getMasterSize() ) / 2.0f,
								 vec2(10.0f, 10.0f), mMpeClient->getMasterSize(), mSphereBatch ) );
}

void BouncingBallApp::mouseDown( MouseEvent event )
{
	
}

void BouncingBallApp::dataMessage( const std::string &message, const uint32_t id )
{
	
}

void BouncingBallApp::updateFrame( uint64_t frameNum )
{
	for( auto & sphere : mSpheres ) {
		sphere.update();
	}
}

void BouncingBallApp::draw()
{
	gl::clear( Color( 0, 0, 0 ) );
	
	gl::setMatricesWindowPersp( getWindowSize() );
	auto localOrigin = vec2( mMpeClient->getVisibleRect().x1, mMpeClient->getVisibleRect().y1 );
	gl::translate( vec3( localOrigin.x * -1.0f, localOrigin.y * -1.0f, 0.0f ) );
	
	for( auto & sphere : mSpheres ) {
		sphere.draw();
	}
	
	mMpeClient->doneRendering();
}

CINDER_APP( BouncingBallApp, RendererGl )
