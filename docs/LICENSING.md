# Release licensing decision

The proposed portfolio release uses GPL version 3. Lucas selected this route.
Ethan's agreement for his contributions must be recorded before a public binary
release. This document does not grant a new license on another contributor's
work, replace third-party notices or declare a completed distribution audit.

Qt Quick 3D 6.12 is offered commercially or under GPLv3. A release using the
open-source route must include the applicable license, notices and corresponding
source/build materials. Reference: https://doc.qt.io/qt-6/qtquick3d-index.html#licenses-and-attributions.

The packaging helper creates a **local verification candidate**. It includes an
exact source snapshot, file hashes, the Qt SDK's SPDX/SBOM documents, Python's
license, installed Python package license metadata, and the Lucide/Feather icon
notices. Torch and PhysicsNeMo are installed separately and are not in the basic
bundle. Microsoft runtime and Qt third-party distribution notices must be
checked against the actual deployed file inventory before public distribution.

Before publishing:

1. Record both owners' GPLv3 agreement and add the approved root license text.
2. Verify complete notices and corresponding source for the selected Qt modules
   and their deployed third-party libraries, plus the compiler runtime.
3. Publish matching project source/build instructions and dependency source
   locations with the binary. Do not ship only an executable.
4. Use the self-authored portfolio inputs. Official challenge datasets and
   third-party model weights are not automatically licensed by this project.
5. Complete clean Windows account/VM verification. A sanitized developer process
   is useful evidence but does not establish a clean-machine result.

The logo was supplied by Lucas. The site texture, industrial assemblies, pads,
supports and road markings are original code-native artwork in this repository.
See `desktop/assets/ASSET_SOURCES.md` for the exact logo/icon provenance.
