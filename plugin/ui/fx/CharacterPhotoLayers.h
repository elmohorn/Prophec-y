#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <algorithm>
#include <array>
#include <cmath>

// Image-only responsibilities of the Character FX window:
// - decode the approved photographic atlas once;
// - extract static chassis and independent spring/light layers once;
// - compose layers behind the existing native JUCE aluminum/glass frame.
// Never touches AudioProcessor, host parameters, UI clock or DSP.
// All mutable Image data is prepared on the UI message thread, never in paint.
class CharacterPhotoLayers final
{
public:
    void setAtlas(const void* data,int bytes)
    {
        atlas_=data!=nullptr&&bytes>0
            ?juce::ImageCache::getFromMemory(data,bytes):juce::Image{};
        buildSpringPhotoLayers();
        buildOpticalPhotoLayers();
    }

    bool hasAtlas() const noexcept { return atlas_.isValid(); }
    bool hasOptical() const noexcept { return opticalPhotoStill_.isValid(); }

    void drawSpring(juce::Graphics& g,const std::array<float,2>& shifts,
                    bool active) const
    {
        drawAnimatedPhotoSprings(g,shifts,active);
    }

    void drawOptical(juce::Graphics& g,float lamp,float ldr) const
    {
        if(!hasOptical())
        {
            drawPhotoFrame(g,1);
            return;
        }
        drawPhotoLayer(g,opticalPhotoStill_);
        if(lamp>.001f)
        {
            juce::Graphics::ScopedSaveState saved(g);
            g.setOpacity(std::clamp(lamp,0.f,1.f));
            drawPhotoLayer(g,opticalPhotoEmission_);
        }
        if(ldr>.001f)
        {
            juce::Graphics::ScopedSaveState saved(g);
            g.setOpacity(std::clamp(ldr,0.f,1.f));
            drawPhotoLayer(g,opticalPhotoSensor_);
        }
    }

private:
    // Each photographic source is 448x120, but its central 448x100 region
    // holds the entire bulb/two-coil scene. Crop only 14px top and 6px
    // bottom of the empty rail, never the lower spring. Source and target
    // are scaled isotropically (130/100=1.3), with lateral pillarboxing.
    static constexpr float photoScale=1.30f;
    static constexpr int photoX=29,photoY=14,photoW=582,photoH=130;
    static constexpr int photoCropY=14,photoCropH=100;
    static void drawPhotoLayer(juce::Graphics& g,const juce::Image& image)
    {
        if(image.isValid())
            g.drawImage(image,photoX,photoY,photoW,photoH,
                        0,photoCropY,448,photoCropH);
    }
    void drawPhotoFrame(juce::Graphics& g, int row) const
    {
        if(!atlas_.isValid())return;
        g.drawImage(atlas_,photoX,photoY,photoW,photoH,
                    0,row*120+photoCropY,448,photoCropH);
    }

    // Load-time extraction only. The original JPEG is one immutable study,
    // while the physical chassis, two wire bodies and optical light are
    // independent caches. No per-frame image extraction or masking.
    static float feather(float distance,float width) noexcept
    {
        const float t=std::clamp(distance/width,0.f,1.f);
        return t*t*(3.f-2.f*t);
    }

    void buildSpringPhotoLayers()
    {
        springPhotoBackdrop_={};
        springPhotoCoils_={};
        if(!atlas_.isValid()||atlas_.getWidth()<448||
           atlas_.getHeight()<360) return;
        constexpr int w=448,h=120;
        springPhotoBackdrop_=juce::Image(juce::Image::ARGB,w,h,true);
        for(auto& coil:springPhotoCoils_)
            coil=juce::Image(juce::Image::ARGB,w,h,true);
        // Pixel-space regions were measured from the *approved* 448x120
        // Spring source. Central winding moves; both socket mountings remain
        // permanently in the still chassis. A feathered matte avoids seams.
        constexpr std::array<int,2> top{24,73};
        constexpr std::array<int,2> bottom{62,114};
        constexpr int x0=94,x1=357;
        for(int y=0;y<h;++y)for(int x=0;x<w;++x)
        {
            const auto original=atlas_.getPixelAt(x,y);
            auto still=original;
            for(std::size_t i=0;i<2;++i)
            {
                if(x<x0||x>x1||y<top[i]||y>bottom[i]) continue;
                const float xf=feather(static_cast<float>(
                    std::min(x-x0,x1-x)),22.f);
                const float yf=feather(static_cast<float>(
                    std::min(y-top[i],bottom[i]-y)),5.f);
                const float bounds=xf*yf;
                const float red=original.getFloatRed();
                const float green=original.getFloatGreen();
                const float blue=original.getFloatBlue();
                // Metal specular highlights are substantially less red than
                // the amber tank. The spatial matte includes shadowed wire,
                // but excludes most of the bare orange background.
                const float metallic=std::clamp(
                    std::max((green-.29f*red)*5.3f,
                             (blue-.09f*red)*4.8f),0.f,1.f);
                const float brightness=std::clamp((red-.11f)*4.8f,0.f,1.f);
                const float matte=bounds*std::clamp(
                    .23f*brightness+.92f*metallic,0.f,1.f);
                springPhotoCoils_[i].setPixelAt(x,y,original.withAlpha(matte));
                // Replace the *entire central wire silhouette* in the static
                // background, not only bright spots; otherwise unmoving wire
                // ghosts will remain behind displaced coils.
                const float yFraction=std::clamp(
                    (static_cast<float>(y-top[i]))/
                    static_cast<float>(bottom[i]-top[i]),0.f,1.f);
                const auto upper=atlas_.getPixelAt(x,std::max(0,top[i]-5));
                const auto lower=atlas_.getPixelAt(x,std::min(119,bottom[i]+5));
                const auto clean=upper.interpolatedWith(lower,yFraction)
                    .withMultipliedBrightness(.76f);
                still=still.interpolatedWith(clean,bounds);
            }
            springPhotoBackdrop_.setPixelAt(x,y,still);
        }
    }

    // The photographic "on" row differs from the "off" row in several
    // places (including sockets). Decompose it into a STATIC off chassis and
    // two spatially restricted additive layers: filament/beam and LDR.
    // This prevents the metal enclosure from flashing with the LFO.
    void buildOpticalPhotoLayers()
    {
        opticalPhotoStill_={};
        opticalPhotoEmission_={};
        opticalPhotoSensor_={};
        if(!atlas_.isValid()||atlas_.getWidth()<448||
           atlas_.getHeight()<360) return;
        constexpr int w=448,h=120;
        opticalPhotoStill_=juce::Image(juce::Image::ARGB,w,h,true);
        opticalPhotoEmission_=juce::Image(juce::Image::ARGB,w,h,true);
        opticalPhotoSensor_=juce::Image(juce::Image::ARGB,w,h,true);
        for(int y=0;y<h;++y)for(int x=0;x<w;++x)
        {
            const auto off=atlas_.getPixelAt(x,120+y);
            const auto on=atlas_.getPixelAt(x,240+y);
            opticalPhotoStill_.setPixelAt(x,y,off);
            const float fx=static_cast<float>(x),fy=static_cast<float>(y);
            const float bulb=std::exp(
                -std::pow((fx-159.f)/65.f,2.f)
                -std::pow((fy-71.f)/45.f,2.f));
            // The light cone spreads from the filament towards the sensor.
            const float beamWidth=14.f+.19f*std::max(0.f,fx-205.f);
            const float beam=fx>205.f && fx<378.f
                ? std::exp(-std::pow((fy-75.f)/beamWidth,2.f))*
                  feather(std::min(fx-205.f,378.f-fx),18.f) : 0.f;
            const float lampFocus=std::clamp(std::max(bulb,beam),0.f,1.f)*
                                  feather(fx-85.f,23.f)*
                                  feather(367.f-fx,20.f);
            const float sensorFocus=std::exp(
                -std::pow((fx-380.f)/36.f,2.f)
                -std::pow((fy-70.f)/40.f,2.f))*
                feather(fx-350.f,13.f)*
                feather(428.f-fx,13.f);
            // On/off RGB reconstruction with a bounded alpha. Target color
            // lies between "off" and "on" only where illumination is allowed.
            const int oldRgb[3]={off.getRed(),off.getGreen(),off.getBlue()};
            const int litRgb[3]={on.getRed(),on.getGreen(),on.getBlue()};
            int diff[3]{};
            float requiredAlpha=0.f;
            for(int k=0;k<3;++k)
            {
                diff[k]=std::max(0,litRgb[k]-oldRgb[k]);
                if(oldRgb[k]<255)
                    requiredAlpha=std::max(requiredAlpha,
                        static_cast<float>(diff[k])/(255.f-oldRgb[k]));
            }
            if(requiredAlpha<=0.f)continue;
            const auto rebuilt=juce::Colour::fromRGB(
                static_cast<juce::uint8>(std::clamp(
                    static_cast<int>(std::lround(oldRgb[0]+diff[0]/requiredAlpha)),0,255)),
                static_cast<juce::uint8>(std::clamp(
                    static_cast<int>(std::lround(oldRgb[1]+diff[1]/requiredAlpha)),0,255)),
                static_cast<juce::uint8>(std::clamp(
                    static_cast<int>(std::lround(oldRgb[2]+diff[2]/requiredAlpha)),0,255)));
            opticalPhotoEmission_.setPixelAt(x,y,rebuilt.withAlpha(
                requiredAlpha*lampFocus));
            opticalPhotoSensor_.setPixelAt(x,y,rebuilt.withAlpha(
                requiredAlpha*sensorFocus));
        }
    }

    void drawAnimatedPhotoSprings(juce::Graphics& g,
                                  const std::array<float,2>& shifts,bool active) const
    {
        if(!springPhotoBackdrop_.isValid())
        {
            drawPhotoFrame(g,0);
            return;
        }
        drawPhotoLayer(g,springPhotoBackdrop_);
        for(std::size_t row=0;row<2;++row)
        {
            if(!springPhotoCoils_[row].isValid())continue;
            // Only ONE compositing draw per moving coil (previously 32
            // transforms per coil). A feathered winding matte keeps the
            // bolted mountings stationary and avoids hard segment seams.
            const float yShift=active?shifts[row]:0.f;
            juce::Graphics::ScopedSaveState state(g);
            g.reduceClipRegion(juce::Rectangle<int>{
                photoX,photoY,photoW,photoH});
            // Precisely one isotropic image transformation per coil.
            // Top/cropped source pixels remain behind the bezel and glass.
            g.drawImageTransformed(springPhotoCoils_[row],
                juce::AffineTransform::scale(photoScale).translated(
                    static_cast<float>(photoX),
                    static_cast<float>(photoY)-
                    static_cast<float>(photoCropY)*photoScale+yShift));
        }
    }


    juce::Image atlas_;
    juce::Image springPhotoBackdrop_;
    std::array<juce::Image,2> springPhotoCoils_{};
    juce::Image opticalPhotoStill_,opticalPhotoEmission_,opticalPhotoSensor_;
};
