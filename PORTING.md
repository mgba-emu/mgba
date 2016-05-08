Porting
=======

Porting is preferentially done upstream so as to avoid fragmenting the codebase into individually unmergeable forks. As such, precaution must be taken to keep changes separate enough to not interfere with other ports, while still maintaining the ability to add new port-specific code seamlessly.

Folders for each port should be under the `src/platform` folder, and make minimally invasive changes to the rest of the tree. If any changes are needed, try to make sure they are generic and have the ability to be ironed out in the future. For example, if a function doesn't work on a specific platform, maybe a way to make that function more portable should be added.

The general porting process involves branching `master`, making the needed changes, and, when the port is mature enough to not have major effects to other ports, merged into `port/crucible`. The crucible is used for mixing upcoming ports to make sure they aren't fragile when `master` merges into it every so often. At this time, the crucible hasn't yet been merged into `master`, but in the future this may occur regularly. Until then, if a port is to get merged into master, make sure the changes to each port occur on the port-specific branch before being merged into `port/crucible`.

Port-specific TODO
------------------

The following ports are considered incomplete and are thus on branches still. They may work, but should not be considered stable.

### PSP (port/psp)
* Add menu
* Add audio
* Thread support
* Make it faster
	* MIPS dynarec
	* Hardware acceleration
