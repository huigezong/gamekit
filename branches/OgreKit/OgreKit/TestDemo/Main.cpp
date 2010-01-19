/*
-------------------------------------------------------------------------------
	This file is part of OgreKit.
	http://gamekit.googlecode.com/

	Copyright (c) 2009 Charlie C.
-------------------------------------------------------------------------------
 This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely, subject to the following restrictions:

  1. The origin of this software must not be misrepresented; you must not
	 claim that you wrote the original software. If you use this software
	 in a product, an acknowledgment in the product documentation would be
	 appreciated but is not required.
  2. Altered source versions must be plainly marked as such, and must not be
	 misrepresented as being the original software.
  3. This notice may not be removed or altered from any source distribution.
-------------------------------------------------------------------------------
*/
#include "OgreKitApplication.h"
#include "OgreBlend.h"
#include "OgreException.h"
#include "OgreColourValue.h"
#include "OgreSceneManager.h"
#include "OgreRenderWindow.h"
#include "OgreViewport.h"
#include "OgreMathUtils.h"
#include "autogenerated/blender.h"

#include "btBulletDynamicsCommon.h"

// ----------------------------------------------------------------------------
class TestApp : public OgreKitApplication, public OgreBlend
{
protected:

	Ogre::SceneNode *m_rollNode, *m_pitchNode, *m_camNode;
	Ogre::Viewport *m_viewport;

	Ogre::String m_blend;

	void setupFreeLook(void);



public:
	TestApp(const Ogre::String& blend);
    virtual ~TestApp();

    void createScene(void);
	void update(Ogre::Real tick);
    void endFrame(void);
};

// ----------------------------------------------------------------------------
TestApp::TestApp(const Ogre::String& blend) :
	m_rollNode(0), m_pitchNode(0), m_camNode(0),
	m_viewport(0), m_blend(blend)
{
}

// ----------------------------------------------------------------------------
TestApp::~TestApp()
{
}

// ----------------------------------------------------------------------------
void TestApp::createScene(void)
{
	read(m_blend);
	convertAllObjects();

	if (!m_camera)
		m_camera = m_manager->createCamera("NoCamera");

	m_viewport = m_window->addViewport(m_camera);
	if (m_blenScene->world)
	{
		Blender::World *wo = m_blenScene->world;
		m_viewport->setBackgroundColour(Ogre::ColourValue(wo->horr, wo->horg, wo->horb));
	}

	m_camera->setAspectRatio((Ogre::Real)m_viewport->getActualWidth() / (Ogre::Real)m_viewport->getActualHeight());
	setupFreeLook();

}

// ----------------------------------------------------------------------------
void TestApp::setupFreeLook(void)
{
	m_rollNode  = m_manager->getRootSceneNode()->createChildSceneNode();
	m_pitchNode = m_rollNode->createChildSceneNode();
	m_camNode = m_pitchNode->createChildSceneNode();

	if (m_camera->getParentSceneNode())
	{
		Ogre::SceneNode *cpn = m_camera->getParentSceneNode();
		cpn->detachObject(m_camera);
		m_camNode->attachObject(m_camera);

		Ogre::Vector3 neul = Ogre::MathUtils::getEulerFromQuat(cpn->_getDerivedOrientation());
		Ogre::Vector3 zeul = Ogre::Vector3(0, 0, neul.z);

		Ogre::Quaternion zrot = Ogre::MathUtils::getQuatFromEuler(zeul);
		zrot.normalise();
		m_rollNode->setOrientation(zrot);
		m_rollNode->setPosition(cpn->_getDerivedPosition());

		Ogre::Quaternion crot = Ogre::MathUtils::getQuatFromEuler(Ogre::Vector3(90, 0, 0));
		crot.normalise();
		m_camNode->setOrientation(crot);
	}
	else
	{
		m_rollNode->setPosition(Ogre::Vector3(0, -10, 0));
		m_camNode->setOrientation(Ogre::MathUtils::getQuatFromEuler(Ogre::Vector3(90, 0, 0)));
		m_camNode->attachObject(m_camera);
	}

}

// ----------------------------------------------------------------------------
void TestApp::update(Ogre::Real tick)
{
	if (this->m_destinationWorld)
		m_destinationWorld->stepSimulation(tick);



	if (m_mouse.moved)
	{
		m_rollNode->roll(Ogre::Radian(  (-m_mouse.relitave.x * tick) / 3.f));
		m_pitchNode->pitch(Ogre::Radian((-m_mouse.relitave.y * tick) / 3.f));

		Ogre::Vector3 p90 = Ogre::MathUtils::getEulerFromQuat(m_pitchNode->getOrientation());
		bool clamp = false;
		if (p90.x > 90)
		{
			clamp = true;
			p90.x = 90;
		}

		if (p90.x < -90)
		{
			clamp = true;
			p90.x = -90;
		}

		if (clamp) m_pitchNode->setOrientation(Ogre::MathUtils::getQuatFromEuler(p90));
	}

	if (m_keyboard.key_count > 0)
	{
		int up = (m_keyboard.isKeyDown(KC_WKEY) || m_keyboard.isKeyDown(KC_UPARROWKEY)) ? 1 : 0;
		int dn = (m_keyboard.isKeyDown(KC_SKEY) || m_keyboard.isKeyDown(KC_DOWNARROWKEY)) ? 1 : 0;
		int lf = (m_keyboard.isKeyDown(KC_AKEY) || m_keyboard.isKeyDown(KC_LEFTARROWKEY)) ? 1 : 0;
		int rg = (m_keyboard.isKeyDown(KC_DKEY) || m_keyboard.isKeyDown(KC_RIGHTARROWKEY)) ? 1 : 0;

		int v0= up - dn, v1= lf - rg;
		if (v0 != 0)
		{
			v0 *= 20;
			Ogre::Real fmot = (-(v0)) * (tick);
			Ogre::Vector3 rp = m_rollNode->getPosition();
			Ogre::Quaternion cr = m_camNode->_getDerivedOrientation();

			m_rollNode->setPosition(rp + cr * Ogre::Vector3(0, 0, fmot));
		}
		if (v1 != 0)
		{
			v1 *= 20;
			Ogre::Real smot = (-(v1)) * (tick);
			Ogre::Vector3 rp = m_rollNode->getPosition();
			Ogre::Quaternion cr = m_camNode->_getDerivedOrientation();

			m_rollNode->setPosition(rp + cr * Ogre::Vector3(smot, 0, 0));
		}
	}
}

// ----------------------------------------------------------------------------
void TestApp::endFrame(void)
{
    if (m_keyboard.isKeyDown(KC_QKEY))
        m_quit = true;
}



#ifdef __APPLE__
#define MAXPATHLEN 512
char* AppleGetBundleDirectory(void) {
	CFURLRef bundleURL;
	CFStringRef pathStr;
	static char path[MAXPATHLEN];
	memset(path,MAXPATHLEN,0);
	CFBundleRef mainBundle = CFBundleGetMainBundle();
	
	bundleURL = CFBundleCopyBundleURL(mainBundle);
	pathStr = CFURLCopyFileSystemPath(bundleURL, kCFURLPOSIXPathStyle);
	CFStringGetCString(pathStr, path, MAXPATHLEN, kCFStringEncodingASCII);
	CFRelease(pathStr);
	CFRelease(bundleURL);
	return path;
}
#endif


// ----------------------------------------------------------------------------
int main(int argc, char**argv)
{
	char *name = "PhysicsAnimationBakingDemo.blend";
#if __APPLE__
	char newName[1024];
	
	char* bundlePath= AppleGetBundleDirectory();
	//cut off the .app filename
	char* lastSlash=0;
	if (lastSlash= strrchr(bundlePath, '/'))
		*lastSlash= '\0';
	sprintf(newName,"%s/%s",bundlePath,"game.blend");
	//	eng.loadBlendFile(newName);
	// how do you debug the Bundle execution, without a console?

	sprintf(newName,"%s/%s/%s",AppleGetBundleDirectory(),"Contents/Resources",name);
	name= newName;
	//FILE* dump= fopen ("out.txt","wb");
	//fwrite(newName,1,strlen(newName),dump);
	//fclose(dump);

#else
	if (argc > 1)
		name= argv[argc-1];
#endif

	TestApp app(name);
    try {
        app.go();
    }
    catch (Ogre::Exception &e)
    {
        printf("%s\n", e.getDescription().c_str());
    }
    return 0;
}