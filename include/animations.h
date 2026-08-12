// ============================================================================
//  animations.h -- the sprite animations, declared.
//
//  The frame bytes themselves live in animations.cpp and never in a header: a
//  const array in a header gets a private copy in every .cpp that includes it,
//  which silently duplicates it in flash with no warning. What is declared here
//  is only the 12-byte Anim descriptor, so including this file costs nothing.
//
//  See the top of animations.cpp for how to add one, and drawAnim() in
//  display.h for how to play one.
// ============================================================================

#ifndef ANIMATIONS_H
#define ANIMATIONS_H

#include "display.h"   // the Anim struct and drawAnim()

// ---- Eye blink -------------------------------------------------------------
// 64x64, ONE blink, about 0.9 seconds. Step 0 and the final step are both the
// eye fully open, so drawing it at sinceMs = 0 gives a held-open eye and the
// caller decides when to blink -- which is what lets the eye troll blink at
// irregular intervals rather than on a metronome.
extern const Anim ANIM_EYE_BLINK;

// ---- Walk cycle ------------------------------------------------------------
// 64x64 figure walking IN PLACE, about 1.2 seconds for two strides. Loops
// cleanly, so drawAnim(..., loop=true) runs it forever; slide x along yourself
// to make it actually travel. WALK_STRIDE_PX in values.h is what keeps the
// travel in step with the legs.
extern const Anim ANIM_WALK;

// The two steps where the legs are fully extended and a foot lands, so a
// footstep sound can be played as the animation crosses each of them. Measured
// from the frame data, so they stay correct unless the frames change.
#define WALK_STEP_A 0
#define WALK_STEP_B 16

#endif
