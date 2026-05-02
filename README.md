# Patches for the Befaco Lich Eurorack module
[Befaco Lich](https://www.befaco.org/lich/) is a module based on Rebel Technology [OWL Platform](https://www.openwarelab.org/).
The OWL platform enables the compilation of various programming languages into binaries that can be executed on the OWL hardware.

This repository stores C++ code for patches meant for the Befaco Lich module.


## Installing and running patches

**INSTALL AND RUN THE PATCHES AT YOUR OWN RISK!** 
The patches have been tested with a Lich module, but may still contain bugs that can cause the module to freeze or crash.

The packaged patch binaries can be found under [releases](https://github.com/ffagerholm/lich-patches/releases), and
can be installed using the [FirmwareSender](https://github.com/pingdynasty/FirmwareSender/releases/tag/v0.1).

To load and run on the Lich module:
* `FirmwareSender -in <Patch name>.bin -out OWL-LICH -run`

To save on Lich (slot 1 in this example):
* `FirmwareSender -in <Patch name>.bin -out OWL-LICH -run -store 1`

For more options of FirmwareSender, run `FirmwareSender -h`.
