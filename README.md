# Leap for Max/MSP/Jitter

A native (Mac + Windows) Max/MSP object for interfacing with LeapMotion controller.

Aiming to expose as much of the v2 SDK capabilities as possible.

Working:
- Connection status, FPS
- IR images
- Hands, fingers
- Nearest hand ID
- @unique, @allframes and @background to control when frames are processed
- @hmd for the LeapVR optimization
- Gesture recognition (circle, swipe, key & screen taps)
- Frame serialization/deserialization (example via jit.matrixset)
- Backwards-compatibility with [aka.leapmotion] via @aka 1 

Work-in-progress: 
- Visualizer (wip)
- Bones, palm, wrist, arm (wip)
- IR image warp/rectification for e.g. see-through AR (wip)
- Motion tracking between frames (wip)
- Grip / pinch (wip)
- Tools (wip) 

New project, many things may still change (including object name!)

## Notes on use

- You need to [install the LeapMotion v2 driver](https://www.leapmotion.com/setup)
- Windows: You need to make sure the Leap.dll is always next to the leap.mxe.
- Images: You need to enable "allow images" in the LeapMotion service for this to work.
 


