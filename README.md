type /luna help for usage

example to run the fishing module, which is a rudimentary port of fish.mac:

/luna run fish

use this to stop the module:

/luna stop fish

bind commands are implemented via /ldo currently as a workaround, the
"/[command]" part isn't passed to AddCommand so I can't use that to register
commands from luna modules.


OBTAINING:
Modules and a pre-built DLL file are provided under the releases section.

BUILDING:
I don't own or use Microsoft Windows or Microsoft Visual Studio. The plugin is
built using mingw32 cross-compilation, it should build fine on native windows
using mingw32.
Due to not being able to use a typical MQ2 plugin build environment, I had to do
some workarounds. More information about this can be found in the various mq2
API files in the source.
