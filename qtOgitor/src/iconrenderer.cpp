/*/////////////////////////////////////////////////////////////////////////////////
/// An
///    ___   ____ ___ _____ ___  ____
///   / _ \ / ___|_ _|_   _/ _ \|  _ \
///  | | | | |  _ | |  | || | | | |_) |
///  | |_| | |_| || |  | || |_| |  _ <
///   \___/ \____|___| |_| \___/|_| \_\
///                              File
///
/// Copyright (c) 2012 Andrew Fenn <andrewfenn@gmail.com> and the Ogitor Team
//
/// The MIT License
///
/// Permission is hereby granted, free of charge, to any person obtaining a copy
/// of this software and associated documentation files (the "Software"), to deal
/// in the Software without restriction, including without limitation the rights
/// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
/// copies of the Software, and to permit persons to whom the Software is
/// furnished to do so, subject to the following conditions:
///
/// The above copyright notice and this permission notice shall be included in
/// all copies or substantial portions of the Software.
///
/// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
/// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
/// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
/// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
/// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
/// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
/// THE SOFTWARE.
////////////////////////////////////////////////////////////////////////////////*/


#include "iconrenderer.hxx"

#include "OgitorsRoot.h"

using namespace Ogitors;

//----------------------------------------------------------------------------------------
IconRenderer::IconRenderer()
{
    mResourceManager = Ogre::ResourceGroupManager::getSingletonPtr();
    mResourceManager->createResourceGroup("QTIconRenderer");
    mResourceManager->initialiseResourceGroup("QTIconRenderer");
    
    mSceneMgrPtr = Ogre::Root::getSingletonPtr()->createSceneManager("OctreeSceneManager", "QTIconRendererSceneManager");
    mSceneMgrPtr->setAmbientLight(Ogre::ColourValue(1,1,1));

    Ogre::TexturePtr rendertexture = Ogre::TextureManager::getSingleton().createManual( "RenderTex", 
                   "QTIconRenderer", Ogre::TEX_TYPE_2D, 
                   128, 128, 0, Ogre::PF_B8G8R8, Ogre::TU_RENDERTARGET );

    mRttTex = rendertexture->getBuffer()->getRenderTarget();

    // create our plane to set a texture to
    Ogre::Plane plane(Ogre::Vector3::UNIT_Z, 0);
    Ogre::MeshManager::getSingleton().createPlane("terrain", "QTIconRenderer",
        plane, 100, 100, 1, 1, true, 1, 1, 1, Ogre::Vector3::UNIT_Y);

    // attach the plane to the scene manager and rotate it so the camera can see it
    mEntityTerrain = mSceneMgrPtr->createEntity("terrainEntity", "terrain");
    Ogre::SceneNode* node = mSceneMgrPtr->getRootSceneNode()->createChildSceneNode();
    node->attachObject(mEntityTerrain);
    mEntityTerrain->setCastShadows(false);
    
    
    Ogre::Camera* RTTCam = mSceneMgrPtr->createCamera("QTIconRendererCam");
    RTTCam->setNearClipDistance(0.01F);
    RTTCam->setFarClipDistance(0);
    RTTCam->setAspectRatio(1);
    RTTCam->setFOVy(Ogre::Degree(90));
    RTTCam->setPosition(0,0,50);
    RTTCam->lookAt(0,0,0);
    
    Ogre::Viewport *v = mRttTex->addViewport( RTTCam );
    v->setBackgroundColour(Ogre::ColourValue(1,1,1));
    v->setClearEveryFrame( true );
}

IconRenderer::~IconRenderer()
{
    mRttTex->removeAllViewports();

    Ogre::TextureManager::getSingletonPtr()->unload(mRttTex->getName());
    Ogre::TextureManager::getSingletonPtr()->remove(mRttTex->getName());

    Ogre::TextureManager::getSingletonPtr()->unload("RenderTex");
    Ogre::TextureManager::getSingletonPtr()->remove("RenderTex");

    Ogre::Root::getSingletonPtr()->destroySceneManager(mSceneMgrPtr);
    mResourceManager->destroyResourceGroup("QTIconRenderer");
}

QImage IconRenderer::fromOgreMaterial(const Ogre::String& name, const Ogre::String& resourceGroup)
{
    mResourceGroup = resourceGroup;
    Ogre::MaterialPtr material = Ogre::MaterialManager::getSingletonPtr()->load(name, mResourceGroup);
    return _getRenderTarget(material->getName());
}

QImage IconRenderer::fromOgreImage(const Ogre::String& name, const Ogre::String& resourceGroup)
{
    mResourceGroup = resourceGroup;
    Ogre::Image img;
    img.load(name,mResourceGroup);
    if (!Ogre::PixelUtil::isAccessible(img.getFormat()))
        return _imageFromRenderTarget(name, img);

    size_t size = Ogre::PixelUtil::getMemorySize(img.getWidth(), img.getHeight(), img.getDepth(), Ogre::PF_A8R8G8B8);
    unsigned char *dataptr = OGRE_ALLOC_T(unsigned char, size, Ogre::MEMCATEGORY_GENERAL);

    Ogre::PixelBox pixbox(128,128, 1, Ogre::PF_A8R8G8B8, dataptr);
    Ogre::Image::scale(img.getPixelBox(), pixbox);
    pixbox.setConsecutive();

    QImage qimg = QImage(dataptr, pixbox.getWidth(), pixbox.getHeight(), QImage::Format_ARGB32);

    OGRE_FREE(dataptr, Ogre::MEMCATEGORY_GENERAL);

    return qimg;
}

QImage IconRenderer::_imageFromRenderTarget(const Ogre::String& name, Ogre::Image& img)
{
    Ogre::TextureManager::getSingletonPtr()->loadImage("QTTextureName", mResourceGroup, img);

    // create our material
    Ogre::MaterialPtr material = Ogre::MaterialManager::getSingletonPtr()->create("terrainMaterial", mResourceGroup);
    Ogre::Technique * technique = material->getTechnique(0);
    Ogre::Pass* pass = technique->getPass(0);
    Ogre::TextureUnitState* textureUnit = pass->createTextureUnitState();
    textureUnit->setTextureName("QTTextureName");

    return _getRenderTarget(material->getName());
}

QImage IconRenderer::_getRenderTarget(const Ogre::String& matName)
{
    /* Some formats aren't possible to get the image data
    just render to a render target so we can generate an image that way */
    mEntityTerrain->setMaterialName(matName);

    mRttTex->update();

    size_t size = Ogre::PixelUtil::getMemorySize(128, 128, 1, Ogre::PF_B8G8R8);
    unsigned char *dataptr = OGRE_ALLOC_T(unsigned char, size, Ogre::MEMCATEGORY_GENERAL);
    Ogre::PixelBox pb(128,128,1,Ogre::PF_B8G8R8, dataptr);
    pb.setConsecutive();

    mRttTex->copyContentsToMemory(pb, Ogre::RenderTarget::FB_FRONT);
    QImage qimg(dataptr, pb.getWidth(), pb.getHeight(), QImage::Format_RGB888);

    OGRE_FREE(dataptr, Ogre::MEMCATEGORY_GENERAL);
    
    return qimg;
}
