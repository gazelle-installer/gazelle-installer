gazelle-installer
============

A customizable GUI installer for antiX Linux and MX Linux

The gui is customizable through modification of the gazelle-installer-data package.

Customizable features include

Project Name
Project Version
Default Min. and Recommended Drive size
Override for installing to target partition on same drive as boot partition (for from=iso installs)

And a few other niceties.  Installer text boxes will display text modified per the variables shown 
in the installer.conf file of gazelle-installer-data.

Services to show in the Services tab are also configurable through services.list, and Descriptions support localizations.

Services tab supports sysVinit enable/disable actions and systemd systemctl enable/disable actions, depending on init system in use.

See gazelle-installer-data services.list and installer.conf for instructions on configuration options.

gazelle-installer is maintained by the MX Linux and antiX Linux Projects.

Development Notes
-----------------
The installer source code is stored in a version control repository that uses branches.

### Master branch
The master branch is for the newest installer. Code in this branch is in a working state, and is for the latest stable MX Linux and antiX Linux release.
It is not guaranteed to be compatible with an earlier version of MX Linux or antiX Linux, but if you're lucky, it just might work.

### Release branches
A release branch will be named after the Debian code name it is associated with. A release branch contains the latest version of the installer designed for that release.
Code in a release branch is meant for versions of MX Linux or antiX Linux based on the Debian release with that code name.
#### Future release
Code in a branch marked with a future Debian code name are destined for a future release. They are likely to work with the absolute most current version of MX and antiX, but there is no guarantee here, especially once an alpha or beta version of antiX or MX Linux becomes available.

### Feature branches
Features that require significant amounts of work before it is ready for at least beta testing, should be developed in a feature branch.
Code in this branch should not be used for anything other than testing the latest features. It may not even compile, let alone work.

Versioning
----------
Because both MX Linux and antiX Linux use packages with slightly different version numbers, both of which are date-based, this versioning scheme identifies the state of the common code base in this repository.

### Version 5 (MX and antiX 23)
Many partition manager fixes and improvements. Support for virtual devices (eg LUKS or LVM).
Swap file and hibernation support, and Btrfs improvements.
Integrated installation media MD5 checker which runs at the start.
#### Version 5.2
Allow the installer to compile with Qt 6. Since this is not a major re-write, no major version increment.
MX Linux 10th anniversary special: 5×2=10
The start of a consistent semantic version numbering scheme for the common gazelle-installer code base.

### Version 4 (MX and antiX 21)
New partition manager core, which replaces the older restrictive approach.
Not formally identified as version 4.

### Version 3 (MX and antiX 19)
Fused the two window components as one, and several resource usage improvements.
Various partition detection, management and auto-mount improvements.
Not formally identified as version 3.

### Version 2 (MX 18.4 and antiX 19-a1)
Major GUI update (time saver) to allow information to be entered while the image is.

### Version 1
The original gazelle installer (as of this repository), along with a few extras.
Not formally identified as version 1.