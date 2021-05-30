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
The master branch is for the newest installer. Code in this branch is in a working state, and is meant to be included in a future MX Linux and antiX Linux release.
It is not guaranteed to be compatible with the latest stable release of MX Linux or antiX Linux, but if you're lucky, it just might work.

### Release branches
A release branch will be named after the Debian code name it is associated with. A release branch contains the latest version of the installer designed for that release.
Code in a release branch is meant for versions of MX Linux or antiX Linux based on the Debian release with that code name.

### Feature branches
Features that require significant amounts of work before it is ready for at least beta testing, should be developed in a feature branch.
Code in this branch should not be used for anything other than testing the latest features. It may not even compile, let alone work.