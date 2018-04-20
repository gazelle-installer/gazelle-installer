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

For now the services tab supports sysVinit enable/disable actions. 

See gazelle-installer-data services.list and installer.conf for instructions on configuration options.

gazelle-installer is maintained by the MX Linux and antiX Linux Projects.
