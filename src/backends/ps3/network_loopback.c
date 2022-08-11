 /*
 * =======================================================================
 *
 * Low level network code working as loopback only
 *
 * =======================================================================
 */

#include "../../common/header/common.h"
#include <arpa/inet.h>

netadr_t net_local_adr;

#define LOOPBACK 0x7f000001
#define MAX_LOOPBACK 4

typedef struct
{
	byte data[MAX_MSGLEN];
	int datalen;
} loopmsg_t;

typedef struct
{
	loopmsg_t msgs[MAX_LOOPBACK];
	int get, send;
} loopback_t;

loopback_t loopbacks[2];
int ip_sockets[2];
int ip6_sockets[2];
int ipx_sockets[2];

void
NET_Init()
{
}

void
NET_Shutdown(void)
{
	NET_Config(false); /* close sockets */
}

void
NET_Config(qboolean multiplayer)
{
	/* nothing - loop back doesn't open or close any sockets */
}


qboolean
NET_GetLoopPacket(netsrc_t sock, netadr_t *net_from, sizebuf_t *net_message)
{
	int i;
	loopback_t *loop;

	loop = &loopbacks[sock];

	if (loop->send - loop->get > MAX_LOOPBACK)
	{
		loop->get = loop->send - MAX_LOOPBACK;
	}

	if (loop->get >= loop->send)
	{
		return false;
	}

	i = loop->get & (MAX_LOOPBACK - 1);
	loop->get++;

	memcpy(net_message->data, loop->msgs[i].data, loop->msgs[i].datalen);
	net_message->cursize = loop->msgs[i].datalen;
	*net_from = net_local_adr;
	return true;
}

void
NET_SendLoopPacket(netsrc_t sock, int length, void *data, netadr_t to)
{
	int i;
	loopback_t *loop;

	loop = &loopbacks[sock ^ 1];

	i = loop->send & (MAX_LOOPBACK - 1);
	loop->send++;

	memcpy(loop->msgs[i].data, data, length);
	loop->msgs[i].datalen = length;
}

qboolean
NET_GetPacket(netsrc_t sock, netadr_t *net_from, sizebuf_t *net_message)
{
	return NET_GetLoopPacket(sock, net_from, net_message);
}

void
NET_SendPacket(netsrc_t sock, int length, void *data, netadr_t to)
{
	switch (to.type)
	{
		case NA_LOOPBACK:
			NET_SendLoopPacket(sock, length, data, to);
			return;
			break;
		default:
			Com_Printf("NET_SendPacket to non loopback impossible!\n");
			exit(1);
			break;
	}
	return;
}

qboolean
NET_CompareAdr(netadr_t a, netadr_t b)
{
	if (a.type != b.type)
	{
		return false;
	}

	if (a.type == NA_LOOPBACK)
	{
		return true;
	}

	if (a.type == NA_IP)
	{
		if ((a.ip[0] == b.ip[0]) && (a.ip[1] == b.ip[1]) &&
			(a.ip[2] == b.ip[2]) && (a.ip[3] == b.ip[3]) && (a.port == b.port))
		{
			return true;
		}
	}

	if (a.type == NA_IP6)
	{
		if ((memcmp(a.ip, b.ip, 16) == 0) && (a.port == b.port))
		{
			return true;
		}
	}

	if (a.type == NA_IPX)
	{
		if ((memcmp(a.ipx, b.ipx, 10) == 0) && (a.port == b.port))
		{
			return true;
		}

		return false;
	}

	return false;
}

qboolean
NET_CompareBaseAdr(netadr_t a, netadr_t b)
{
	if (a.type != b.type)
	{
		return false;
	}

	if (a.type == NA_LOOPBACK)
	{
		return true;
	}

	if (a.type == NA_IP)
	{
		if ((a.ip[0] == b.ip[0]) && (a.ip[1] == b.ip[1]) &&
			(a.ip[2] == b.ip[2]) && (a.ip[3] == b.ip[3]))
		{
			return true;
		}

		return false;
	}

	if (a.type == NA_IP6)
	{
		if ((memcmp(a.ip, b.ip, 16) == 0))
		{
			return true;
		}

		return false;
	}

	if (a.type == NA_IPX)
	{
		if ((memcmp(a.ipx, b.ipx, 10) == 0))
		{
			return true;
		}

		return false;
	}

	return false;
}

qboolean
NET_StringToAdr(const char *s, netadr_t *a)
{
	memset(a, 0, sizeof(*a));

	if (!strcmp(s, "localhost"))
	{
		a->type = NA_LOOPBACK;
		return true;
	}

	Com_Printf("NET_StringToAdr not implemented!\n");

	return false;
}

qboolean NET_IsLocalAddress(netadr_t adr)
{
    return NET_CompareAdr(adr, net_local_adr);
}

char *
NET_BaseAdrToString(netadr_t a)
{
	static char s[64];

	switch (a.type)
	{
		case NA_IP:
		case NA_LOOPBACK:
			Com_sprintf(s, sizeof(s), "%i.%i.%i.%i", a.ip[0],
				a.ip[1], a.ip[2], a.ip[3]);
			break;

		case NA_BROADCAST:
			Com_sprintf(s, sizeof(s), "255.255.255.255");
			break;

		case NA_IP6:
		case NA_MULTICAST6:
			Com_sprintf(s, sizeof(s), "IPv6 not supported");
			break;

		default:
			Com_sprintf(s, sizeof(s), "invalid IP address family type");
			break;
	}

	return s;
}

char *
NET_AdrToString(netadr_t a)
{
	static char s[64];
	const char *base;

	base = NET_BaseAdrToString(a);
	Com_sprintf(s, sizeof(s), "[%s]:%d", base, a.port);

	return s;
}

void
NET_Sleep(int msec)
{
	return;
}