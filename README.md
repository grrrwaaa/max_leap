# Leap for Max/MSP/Jitter

A native (mac + windows) Max/MSP object for interfacing with LeapMotion controller.

Aiming to expose as much of the v2 SDK capabilities as possible:
- IR images* (done)
- Serialization/deserialization (done)
- Hands, fingers (done)
- Bones, palm, wrist, arm (wip)
- IR image warp/rectification for e.g. see-through AR (wip)
- Motion tracking between frames (wip)
- Grip / pinch (wip)
- Gestures (wip)
- Tools (wip) 

Work in progress, many things may change (including object name!)

*You need to enable "allow images" in the LeapMotion service for this to work.

There's also backwards-compatibility mode (@aka 1) that outputs the same data as aka.leapmotion. 


