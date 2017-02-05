Networked PhysX Component
=========================

This project allows players to control pawns that are simulating Physics with PhysX, and provides prediction and reconciliation. It interfaces with the engines INetworkPredictionInterface.

**IMPORTANT:** This project is a work-in-progress and incomplete. I also highly recommend using a fixed-timestep version of the engine, which will result in far fewer corrections needing to be made.

[Unreal Forum Thread](https://forums.unrealengine.com/showthread.php?135955-Networked-Physics-with-PhysX/page2)

TODO
----
* Add Smoothing
* Use a separate PhysX scene for replay. This will prevent stuttering and collision errors with other non-local pawns. (May require source change).
* Setup for locked and non-locked TimeSteps versions of the engine.
* Use new PhysX delegates in 4.15 as well as pre/post Ticks.
* Improve substepping support (?)
* Move to a plugin based format.
* Implement bone-based smoothing for skeletal meshes, so that duplicated collision geometry isn't required for smoothing.

[A pre-compiled version of the initial commit to this repro can be downloaded here.](https://drive.google.com/file/d/0B_FT-hzi26QkbW5WaTgtZGRCUzQ/view?usp=sharing)


License
-------
This project and source are released under the **MIT License.**

TL;DR: do what you want with it, just credit me pls :).


Additional Notes
----------------
Thanks to community members 0lento, Blue Man and OmaManfred for making headway with this problem.