# Changelog

## 1.3.0 (2026-11-25)

Battery saver, and a plainer store description.

- Battery saver (a setting): the face never animates. Each minute's view is
  computed unseen and painted once, whole, with the new digits: one display
  update a minute. Taps and touches leave the colors still, and the face
  does not animate when it appears.
- The store description is shorter and says what the face does first.

## 1.2.0 (2026-11-23)

Every hour a new dive, unique to your watch.

- Each watch makes its own random seed, and each hour of each day draws
  from it where to dive and how to turn: no two watches, days or hours share
  a dive (they used to repeat twelve dives, twice a day, on every watch).
- 71 targets instead of 12: Misiurewicz points found up to preperiod +
  period 10 and each checked for a whole hour (no big black blobs, never
  flat, no confetti deep down), dived into either way up.
- The dive spirals: every minute the view turns a little, by the hour's
  random spin plus a random nudge of its own.
- Still computed on the watch, and still reproducible: a dive depends only
  on the watch's seed, the date and the minute.

## 1.1.0 (2026-11-22)

Dark mode, and the face's first settings.

- Dark mode: black digits, drawn in the same black as the set and outlined
  in white, over a darker palette of navy, violet, plum, crimson and ember.
  Black-and-white watches dither it darker to match.
- Dark at night: dark mode can turn itself on between two hours you choose
  (22:00-07:00 by default) and off again in the morning.
- Both are in the face's settings in the Pebble app. The watch remembers
  them, so the face still needs the phone only to change them.

## 1.0.0 (2026-11-22)

First release: an hour-long dive into the Mandelbrot set, on every Pebble.

- The hour picks one of twelve dive targets (Misiurewicz points and Seahorse
  Valley), the minute how deep: 1.2x per minute, to about 47,000x at :59.
- The digits are drawn from the set: white strokes, fine where the dive is
  shallow and bold where it is deep, swelling as the colors flow.
- Black-and-white watches show the same face, dithered.
- The face holds still to spare the battery: each new minute sharpens in, and
  a flick of the wrist (or a touch on Pebble Time 2 and Round 2) sets the
  colors flowing for 30 seconds.
- Needs no phone and no settings.
