#ifndef _HAVCFG_H
# define _HAVCFG_H

/* your configuration */

/* # define _HAV_PRINT_ERRORS */

# ifdef _HAV_IS_EXTERNAL
#  include <hav/havcfgx.h>
# else
#  include "havcfgx.h"
# endif

#endif