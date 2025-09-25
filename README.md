# Gestures library

The Gestures library takes input events from touchpads, mice, and multitouch
mice, detects gestures, and applies quality improvements to the incoming data.
It is used on ChromeOS and Android.

## Features

*   Detection of common touchpad gestures:
    *   Pointer movement (from single-finger swiping)
    *   Scrolling (two-finger swipes)
    *   Three- and four-finger swipes
    *   Pinch gestures
    *   Tap-to-click with one to three fingers
*   Configurable data quality improvements, including:
    *   Palm classification
    *   Box filtering for noisy touch coordinate input
    *   Detection and mitigation of click wiggle (where clicking by pressing
        down on a touchpad causes pointer movement)
    *   Merged finger detection
    *   Detection and mitigation of drumrolls (where one finger going down very
        shortly after another lifts is reported as one finger moving very
        quickly)
*   Support for haptic touchpads
*   Support for "Track 5, Report 2" and semi-multitouch touchpads
*   Support for conventional and multitouch mice

## Architecture

The Gestures library consumes hardware states, which describe the state of an
input device at one instant in time. (These are represented by `struct
HardwareState`.) For a touchpad, this is the set of touches currently down
(including coordinates, dimensions, and orientations, if available) and the set
of buttons currently pressed.

Hardware states flow up a stack of interpreter objects, each of which
can modify them to improve data quality or report gestures that they have
detected, which are represented by `struct Gesture`. Once a gesture is detected
it passes back down the stack until it reaches the bottom, with each
interpreter able to make modifications to it before it is reported to the user
of the library.

The actual stacks used for different device types can be found in the relevant
`GestureInterpreter::Initialize...` functions (e.g. `InitializeTouchpad`) in
gestures.cc.

### Example stack

For example, a simplified touchpad stack might be made up of the following
interpreters:

*   `ImmediateInterpreter` (the core of the touchpad stack, which detects
    gestures)
*   `PalmClassifyingFilterInterpreter`
*   `ScalingFilterInterpreter`
*   `LoggingFilterInterpreter`

When a new hardware state is passed in to the library, it will first be passed
to `LoggingFilterInterpreter`, which will add it to its activity log, and then
pass it on unmodified to `ScalingFilterInterpreter`. `ScalingFilterInterpreter`
will scale the values in the hardware state to be in mm, rather than whatever
units and resolution the touchpad reported them in. Next,
`PalmClassifyingFilterInterpreter` will look for any touches that look like they
could be palms, and mark them with a flag. Lastly, `ImmediateInterpreter` will
detect gestures from the remaining touches.

Any gestures that `ImmediateInterpreter` detects will then pass back down the
stack. `ScalingFilterInterpreter` may apply a scale factor to them, and
`LoggingFilterInterpreter` will add them to its activity log. Finally, the
`Gesture` struct will be passed to the user of the library via a callback.

### Hardware properties

In addition to hardware states, the library also takes a `struct
HardwareProperties`, which describes the unchanging capabilities of an input
device. For a touchpad, this includes the ranges and resolutions of its X, Y,
and orientation axes, as well as the maximum number of touches it can report
simultaneously. This information is accessible to all interpreters in the stack.

### Gesture properties

Each interpreter can declare configurable parameters using [gesture
properties](docs/gesture_properties.md). These can include everything from
simple booleans controlling whether scrolling is reversed, to arrays describing
acceleration curves to be applied to pointer movement.
