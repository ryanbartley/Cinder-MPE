//
//  Ball.h
//  BouncingBall
//
//  Created by Ryan Bartley on 9/15/14.
//
//

#pragma once

#include "cinder/gl/Batch.h"

const static uint32_t kDefaultRadius = 30;

class Sphere {
public:
	
	Sphere( const ci::vec2 &position, const ci::vec2 &velocity, const ci::vec2 &size, const ci::gl::BatchRef &batch )
	: mPosition( ci::math<float>::clamp( position.x, kDefaultRadius, size.x - kDefaultRadius),
				ci::math<float>::clamp( position.y, kDefaultRadius, size.y - kDefaultRadius) ),
	mVelocity( velocity ), mClientSize( size ), mSphere( batch ) {}
	~Sphere() {}
	
	inline void update()
	{
		float radius = mDiameter * 0.5;
		
		if ( mPosition.x < radius || mPosition.x >
			( mClientSize.x - radius ) ) {
			std::cout << "Resetting x" << std::endl;
			mVelocity.x = mVelocity.x * -1;
		}
		if ( mPosition.y < radius || mPosition.y >
			( mClientSize.y - radius ) ) {
			std::cout << "Resetting y" << std::endl;
			mVelocity.y = mVelocity.y * -1;
		}
		
		mPosition += mVelocity;
	}
	
	inline void draw()
	{
		using namespace ci;
		
		gl::pushModelMatrix();
		gl::translate( mPosition );
		mSphere->draw();
		gl::popModelMatrix();
	}
	
private:
	ci::vec2			mPosition;
	ci::vec2			mVelocity;
	ci::ivec2			mClientSize;
	float				mDiameter;
	ci::gl::BatchRef	mSphere;
};
