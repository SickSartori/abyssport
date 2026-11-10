# Controls

Golden Abyss was made for a console with two sticks, a touch screen, a touch pad on its back, a gyroscope and a
camera. This page says how each of those is played on a PC, as the program does it today. It describes the code
as it is, taken from Project Nativ: none of it has been played through in abyssport yet.

The in-game menu (F10) has a Controls page where every key can be changed.

## Keyboard and mouse or controller

The setting `input_mode` is `auto` by default: the scheme follows the device used last. Pressing a bound key
switches to keyboard and mouse; a controller button or stick switches back to the controller. `pad` and `kbm`
fix one of the two.

## Keyboard (defaults)

| Action | Key |
|---|---|
| Move (left stick) | W, A, S, D |
| Cross | Space |
| Circle | C |
| Square | F |
| Triangle | E |
| L (aim) | Q |
| R (fire) | R |
| Start | Enter |
| Select | Tab |
| D-pad | Arrow keys |

Each key is the setting `bind_<action>` (an SDL key name) and can be rebound from the menu. A key given to a new
action is taken from the one that had it.

## Mouse

With keyboard and mouse in use, the menu closed and the window focused, the mouse is captured:

- moving it turns the camera (`mouse_look`, with `mouse_sens` and `mouse_invert`);
- its buttons are actions: by default the left button is R (fire) and the right button is L (aim).

Each of the five buttons (left, right, middle and the two side buttons) can be put on any action of the keyboard
table, or on none, from the Controls page of the in-game menu. The settings are `mouse_left`, `mouse_right`,
`mouse_middle`, `mouse_x1` and `mouse_x2`, each an action's name (`r`, `l`, `cross`, `triangle`...) or empty.
The left button also starts the swipes and twists the game asks for while the mouse is captured, whatever it is
set to.

Holding **Alt** frees the cursor. The cursor is then a finger: the left button touches the front screen, the
right button touches the rear pad. This is how the game's touch actions are done: tapping an icon, rubbing a
charcoal sketch, drawing a machete cut, swiping to climb.

Shapes drawn with the mouse are accepted a little more easily than a finger's (a recogniser score of 0.7
instead of 0.8).

## Tilting (balance beams, the river slide)

The console steers these by tilting. On a PC:

- keyboard: A and D tilt left and right while the section lasts, easing back to level when released;
- a controller without a gyroscope: its left stick tilts;
- a controller with a gyroscope (DualShock 4, DualSense, Switch Pro): tilt the controller.

## Controller

Controllers are recognised by SDL: Xbox, PlayStation, Switch and most others map by themselves. The setting
`shoulder_buttons` chooses whether L and R are the triggers (default) or the bumpers.

## Function keys

| Key | What it does |
|---|---|
| F10 | Opens and closes the in-game menu (settings, controls) |
| Esc | Closes the menu, or hides the menu bar |
| F1 | Cycles the performance display |
| F2 | Cycles the internal resolution, 1x to 6x |
| F4 | Turns FSR anti-aliasing off and on |
| F5 | Writes a marker in the log, for a bug report |
| F7 | The rear camera's lamp, for the hold-it-to-the-light puzzles |
| F8 | The experimental raw mouse look |
| F12 | Saves a capture of the frame's graphics |

## Not there yet

- There is no launcher: settings are changed in the in-game menu or in `settings.ini`.
