This is an implementation of Most-Pixels-Ever(original source by [Bill Lindmeier](https://github.com/wdlindmeier/Most-Pixels-Ever-Cinder))for Cinder. This is a test for the glNext branch. Please use the above link for a release version. Take a look at what Most-Pixels-Ever is [here](https://github.com/shiffman/Most-Pixels-Ever-Processing).

This version of Most-Pixels-Ever is built for glNext branch of the [Cinder Repo](https://github.com/cinder/Cinder/tree/glNext). To get this version follow these instructions...

- Go to a folder that you'd like to save cinder and open it in terminal
- Type `git clone https://github.com/cinder/Cinder.git`
- That will clone Cinder into `/path/you/saved/to/Cinder`
- Type `cd Cinder` and you'll be in the root of Cinder.
- Type `git checkout glNext` and it will change your branch to glNext.
- Now you need to pull down the submodules. Type `git submodule init` and `git submodule update`. These commands will pull down boost and tinderbox.
- Next type `open xcode/cinder.xcodeproj`, this will open Xcode and just press play. This will build Cinder.
- Go to the `samples/_opengl/` and open any of the xcode projects and if pressing play runs your app, you're done with the Cinder prep.

Now we'll clone this repo...

- go to your blocks folder from the Cinder root and type `git clone https://github.com/ryanbartley/Cinder-MPE.git`
- This will clone Cinder-MPE into your blocks folder. 
- Now you can check out the samples and you can make new Cinder-MPE projects with tinderbox.

Cinder-MPE also uses Cinder-Asio [here](https://github.com/BanTheRewind/Cinder-Asio). You'll need to clone Cinder-Asio in your blocks as well as it is a dependency of this block. You can simply do that the way that we did it above by cloning it into your Cinder/blocks/ folder.
