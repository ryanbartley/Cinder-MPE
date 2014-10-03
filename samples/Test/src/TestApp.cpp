#include "cinder/app/AppNative.h"
#include "cinder/app/RendererGl.h"
#include "cinder/gl/gl.h"

#include "Client.h"
#include "ServerBase.hpp"

using namespace ci;
using namespace ci::app;
using namespace std;

class TestApp : public AppNative {
  public:
	void setup() override;
	void mouseDown( MouseEvent event ) override;
	void keyDown( KeyEvent event ) override;
	void update() override;
	void draw() override;
	
	void updateFrame( uint64_t frameNum );
	void dataMessage( const std::string &message, const uint32_t id );
	
	mpe::ClientRef	mMpeClient;
	std::shared_ptr<boost::asio::io_service> mIoService;
	std::shared_ptr<boost::asio::io_service::work> mWork;
	std::thread		mThread;
};

void TestApp::setup()
{
	mIoService.reset( new boost::asio::io_service );
	mWork.reset( new boost::asio::io_service::work( *mIoService ) );
	mThread = std::thread( boost::bind( &boost::asio::io_service::run, mIoService ) );
	
	auto asset = "settings.0.json";
	mMpeClient = mpe::Client::create( loadAsset( asset ), *mIoService, true );
	mMpeClient->setUpdateFrameCallback( std::bind( &TestApp::updateFrame, this, std::placeholders::_1 ) );
	mMpeClient->setDataMessageCallback( std::bind( &TestApp::dataMessage, this, std::placeholders::_1, std::placeholders::_2 ) );
}

void TestApp::mouseDown( MouseEvent event )
{
	mMpeClient->sendMessage( "Hello" );
}

void TestApp::keyDown( cinder::app::KeyEvent event )
{
	if( event.getCode() == KeyEvent::KEY_SPACE ) {
		mWork.reset();
		mIoService->stop();
		mThread.join();
	}
}

void TestApp::updateFrame( uint64_t frameNum )
{
	cout << "Frame Number: " << frameNum << endl;
}

void TestApp::dataMessage( const std::string &message, const uint32_t id )
{
//	CI_LOG_V("Received data message: " + message);
}

void TestApp::update()
{

}

void TestApp::draw()
{
	gl::clear( Color( 0, 0, 0 ) );
	
	mMpeClient->doneRendering();
}

CINDER_APP_NATIVE( TestApp, RendererGl )
