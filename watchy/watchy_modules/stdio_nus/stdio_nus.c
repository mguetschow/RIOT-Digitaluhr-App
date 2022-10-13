#include <stdlib.h>
#include <stdio_base.h>
#include <isrpipe.h>

#if MODULE_VFS
#include <vfs.h>
#endif

#define ENABLE_DEBUG 0
#include <debug.h>

#include "../../gatt-adv.h"
#include "../../include/watchy.h"
#include "../../include/watchy_events.h"

#include "stdio_nus.h"


/* isrpipe for stdin */
static uint8_t _isrpipe_stdin_mem[32];
isrpipe_t _isrpipe_stdin = ISRPIPE_INIT(_isrpipe_stdin_mem);

void stdio_init(void)
{
	tsrb_clear(&_isrpipe_stdin.tsrb);
#if MODULE_VFS
	vfs_bind_stdio();
#endif
}

ssize_t stdio_read(void* buffer, size_t count)
{
	/* blocks until at least one character was read */
	ssize_t res = isrpipe_read(&_isrpipe_stdin, buffer, count);

	return res;
}

ssize_t stdio_write(const void* buffer, size_t len)
{
	if (watch_state.bluetooth_pwr != BT_CONN)
		return len;

	return gatt_svr_nus_tx_buf((char *)buffer, len);
}

#if IS_USED(MODULE_STDIO_AVAILABLE)
int stdio_available(void)
{
	return tsrb_avail(&_isrpipe_stdin.tsrb);
}
#endif
