/* PhotoCraft Classic: forwarding header so that monocypher-ed25519.h's
 * '#include "monocypher.h"' resolves without -Ithird_party/monocypher
 * (upstream expects the src/ directory on the include path). */
#include "../monocypher.h"
