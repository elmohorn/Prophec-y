#pragma once
#include <cstddef>
struct b_whirl;
class RotarySetBfree {
public:
 enum Speed {Slow, Stop, Fast};
 RotarySetBfree(); ~RotarySetBfree();
 RotarySetBfree(const RotarySetBfree&)=delete; RotarySetBfree& operator=(const RotarySetBfree&)=delete;
 bool prepare(double sampleRate);
 void resetAudio() noexcept;
 void setSpeed(Speed speed) noexcept;
 // Motion adds bounded rotor variation. Wheel sweeps slowly from Slow to Fast.
 // wheelDepth=0 disables modulation-wheel influence; set values on audio thread.
 void setMotion(float amount) noexcept;
 void setModWheel(float amount) noexcept;
 void setWheelDepth(float amount) noexcept;
 void process(const float* mono,float* left,float* right,std::size_t frames) noexcept;
 double hornHz()const noexcept; double drumHz()const noexcept;
 double hornAngle()const noexcept; double drumAngle()const noexcept;
private:
 void updateMotion(std::size_t frames) noexcept;
 b_whirl* w_=nullptr; Speed speed_=Slow; float motion_=0,wheel_=0,depth_=0;
 double phase_=0, phaseDrum_=0, smoothWheel_=0, smoothMotion_=0;
};
