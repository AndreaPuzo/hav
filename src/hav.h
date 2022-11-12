#ifndef _HAV_H
# define _HAV_H

# define _HAV_VERSION_MAJOR 0
# define _HAV_VERSION_MINOR 0
# define _HAV_VERSION_PATCH 0
# define _HAV_VERSION       ((_HAV_VERSION_MAJOR << 16) | (_HAV_VERSION_MINOR))

# define __HAV__ _HAV_VERSION

# ifdef _HAV_IS_EXTERNAL
#  include <hav/havcfg.h>
#  include <hav/havdef.h>
#  include <hav/havstr.h>
# else
#  include "havcfg.h"
#  include "havdef.h"
#  include "havstr.h"
# endif

#endif