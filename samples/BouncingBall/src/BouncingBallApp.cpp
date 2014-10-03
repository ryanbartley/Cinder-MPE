#include "cinder/app/AppNative.h"
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

class BouncingBallApp : public AppNative {
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
	
	std::shared_ptr<boost::asio::io_service> mIoService;
	std::shared_ptr<boost::asio::io_service::work> mWork;
	std::thread		mThread;
};

void BouncingBallApp::setup()
{
	randSeed( 5 );
	
	mIoService.reset( new boost::asio::io_service );
	mWork.reset( new boost::asio::io_service::work( *mIoService ) );
	mThread = std::thread( boost::bind( &boost::asio::io_service::run, mIoService ) );
	
	// Initialize and setup the MPE Client
	mMpeClient = mpe::Client::create( loadAsset( "settings."+ to_string( CLIENT_ID ) +".json" ), *mIoService );
	// Pass the function callbacks to MPE Client
	mMpeClient->setDataMessageCallback( &BouncingBallApp::dataMessage, this );
	mMpeClient->setUpdateFrameCallback( &BouncingBallApp::updateFrame, this );
	mMpeClient->setResetCallback( &BouncingBallApp::reset, this );
	
	// Initialize some gl specific stuff
	gl::viewport( mMpeClient->getGlWindowInfo() );
	gl::scissor( mMpeClient->getGlWindowInfo() );

	setupGl();
	
	mSpheres.push_back( ::Sphere( randVec2f() * vec2( mMpeClient->getMasterSize() ),
								 randVec2f() * 10.0f, mMpeClient->getMasterSize(), mSphereBatch ) );
	
	// Initialize the resettable member functions using the reset function.
	reset();
	
	gl::enableDepthRead();
	gl::enableDepthWrite();
}

void BouncingBallApp::setupGl()
{
	mGlsl = gl::GlslProg::create( gl::GlslProg::Format()
								 .vertex( loadAsset( "Phong.vert" ) )
								 .fragment( loadAsset( "Phong.frag" ) ) );
	// Create our sphere that we can draw multiple times
	mSphereBatch = gl::Batch::create( geom::Sphere().radius( kDefaultRadius ).enable( geom::Attrib::NORMAL ), mGlsl );
}

void BouncingBallApp::reset()
{
	mCam.setPerspective( 60.0f, getWindowAspectRatio(), .01f, 1000 );
	mCam.lookAt( vec3( 0, 0, 100 ), vec3( 0 ) );
}

void BouncingBallApp::mouseDown( MouseEvent event )
{
}

void BouncingBallApp::dataMessage( const std::string &message, const uint32_t id )
{
	auto parts = ci::split( message, "|" );
	if( parts[0] == "Sph" ) {
		auto position = ci::split( parts[1], "," );
		mSpheres.push_back( vec2(position[0], position[1] ) );
	}
	else if( parts[0] == "Cub" ) {
		auto position = ci::split( parts[1], "," );
		mSpheres.push_back( vec2(position[0], position[1] ) );
	}
	
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
//	auto visRect = mMpeClient->getVisibleRect();
//	gl::viewport( vec2( visRect.x1, visRect.y1 ), vec2( visRect.x2, visRect.y2 ) );
//	gl::setMatrices( mCam );
	gl::setMatricesWindowPersp( getWindowSize() );
	
	for( auto & sphere : mSpheres ) {
		sphere.draw();
	}
	mMpeClient->doneRendering();
	cout << getAverageFps() << endl;
}

CINDER_APP_NATIVE( BouncingBallApp, RendererGl )
