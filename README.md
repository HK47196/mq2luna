type /luna help for usage

example to run the fishing module, which is a rudimentary port of fish.mac:

/luna run fish

use this to stop the module:

/luna stop fish

bind commands are implemented via /ldo currently as a workaround, the
"/[command]" part isn't passed to AddCommand so I can't use that to register
commands from luna modules.
