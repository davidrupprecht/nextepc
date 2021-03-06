#define TRACE_MODULE net_lib
#include "core.h"
#include "core_debug.h"
#include "core_list.h"
#include "core_pool.h"
#include "core_net.h"
#include "core_errno.h"
#include "core_time.h"

#if USE_USRSCTP == 1
#if HAVE_USRSCTP_H
#include <usrsctp.h>
#endif
#else
#if HAVE_NETINET_SCTP_H
#include <netinet/sctp.h>
#endif
#endif

#if LINUX == 1
#include <netpacket/packet.h>
#include <linux/if_tun.h>
#endif

#if HAVE_NET_ROUTE_H
#include <net/route.h>
#endif

#define NET_FD_TYPE_SOCK    0
#define NET_FD_TYPE_LINK    1

typedef struct {
    lnode_t node;
    void *net_sl;
    int fd;
    int type;
    net_handler handler;
    void *data;
} net_fd_t;

typedef struct {
    int max_fd;
    mutex_id mut;
    list_t fd_list;
    fd_set rfds;
} net_fd_tbl_t;

pool_declare(net_pool, net_sock_t, MAX_NET_POOL_SIZE);
pool_declare(ftp_pool, net_ftp_t, MAX_FTP_SESSION_SIZE);
pool_declare(link_pool, net_link_t, MAX_NET_POOL_SIZE);
pool_declare(net_fd_pool, net_fd_t, MAX_NET_POOL_SIZE);

static net_fd_tbl_t g_net_fd_tbl;

status_t net_init(void)
{
    /* Initialize network connection pool */
    pool_init(&net_pool, MAX_NET_POOL_SIZE);
    /* Initialize ftp connection pool */
    pool_init(&ftp_pool, MAX_FTP_SESSION_SIZE);
    /* Initialize network connection pool */
    pool_init(&link_pool, MAX_NET_POOL_SIZE);
    /* Initialize network fd pool */
    pool_init(&net_fd_pool, MAX_NET_POOL_SIZE);

    memset(&g_net_fd_tbl, 0, sizeof(net_fd_tbl_t));
    mutex_create(&g_net_fd_tbl.mut, MUTEX_DEFAULT);

    return CORE_OK;
}

status_t net_final(void)
{
    /* Finalize network connection pool */
    pool_final(&net_pool);
    /* Finalize ftp connection pool */
    pool_final(&ftp_pool);
    /* Finalize network connection pool */
    pool_final(&link_pool);
    /* Finalize network fd pool */
    pool_final(&net_fd_pool);

    mutex_delete(g_net_fd_tbl.mut);

    return CORE_OK;
}

int net_pool_avail()
{
    return pool_avail(&net_pool);
}

/** Allocate socket from network pool and create it */
static net_sock_t *net_sock_create(int type, int protocol)
{
    int rc,sock, sockopt;
    net_sock_t *net_sock = NULL;

    pool_alloc_node(&net_pool, &net_sock);
    d_assert(net_sock != NULL, return NULL,"No net pool is availabe\n");

    /* Create stream socket */
    sock = socket(PF_INET, type, protocol);
    if (sock < 0)
    {
        d_error("Can not create socket(type: %d protocol: %d)",type,protocol);
        return NULL;
    }

    /* Set the useful socket option */

    /* Don't leave the socket in a TIME_WAIT state if we close the
     * connection
     */
#if 0
    fix_ling.l_onoff = 1;
    fix_ling.l_linger = 0;
    rc = setsockopt(sock, SOL_SOCKET, SO_LINGER, &fix_ling,
            sizeof(fix_ling));
    if (rc < 0)
    {
        goto cleanup;
    }
#endif

    /* Reuse the binded address */
    sockopt = 1;
    rc = setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &sockopt,
            sizeof(sockopt));
    if (rc < 0)
    {
        d_error("setsockopt error(SO_REUSEADDR)");
        goto cleanup;
    }

    if (protocol == IPPROTO_SCTP) 
    {
        struct sctp_event_subscribe event;
        struct sctp_paddrparams heartbeat;
        struct sctp_rtoinfo rtoinfo;
        struct sctp_initmsg initmsg;
        socklen_t socklen;

        memset(&event, 0, sizeof(event));
        memset(&heartbeat, 0, sizeof(heartbeat));
        memset(&rtoinfo, 0, sizeof(rtoinfo));
        memset(&initmsg, 0, sizeof(initmsg));

        event.sctp_association_event = 1;
        event.sctp_send_failure_event = 1;
        event.sctp_shutdown_event = 1;

#if USE_USRSCTP == 1
        if (setsockopt(sock, IPPROTO_SCTP, SCTP_EVENT,
                                &event, sizeof( event)) != 0 ) 
#else
        if (setsockopt(sock, IPPROTO_SCTP, SCTP_EVENTS,
                                &event, sizeof( event)) != 0 ) 
#endif
        {
            d_error("Unable to subscribe to SCTP events: (%d:%s)",
                    errno, strerror( errno ));
            goto cleanup;
        }

        socklen = sizeof(heartbeat);
        if (getsockopt(sock, IPPROTO_SCTP, SCTP_PEER_ADDR_PARAMS,
                                &heartbeat, &socklen) != 0 ) 
        {
            d_error("Failure :  getsockopt for SCTP_PEER_ADDR: (%d:%s)",
                    errno, strerror( errno ));
            goto cleanup;
        }

        d_trace(3,"Old spp _flags = 0x%x hbinter = %d pathmax = %d\n",
                heartbeat.spp_flags,
                heartbeat.spp_hbinterval,
                heartbeat.spp_pathmaxrxt);

        /* FIXME : Need to configure this param */
        heartbeat.spp_hbinterval = 5000; /* 5 secs */

        if (setsockopt(sock, IPPROTO_SCTP, SCTP_PEER_ADDR_PARAMS,
                                &heartbeat, sizeof( heartbeat)) != 0 ) 
        {
            d_error("Failure : setsockopt for SCTP_PEER_ADDR_PARAMS: (%d:%s)",
                    errno, strerror( errno ));
            goto cleanup;
        }
        d_trace(3,"New spp _flags = 0x%x hbinter = %d pathmax = %d\n",
                heartbeat.spp_flags,
                heartbeat.spp_hbinterval,
                heartbeat.spp_pathmaxrxt);

        socklen = sizeof(rtoinfo);
        if (getsockopt(sock, IPPROTO_SCTP, SCTP_RTOINFO,
                                &rtoinfo, &socklen) != 0 ) 
        {
            d_error("Failure :  getsockopt for SCTP_RTOINFO: (%d:%s)",
                    errno, strerror( errno ));
            goto cleanup;
        }

        d_trace(3,"Old RTO (initial:%d max:%d min:%d)\n",
                rtoinfo.srto_initial,
                rtoinfo.srto_max,
                rtoinfo.srto_min);

        /* FIXME : Need to configure this param */
        rtoinfo.srto_initial = 3000;
        rtoinfo.srto_min = 1000;
        rtoinfo.srto_max = 5000;

        if (setsockopt(sock, IPPROTO_SCTP, SCTP_RTOINFO,
                                &rtoinfo, sizeof(rtoinfo)) != 0 ) 
        {
            d_error("Failure : setsockopt for SCTP_RTOINFO: (%d:%s)",
                    errno, strerror( errno ));
            goto cleanup;
        }
        d_trace(3,"New RTO (initial:%d max:%d min:%d)\n",
                rtoinfo.srto_initial,
                rtoinfo.srto_max,
                rtoinfo.srto_min);

        socklen = sizeof(initmsg);
        if (getsockopt(sock, IPPROTO_SCTP, SCTP_INITMSG,
                                &initmsg, &socklen) != 0 ) 
        {
            d_error("Failure :  getsockopt for SCTP_INITMSG: (%d:%s)",
                    errno, strerror( errno ));
            goto cleanup;
        }

        d_trace(3,"Old INITMSG (numout:%d maxin:%d maxattempt:%d maxinit_to:%d)\n",
                    initmsg.sinit_num_ostreams,
                    initmsg.sinit_max_instreams,
                    initmsg.sinit_max_attempts,
                    initmsg.sinit_max_init_timeo);

        /* FIXME : Need to configure this param */
        initmsg.sinit_max_attempts = 4;
        initmsg.sinit_max_init_timeo = 8000; /* 8secs */

        if (setsockopt(sock, IPPROTO_SCTP, SCTP_INITMSG,
                                &initmsg, sizeof(initmsg)) != 0 ) 
        {
            d_error("Failure : setsockopt for SCTP_INITMSG: (%d:%s)",
                    errno, strerror( errno ));
            goto cleanup;
        }

        d_trace(3,"New INITMSG (numout:%d maxin:%d maxattempt:%d maxinit_to:%d)\n",
                    initmsg.sinit_num_ostreams,
                    initmsg.sinit_max_instreams,
                    initmsg.sinit_max_attempts,
                    initmsg.sinit_max_init_timeo);
    }

    /* Set socket descriptor */
    net_sock->sock_id = sock;

cleanup:
    if (rc < 0)
    {
        close(sock);
        pool_free_node(&net_pool, net_sock);
    }

    return net_sock;
}

static void net_sock_delete(net_sock_t *net_sock)
{
    pool_free_node(&net_pool, net_sock);
    return;
}

/** Resolve host name */
int net_resolve_host(const char *host, struct in_addr *addr)
{
    int rc;
    /* FIXME : Resolve host by using DNS */
    rc = inet_aton(host, addr);
    return rc;
}

int net_open_ext(net_sock_t **net_sock, 
                 const c_uint32_t local_addr,
                 const char *remote_host,
                 const int lport,
                 const int rport,
                 int type, int proto, c_uint32_t ppid, const int flag)
{
    struct sockaddr_in sock_addr;
    int rc;
    net_sock_t *result_sock = NULL;
    socklen_t addr_len;

    d_assert(net_sock, return -1, "net_sock is NULL\n");

    result_sock = net_sock_create(type, proto);
    if (result_sock == NULL)
    {
        return -1;
    }

    /* FIXME : Set socket option */
    memset(&sock_addr, 0, sizeof(sock_addr));
    sock_addr.sin_family = AF_INET;
    sock_addr.sin_port = htons(rport);

    /* Resolve host(it must be hostname or valid IP) */
    if (!net_resolve_host(remote_host, &sock_addr.sin_addr))
    { 
        goto cleanup;
    }

    result_sock->type = type;
    result_sock->proto = proto;
    result_sock->ppid = ppid;

    /* Connect to host */
    if (proto == IPPROTO_UDP || 
            (proto == IPPROTO_SCTP && (lport != 0 || local_addr)))
    {
        struct sockaddr_in cli_addr;

        memset(&cli_addr, 0, sizeof(cli_addr));
        cli_addr.sin_family = AF_INET;
        cli_addr.sin_addr.s_addr = local_addr;
        cli_addr.sin_port = htons(lport);

        if (bind(result_sock->sock_id, (struct sockaddr *)&cli_addr,
                    sizeof(cli_addr)) < 0)
        {
            d_error("bind error(proto:%d lport:%d)",proto,lport);
            goto cleanup;
        }
    }

    if (proto == IPPROTO_TCP ||
        (proto == IPPROTO_SCTP && type == SOCK_STREAM)) 
    {
        rc = connect(result_sock->sock_id, 
                (struct sockaddr *)&sock_addr, sizeof(sock_addr));
        if (rc < 0)
        {
            d_error("connect error(%d:%s)(proto:%d remote:%s dport:%d lport:%d)",
                    errno,
                    strerror(errno),
                    proto,
                    remote_host,
                    rport,
                    lport);
            goto cleanup;
        }
        addr_len = sizeof(result_sock->remote);
        if (getpeername(result_sock->sock_id, 
                (struct sockaddr *)&result_sock->remote, &addr_len) != 0)
        {
            d_warn("getpeername error = %d\n",errno);
        }
    }
    else
    {
        memcpy(&result_sock->remote, &sock_addr, sizeof(sock_addr));
    }

#if 0 /* deprecated */
    addr_len = sizeof(result_sock->local);
    if (getsockname(result_sock->sock_id, 
                (struct sockaddr *)&result_sock->local, &addr_len)
            != 0)
    {
        d_warn("getsockname error = %d\n",errno);
    }
#endif

    *net_sock = result_sock;

    return 0;

cleanup:
    net_close(result_sock);
    return -1;
}

/** Create TCP/UDP socket and open it */
int net_open(net_sock_t **net_sock, const char *host, 
                 const int lport,
                 const int rport,
                 int type, int proto)
{
    return net_open_ext(net_sock, 
                 0,
                 host, 
                 lport,
                 rport,
                 type, proto, 0, 0);
}

/** Read data from socket */
int net_read(net_sock_t *net_sock, char *buffer, size_t size, int timeout)
{
    fd_set rfds;
    struct timeval tv;
    int rc;

    d_assert(net_sock, return -1, "net_sock is NULL\n");

    FD_ZERO(&rfds);
    FD_SET(net_sock->sock_id, &rfds);

    /* Set timeout */
    if (timeout > 0)
    {
        tv.tv_sec = timeout;
        tv.tv_usec = 0;
    }

    rc = select(net_sock->sock_id + 1, &rfds, NULL, NULL,
                    (timeout ? &tv : NULL));
    if (rc < 0) net_sock->sndrcv_errno = errno;

    if (rc == -1) /* Error */
    {
        return -1;
    }

    else if (rc) /* Data received */
    {
        if (net_sock->proto == IPPROTO_TCP)
        {
            rc = recv(net_sock->sock_id, buffer, size, 0);
            if (rc < 0) net_sock->sndrcv_errno = errno;
        }
        else if (net_sock->proto == IPPROTO_UDP)
        {
            struct sockaddr remote_addr;
            socklen_t addr_len = sizeof(struct sockaddr);

            rc = recvfrom(net_sock->sock_id, buffer, size, 0,
                    &remote_addr, &addr_len);
            if (rc < 0) net_sock->sndrcv_errno = errno;

            /* Save the remote address */
            memcpy(&net_sock->remote, &remote_addr, sizeof(remote_addr));
        }
        else if (net_sock->proto == IPPROTO_SCTP)
        {
            int flags = 0;
            struct sockaddr remote_addr;
            socklen_t addr_len = sizeof(struct sockaddr);
#if USE_USRSCTP == 1
            rc = usrsctp_recvv((struct socket *)net_sock, buffer, size,
                        (struct sockaddr *)&remote_addr, &addr_len, 
                        NULL, NULL, NULL, &flags);
#else
            struct sctp_sndrcvinfo sndrcvinfo;

            rc = sctp_recvmsg(net_sock->sock_id, buffer, size,
                        (struct sockaddr *)&remote_addr, &addr_len, 
                        &sndrcvinfo, &flags);
#endif
            if (rc < 0) net_sock->sndrcv_errno = errno;

            /* Save the remote address */
            if (net_sock->type == SOCK_SEQPACKET)
            {
                memcpy(&net_sock->remote, &remote_addr, sizeof(remote_addr));
            }

            if (flags & MSG_NOTIFICATION) 
            {
                if (flags & MSG_EOR) 
                {
                    union sctp_notification *not = 
                        (union sctp_notification *)buffer;

                    switch( not->sn_header.sn_type ) 
                    {
                        case SCTP_ASSOC_CHANGE :
                            d_trace(3, "SCTP_ASSOC_CHANGE"
                                    "(type:0x%x, flags:0x%x, state:0x%x)\n", 
                                    not->sn_assoc_change.sac_type,
                                    not->sn_assoc_change.sac_flags,
                                    not->sn_assoc_change.sac_state);

                            net_sock->sndrcv_errno = EAGAIN;
                            if (not->sn_assoc_change.sac_state == SCTP_COMM_UP)
                                return -2;
                            if (not->sn_assoc_change.sac_state == 
                                    SCTP_SHUTDOWN_COMP ||
                                not->sn_assoc_change.sac_state == 
                                    SCTP_COMM_LOST)
                                net_sock->sndrcv_errno = ECONNREFUSED;
                            break;
#if USE_USRSCTP != 1
                        case SCTP_SEND_FAILED :
                            d_error("SCTP_SEND_FAILED"
                                    "(type:0x%x, flags:0x%x, error:0x%x)\n", 
                                    not->sn_send_failed.ssf_type,
                                    not->sn_send_failed.ssf_flags,
                                    not->sn_send_failed.ssf_error);
                            break;
#endif
                        case SCTP_SHUTDOWN_EVENT :
                            d_trace(3, "SCTP_SHUTDOWN_EVENT\n");
                            net_sock->sndrcv_errno = ECONNREFUSED;
                            break;
                        default :
                            net_sock->sndrcv_errno = EAGAIN;
                            d_error("Discarding event with unknown "
                                    "flags = 0x%x, type 0x%x", 
                                    flags, not->sn_header.sn_type);
                            break;
                    }
                }
                else
                {
                    d_error("Not engough buffer. Need more recv : 0x%x", flags);
                }
                return -1;
            }
        }
        else
        {
            return -1;
        }
    }
    else /* Timeout */
    {
        return 0;
    }

    return rc;
}

/** Write data into socket */
int net_write(net_sock_t *net_sock, char *buffer, size_t size,
        struct sockaddr_in *dest_addr, int addrsize)
{
    char ip_addr[INET_ADDRSTRLEN];
    int rc;

    d_assert(net_sock, return -1, "net_sock is NULL\n");

    d_trace(3,"%s)Send %d bytes to %s:%d(%s)\n",
            net_sock->proto == IPPROTO_TCP ? "TCP" : 
            net_sock->proto == IPPROTO_UDP ? "UDP" : "SCTP",
            size, INET_NTOP(&dest_addr->sin_addr, ip_addr),
            ntohs(dest_addr->sin_port),
            dest_addr->sin_family == AF_INET ? "AF_INET" : "Invalid");

    if (net_sock->proto == IPPROTO_TCP)
    {
        rc = send(net_sock->sock_id, buffer, size, 0);
        if (rc < 0) net_sock->sndrcv_errno = errno;

        return rc;
    }
    else if (net_sock->proto == IPPROTO_UDP)
    {
        rc =  sendto(net_sock->sock_id, buffer, size, 0,
                (struct sockaddr *)dest_addr, addrsize);
        if (rc < 0) net_sock->sndrcv_errno = errno;

        return rc;
    }
    else if (net_sock->proto == IPPROTO_SCTP)
    {
#if USE_USRSCTP != 1
        rc = sctp_sendmsg(net_sock->sock_id, buffer, size,
                (struct sockaddr *)dest_addr, addrsize,
                htonl(net_sock->ppid), 
                0,  /* flags, FIXME : SCTP_ADDR_OVER is needed? */
                0,  /* stream_no */
                0,  /* timetolive */
                0); /* context */
        if (rc < 0) net_sock->sndrcv_errno = errno;

        return rc;
#endif
    }

    return -1;
}

int net_send(net_sock_t *net_sock, char *buffer, size_t size)
{
    d_assert(net_sock && buffer, return -1, "Invalid params\n");

    return net_write(net_sock, buffer, size, 
                &net_sock->remote, sizeof(net_sock->remote));
}

int net_sendto(net_sock_t *net_sock, char *buffer, size_t size,
        c_uint32_t addr, c_uint16_t port)
{
    struct sockaddr_in sock_addr;
    d_assert(net_sock && buffer, return -1, "Invalid params\n");

    memset(&sock_addr, 0, sizeof(sock_addr));
    sock_addr.sin_family = AF_INET;
    sock_addr.sin_port = htons(port);
    sock_addr.sin_addr.s_addr = addr;

    return net_write(net_sock, buffer, size, &sock_addr, sizeof(sock_addr));
}

/** Close the socket */
int net_close(net_sock_t *net_sock)
{
    int rc;

    d_assert(net_sock, return -1, "net_sock is NULL\n");

    rc = close(net_sock->sock_id);
    net_sock_delete(net_sock);
    return rc;
}

/** Accept the new connection.This function will allocation new session
 *  from network pool
 */
int  net_accept(net_sock_t **new_accept_sock, net_sock_t *net_sock, int timeout)
{
    fd_set rfds;
    struct timeval tv;
    int new_sock;
    int rc;
    int sock;
    socklen_t addr_len;

    sock = net_sock->sock_id;

    FD_ZERO(&rfds);
    FD_SET(sock, &rfds);

    if (timeout > 0)
    {
        tv.tv_sec = timeout;
        tv.tv_usec = 0;
    }

    rc = select(sock+1, &rfds, NULL, NULL, (timeout ? &tv : NULL));
    if (rc == -1)
    {
        /* Error */
        goto cleanup;
    }

    if (rc == 0) /* Timeout */
    {
        goto cleanup;
    }
    else
    {
        if (FD_ISSET(sock, &rfds))
        {
            net_sock_t *node = NULL;
            pool_alloc_node(&net_pool, &node);
            new_sock = accept(sock, NULL, NULL);

            node->sock_id = new_sock;
            node->proto = net_sock->proto;
            node->ppid = net_sock->ppid;
            *new_accept_sock = node;

            /* Save local and remote address */
            addr_len = sizeof(node->remote);
            if (getpeername(node->sock_id, 
                        (struct sockaddr *)&node->remote, &addr_len) != 0)
            {
                d_warn("getpeername error = %d\n",errno);
            }

#if 0 /* deprecated */
            addr_len = sizeof(node->local);
            if (getsockname(node->sock_id, 
                        (struct sockaddr *)&node->local, &addr_len) != 0)
            {
                d_warn("getsockname error = %d\n",errno);
            }
#endif
        }
        else
        {
            rc = -1;
        }
    }

cleanup:
    return rc;
}

/** Listen connection */
int net_listen_ext(net_sock_t **net_sock, 
        const int type, const int proto, c_uint32_t ppid,
        const c_uint32_t addr, const int port)
{
    struct sockaddr_in sock_addr;
    net_sock_t *result_sock = NULL;

    /* Create socket */
    result_sock = net_sock_create(type, proto);
    if (result_sock == NULL)
    {
        return -1;
    }

    memset(&sock_addr, 0, sizeof(sock_addr));
    sock_addr.sin_family = AF_INET;
    sock_addr.sin_port = htons(port);
    sock_addr.sin_addr.s_addr = addr;

    if (bind(result_sock->sock_id, (const void *)&sock_addr,
                sizeof(sock_addr)) < 0)
    {
       goto cleanup;
    }

    if (proto == IPPROTO_TCP || proto == IPPROTO_SCTP)
    {
        if (listen(result_sock->sock_id, 5) < 0)
        {
            goto cleanup;
        }
    }

    result_sock->type = type;
    result_sock->proto = proto;
    result_sock->ppid = ppid;

    *net_sock = result_sock;

    return 0;

cleanup:
    net_close(result_sock);
    return -1;
}

int net_listen(
        net_sock_t **net_sock, const int type, const int proto,
        const int port)
{
    return net_listen_ext(net_sock, type, proto, 0, INADDR_ANY, port);
}


/******************************************************************************
 * FTP Library
******************************************************************************/
static int net_ftp_readline(net_sock_t *net_sock, char *buf, int buf_size)
{
    int cnt = 0;
    int rc;
    char c;

    while (1)
    {
        rc = net_read(net_sock, &c, 1, 0);
        if (rc != 1)
        {
            /* Error */
        }

        if ( c == '\n')
        {
            break;
        }

        if (cnt < buf_size)
        {
            buf[cnt++] = c;
        }
    }

    if (cnt < buf_size)
    {
        buf[cnt] = '\0';
    }
    else
    {
        buf[buf_size-1] = '\0';
    }

    d_trace(1, "RX:%s\n",buf);

    return cnt;
}

static int net_ftp_get_reply(net_ftp_t *session)
{
    int more = 0;
    int rc ;
    int first_line = 1;
    int code = 0;

    d_assert(session, return -1, "Invalid session\n");

    do
    {
        if ((rc = net_ftp_readline(session->ctrl_sock, session->resp_buf,
                        sizeof(session->resp_buf))) < 0)
        {
            return rc;
        }
        if (first_line)
        {
            code = strtoul(session->resp_buf, NULL, 0);
            first_line = 0;
            more = (session->resp_buf[3] == '-');
        }
        else
        {
            if (isdigit(session->resp_buf[0]) && isdigit(session->resp_buf[1])
                  && isdigit(session->resp_buf[2]) &&
                  (code == strtoul(session->resp_buf, NULL, 0)) &&
                  session->resp_buf[3] == ' ')
            {
                more = 0;
            }
            else
            {
                more = 1;
            }
        }

    } while (more);

    return (session->resp_buf[0] - '0');
}

static int net_ftp_send_cmd(net_ftp_t *session)
{
    int rc;

    d_assert(session, return -1, "Invalid session\n");
    d_trace(1,"TX:%s\n",session->cmd_buf);
    rc = net_send(session->ctrl_sock, session->cmd_buf,
            strlen(session->cmd_buf));
    if (rc != strlen(session->cmd_buf))
    {
        d_error("FTP : net_ftp_send_cmd error\n");
        return -1;
    }
    rc = net_ftp_get_reply(session);
    return rc;
}

int net_ftp_open(const char *host,
                 const char *username,
                 const char *passwd,
                 int flag,
                 net_ftp_t **ftp_session)
{
    int port = 21; /* default ftp port */
    int rc = -1;
    net_ftp_t *session = NULL;
    char *ptr = NULL;

    d_assert(ftp_session, return -1, "Invalid ftp sesssion\n");
    *ftp_session = NULL;

    pool_alloc_node(&ftp_pool, &session);
    d_assert(session != NULL, return -1,"No ftp pool is availabe\n");

    memset(session, 0, sizeof(net_ftp_t));

    /* Check if port number is given.
     * ex: 192.168.1.1:1111
     */
    if ((ptr = strchr(host,':')) != NULL)
    {
        *ptr++ = '\0';
        port = atoi(ptr);
    }

    /* Open control channel */
    rc = net_open(&session->ctrl_sock, host, 0, port, 
            SOCK_DGRAM, IPPROTO_TCP);
    if (rc != 0)
    {
        d_error("net_open error(errno = %d) : host = %s, port = %d\n",
                errno,
                host,port);
        pool_free_node(&ftp_pool, session);
        return -1;
    }

    /* Read welcome messages */
    net_ftp_get_reply(session);

    /* Login */
    sprintf(session->cmd_buf,"USER %s\r\n", username ? username : "anonymous");
    rc = net_ftp_send_cmd(session);
    if (rc != 3)
    {
        d_error("FTP : User %s not accepted\n", username);
        goto cleanup;
    }
    sprintf(session->cmd_buf,"PASS %s\r\n", passwd ? passwd : "taiji@");
    rc = net_ftp_send_cmd(session);
    if (rc !=2)
    {
        d_error("FTP : Login failed for user %s\n",
                username ? username : "anonymous");
        goto cleanup;
    }

    /* Ftp login success */
    sprintf(session->cmd_buf,"TYPE I\r\n");
    rc = net_ftp_send_cmd(session);
    if (rc !=2 )
    {
        d_error("FTP : TYPE failed\n");
        goto cleanup;
    }
    session->flag = flag;
    *ftp_session = session;

    return 0;

cleanup:
    pool_free_node(&ftp_pool, session);
    net_close(session->ctrl_sock);

    return -1;
}


static int net_ftp_opendata(net_ftp_t *ftp_session)
{
    char *cp = NULL;
    int port = 0;
    int rc;
    char ip_addr[INET_ADDRSTRLEN];

    d_assert(ftp_session, return -1, "Invalid session\n");

    /* FIXME: Passive or Active */
    sprintf(ftp_session->cmd_buf,"PASV\r\n");
    rc = net_ftp_send_cmd(ftp_session);
    if (rc != 2)
    {
        d_error("FTP : PASV failed\n");
        return -1;
    }
    /* Response is like
     * 227 Entering Passive Mode (0,0,0,0,p1,p2)
     * Server's port for data connection is p1*256 + p2
     */
    cp = strchr(ftp_session->resp_buf, '(');
    if (cp)
    {
        unsigned int v[6];

        sscanf(cp,"(%u,%u,%u,%u,%u,%u)",&v[0],&v[1],&v[2],&v[3],&v[4],&v[5]);
        port = v[4]*256 + v[5];
    }

    /* Open control channel */
    rc = net_open(&ftp_session->data_sock,
            INET_NTOP(&ftp_session->ctrl_sock->remote.sin_addr, ip_addr),
            0,
            port,
            SOCK_STREAM, IPPROTO_TCP);
    if (rc != 0)
    {
        d_error("net_open error in net_ftp_opendata(host = %s,port = %d)\n",
                INET_NTOP(&ftp_session->ctrl_sock->remote.sin_addr, ip_addr), 
                port);
        return -1;
    }

    return 0;
}

int  net_ftp_get(net_ftp_t *ftp_session,
                 const char *remote_filename,
                 const char *local_filename)
{
    int rc;
    int len;
    int local_fd;
    int total_size = 0;
    char buf[512];
    char *l_filename = NULL;

    d_assert(ftp_session, return -1, "Invalid session\n");

    d_assert(remote_filename, return -1,
            "Invalid filename.It should not be NULL\n");

    /* Open data channel */
    rc = net_ftp_opendata(ftp_session);
    if (rc != 0)
    {
        return -1;
    }

    sprintf(ftp_session->cmd_buf,"RETR %s\r\n",remote_filename);
    rc = net_ftp_send_cmd(ftp_session);
    if (rc != 1)
    {
        d_error("FTP : RETR %s failed\n",remote_filename);
        return -1;
    }

    /* Open local file descriptor */
    if (local_filename == NULL)
    {
        /* Strip leading '/' if exist */
        l_filename = strrchr(remote_filename,'/');
        if (l_filename)
            l_filename++;
        else
            l_filename = (char *)remote_filename;
    }
    else
    {
        l_filename = (char *)local_filename;
    }

    local_fd = open(l_filename, O_RDWR|O_CREAT, 0644);
    if (local_fd == -1)
    {
        d_error("FTP : local open error(filename = %s)\n",local_filename);
        return -1;
    }

    do {
        len = net_read(ftp_session->data_sock, buf, 512, 0);
        if (len > 0)
        {
            /* Write to file */
            write(local_fd, buf, len);
        }
        else
        {
            break;
        }
        total_size += len;
    } while (1);

    d_trace(1,"Receive completed (bytes = %d)\n",total_size);

    rc = net_ftp_get_reply(ftp_session);
    close(local_fd);

    net_close(ftp_session->data_sock);
    ftp_session->data_sock = NULL;

    return 0;
}

int  net_ftp_put(net_ftp_t *ftp_session,
                 const char *local_filename,
                 const char *remote_filename)
{
    int rc;
    int len;
    int local_fd;
    int total_size = 0;
    char buf[512];
    char *r_filename = NULL;

    d_assert(ftp_session, return -1, "Invalid session\n");

    d_assert(local_filename, return -1,
            "Invalid filename.It should not be NULL\n");

    /* Open data channel */
    rc = net_ftp_opendata(ftp_session);
    if (rc != 0)
    {
        return -1;
    }

    if (remote_filename == NULL)
    {
        r_filename = strrchr(local_filename, '/');
        if (r_filename)
            r_filename++;
        else
            r_filename = (char *)local_filename;
    }
    else
    {
        r_filename = (char *)remote_filename;
    }

    sprintf(ftp_session->cmd_buf,"STOR %s\r\n",r_filename);
    rc = net_ftp_send_cmd(ftp_session);
    if (rc != 1)
    {
        d_error("FTP : STOR %s failed\n",remote_filename);
        return -1;
    }

    /* Open local file descriptor */
    local_fd = open(local_filename, O_RDONLY, 0);
    if (local_fd == -1)
    {
        d_error("FTP : local open error(filename = %s)\n",local_filename);
        return -1;
    }

    do {
        len = read(local_fd, buf, 512);
        if (len > 0)
        {
            len = net_send(ftp_session->data_sock, buf, len);
        }
        else
        {
            break;
        }
        total_size += len;
    } while (1);
    net_close(ftp_session->data_sock);
    ftp_session->data_sock = NULL;
    d_trace(1,"Trasnsmit completed (bytes = %d)\n",total_size);
    rc = net_ftp_get_reply(ftp_session);
    close(local_fd);

    return 0;
}

int net_ftp_close(net_ftp_t *ftp_session)
{
    d_assert(ftp_session, return -1, "Invalid session\n");

    if (ftp_session->ctrl_sock)
    {
        net_close(ftp_session->ctrl_sock);
    }
    if (ftp_session->data_sock)
    {
        net_close(ftp_session->data_sock);
    }
    pool_free_node(&ftp_pool, ftp_session);
    return 0;
}

int net_ftp_quit(net_ftp_t *ftp_session)
{
    int rc;
    d_assert(ftp_session, return -1, "Invalid session\n");

    sprintf(ftp_session->cmd_buf,"QUIT\r\n");
    rc = net_ftp_send_cmd(ftp_session);
    if (rc != 2)
    {
        d_error("FTP : QUIT failed\n");
        return -1;
    }
    return 0;
}


/******************************************************************************
 * Network link (raw socket)
******************************************************************************/

int net_raw_open(net_link_t **net_link, int proto)
{
    int sock;
    net_link_t *new_link = NULL;

    d_assert(net_link, return -1, "Invalid arguments\n");

    d_assert(getuid() == 0 || geteuid() == 0 , return -1,
            "ROOT privileges required to open the interface\n");

    pool_alloc_node(&link_pool, &new_link);
    d_assert(new_link != NULL, return -1,"No link pool is availabe\n");

    memset(new_link, 0, sizeof(net_link_t));
    /* Create raw socket */
    sock = socket(PF_INET, SOCK_RAW, proto);

    if (sock < 0)
    {
        return -1;
    }

    new_link->fd = sock;

    *net_link = new_link;
    return 0;
}

/** Create tuntap socket */
#if 0 /* deprecated */
int net_tuntap_open(net_link_t **net_link, char *tuntap_dev_name, 
        int is_tap)
{
    int sock;
    net_link_t *new_link = NULL;
#if LINUX == 1
    char *dev = "/dev/net/tun";
    int rc;
    struct ifreq ifr;
    int flags = IFF_NO_PI;
#else
    char *dev = "/dev/tun0";
#endif

    sock = open(dev, O_RDWR);
    if (sock < 0)
    {
        d_error("Can not open %s",dev);
        return -1;
    }

    pool_alloc_node(&link_pool, &new_link);
    d_assert(new_link != NULL, return -1,"No link pool is availabe\n");

#if LINUX == 1
    memset(&ifr, 0, sizeof(ifr));

    ifr.ifr_flags = (is_tap ? (flags | IFF_TAP) :  (flags | IFF_TUN));
    strncpy(ifr.ifr_name, tuntap_dev_name, IFNAMSIZ);

    rc = ioctl(sock, TUNSETIFF, (void *)&ifr);
    if (rc < 0)
    {
        d_error("iotcl error(dev:%s flags = %d)", tuntap_dev_name, flags);
        goto cleanup;
    }
#endif

    /* Save socket descriptor */
    new_link->fd = sock;
    /* Save the interface name */
    strncpy(new_link->ifname, tuntap_dev_name, IFNAMSIZ);

    *net_link = new_link;
    return 0;

#if LINUX == 1
cleanup:
    pool_free_node(&link_pool, new_link);
    close(sock);
    return -1;
#endif
}
#endif

/** Create tun socket */
int net_tun_open(net_link_t **net_link, char *ifname, int is_tap)
{
    int sock;
    net_link_t *new_link = NULL;
#if LINUX == 1
    char *dev = "/dev/net/tun";
    int rc;
    struct ifreq ifr;
    int flags = IFF_NO_PI;

    sock = open(dev, O_RDWR);
    if (sock < 0)
    {
        d_error("Can not open %s",dev);
        return -1;
    }
#else
    char name[C_PATH_MAX];
    int tun = 0;

#define TUNTAP_ID_MAX 255
    for (tun = 0; tun < TUNTAP_ID_MAX; tun++)
    {
        (void)snprintf(name, sizeof(name), "/dev/tun%i", tun);
        if ((sock = open(name, O_RDWR)) > 0)
        {
            (void)snprintf(name, sizeof(name), "tun%i", tun);
            ifname = name;
            break;
        }
    }
#endif

    pool_alloc_node(&link_pool, &new_link);
    d_assert(new_link != NULL, return -1,"No link pool is availabe\n");

#if LINUX == 1
    memset(&ifr, 0, sizeof(ifr));

    ifr.ifr_flags = (is_tap ? (flags | IFF_TAP) : (flags | IFF_TUN));
    strncpy(ifr.ifr_name, ifname, IFNAMSIZ);

    rc = ioctl(sock, TUNSETIFF, (void *)&ifr);
    if (rc < 0)
    {
        d_error("iotcl error(dev:%s flags = %d)", ifname, flags);
        goto cleanup;
    }
#endif

    /* Save socket descriptor */
    new_link->fd = sock;
    /* Save the interface name */
    strncpy(new_link->ifname, ifname, IFNAMSIZ);

    *net_link = new_link;
    return 0;

#if LINUX == 1
cleanup:
    pool_free_node(&link_pool, new_link);
    close(sock);
    return -1;
#endif
}

int net_tun_set_ipv4(net_link_t *link, c_uint32_t ip_addr, c_uint8_t bits)
{
#if LINUX != 1
    int sock;
    
	struct ifaliasreq ifa;
	struct ifreq ifr;
	struct sockaddr_in addr;
	struct sockaddr_in mask;
    c_uint32_t mask_addr = htonl(0xffffffff << (32 - bits));

    char buf[512];
    int len;
    struct rt_msghdr *rtm;
    struct sockaddr_in dst, gw;
	struct sockaddr_in *paddr;

    sock = socket(AF_INET, SOCK_DGRAM, 0);

	(void)memset(&ifa, '\0', sizeof ifa);
	(void)strlcpy(ifa.ifra_name, link->ifname, sizeof ifa.ifra_name);

	(void)memset(&ifr, '\0', sizeof ifr);
	(void)strlcpy(ifr.ifr_name, link->ifname, sizeof ifr.ifr_name);

	/* Delete previously assigned address */
	(void)ioctl(sock, SIOCDIFADDR, &ifr);

	(void)memset(&addr, '\0', sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = ip_addr;
	addr.sin_len = sizeof(addr);
	(void)memcpy(&ifa.ifra_addr, &addr, sizeof(addr));
	(void)memcpy(&ifa.ifra_broadaddr, &addr, sizeof(addr));

	(void)memset(&mask, '\0', sizeof(mask));
	mask.sin_family = AF_INET;
	mask.sin_addr.s_addr = 0xffffffff;
	mask.sin_len = sizeof(mask);
	(void)memcpy(&ifa.ifra_mask, &mask, sizeof(ifa.ifra_mask));

	if (ioctl(sock, SIOCAIFADDR, &ifa) == -1) {
		d_error("Can't IP address(dev:%s err:%s)",
                link->ifname, strerror(errno));
		return -1;
	}

    close(sock); /* AF_INET, SOCK_DGRAM */

    sock = socket(PF_ROUTE, SOCK_RAW, 0);
    if (sock < 0)
    {
        d_error("Can't open PF_ROUTE(%s)", strerror(errno));
        return -1;
    }

    (void)memset(&buf, 0, sizeof(buf));
    rtm = (struct rt_msghdr *)buf;
    rtm->rtm_type = RTM_ADD;
    rtm->rtm_version = RTM_VERSION;
    rtm->rtm_pid = getpid();
    rtm->rtm_seq = 0;
    rtm->rtm_flags = RTF_UP | RTF_GATEWAY;
    rtm->rtm_addrs = RTA_DST | RTA_GATEWAY | RTA_NETMASK;
    paddr = (struct sockaddr_in *)(rtm + 1);

	(void)memset(&dst, '\0', sizeof(dst));
	dst.sin_family = AF_INET;
	dst.sin_addr.s_addr = ip_addr & mask_addr;
	dst.sin_len = sizeof(dst);
	(void)memcpy(paddr, &dst, sizeof(dst));
    paddr = (struct sockaddr_in *)((char *)paddr +
            CORE_ALIGN(sizeof(*paddr), sizeof(c_uintptr_t)));

	(void)memset(&gw, '\0', sizeof(gw));
	gw.sin_family = AF_INET;
	gw.sin_addr.s_addr = ip_addr;
	gw.sin_len = sizeof(gw);
	(void)memcpy(paddr, &gw, sizeof(gw));
    paddr = (struct sockaddr_in *)((char *)paddr +
            CORE_ALIGN(sizeof(*paddr), sizeof(c_uintptr_t)));

	(void)memset(&mask, '\0', sizeof(mask));
	mask.sin_family = AF_INET;
	mask.sin_addr.s_addr = mask_addr;
	mask.sin_len = sizeof(mask);
	(void)memcpy(paddr, &mask, sizeof(mask));
    paddr = (struct sockaddr_in *)((char *)paddr +
            CORE_ALIGN(sizeof(*paddr), sizeof(c_uintptr_t)));

    len = (char*)paddr - buf;
    rtm->rtm_msglen = len;
    if (write(sock, buf, len) < 0)
    {
        d_error("Can't add routing(%s)", strerror(errno));
        return -1;
    }

    close(sock); /* PF_ROUTE, SOCK_RAW */

#endif  /* LINUX == 1 */

	return 0;
}

#if LINUX == 1
int net_link_open(net_link_t **net_link, char *device, int proto)
{
    int sock,ioctl_sock;
    net_link_t *new_link = NULL;
    struct ifreq ifr;
    struct sockaddr_ll sll;

    d_assert(net_link && device, return -1, "Invalid arguments\n");

    d_assert(getuid() == 0 || geteuid() == 0 , return -1,
            "ROOT privileges required to open the interface\n");

    pool_alloc_node(&link_pool, &new_link);
    d_assert(new_link != NULL, return -1,"No link pool is availabe\n");

    memset(new_link, 0, sizeof(net_link_t));
    /* Create raw socket */
#if 0
    sock = socket(PF_INET, SOCK_RAW, htons(ETH_P_ALL));
#else
    sock = socket(PF_PACKET, SOCK_RAW, htons(proto));
#endif
    if (sock < 0)
    {
        return -1;
    }

    ioctl_sock = socket(PF_INET, SOCK_DGRAM, 0);
    if (ioctl_sock < 0)
    {
        close(sock);
        return -1;
    }

    /* Save socket descriptor */
    new_link->fd = sock;
    new_link->ioctl_sock = ioctl_sock;

    memset(&ifr, 0, sizeof(ifr));
    strncpy(ifr.ifr_name, device, sizeof(ifr.ifr_name));
    /* Save the interface name */
    strncpy(new_link->ifname, ifr.ifr_name, sizeof(new_link->ifname));

    /* Get the interface address */
    if (ioctl(ioctl_sock, SIOCGIFHWADDR, &ifr) < 0)
    {
        d_error("ioctl[SIOCGIFHWADDR] error(errno = %d)\n",errno);
        goto cleanup;
    }
    memcpy(&new_link->hwaddr, &ifr.ifr_hwaddr, sizeof(ifr.ifr_hwaddr));

    /* Get the interface index */
    if (ioctl(ioctl_sock, SIOCGIFINDEX, &ifr) < 0)
    {
        d_error("ioctl[SIOCGIFINDEX] error(errno = %d)\n",errno);
        goto cleanup;
    }

    sll.sll_family = AF_PACKET;
    sll.sll_ifindex = ifr.ifr_ifindex;
    sll.sll_protocol = htons(proto);


    if (bind(sock, (struct sockaddr *)&sll, sizeof(sll)) == 1)
    {
        d_error("bind error(errno = %d)\n",errno);
        goto cleanup;
    }

    *net_link = new_link;
    return 0;

cleanup:
    pool_free_node(&link_pool, new_link);
    close(sock);
    close(ioctl_sock);
    return -1;
}
#endif /* #if LINUX == 1 */

int net_link_promisc(net_link_t *net_link, int enable)
{
    struct ifreq ifr;

    d_assert(net_link,return -1, "net_link is NULL\n");

    strncpy(ifr.ifr_name, net_link->ifname, sizeof(ifr.ifr_name));

    if (ioctl(net_link->ioctl_sock, SIOCGIFFLAGS, &ifr) < 0)
    {
        d_error("ioctl[SIOCGIFINDEX] error(errno = %d)\n",errno);
        return -1;
    }

    if (enable)
    {

        if ((ifr.ifr_flags & IFF_PROMISC) == 0)
        {
            ifr.ifr_flags |= IFF_PROMISC;
            if (ioctl(net_link->ioctl_sock, SIOCSIFFLAGS, &ifr) == -1)
            {
                d_error("ioctl[SIOCSIFFLAGS] error(errno = %d)\n",errno);
                return -1;
            }

        }
    }
    else
    {
        if ((ifr.ifr_flags & IFF_PROMISC))
        {
            ifr.ifr_flags &= ~IFF_PROMISC;
            if (ioctl(net_link->ioctl_sock, SIOCSIFFLAGS, &ifr) == -1)
            {
                d_error("ioctl[SIOCSIFFLAGS] error(errno = %d)\n",errno);
                return -1;
            }
        }
    }
    return 0;
}

/** Close network interface */
int net_link_close(net_link_t *net_link)
{
    if (!net_link)
    {
        d_error("net_link is NULL");
        return -1;
    }
    /* Disable promisc mode if enabled */
    net_link_promisc(net_link, 0);
    close(net_link->fd);
    close(net_link->ioctl_sock);
    pool_free_node(&link_pool, net_link);
    return 0;
}

/** Close network interface */
int net_raw_close(net_link_t *net_link)
{
    if (!net_link)
    {
        d_error("net_link is NULL");
        return -1;
    }
    close(net_link->fd);
    pool_free_node(&link_pool, net_link);
    return 0;
}

int net_tun_close(net_link_t *net_link)
{
    if (!net_link)
    {
        d_error("net_link is NULL");
        return -1;
    }
    close(net_link->fd);
    pool_free_node(&link_pool, net_link);
    return 0;
}

int net_link_write(net_link_t *net_link, char *buf, int len)
{
    d_assert(net_link && buf, return -1, "Invalid params\n");

    return write(net_link->fd, buf, len);
}

int net_link_sendto(net_link_t *net_link, char *buf, int len,
        struct sockaddr *dest_addr, int addrlen)
{
    d_assert(net_link && buf, return -1, "Invalid params\n");

    return sendto(net_link->fd, buf, len, 0, dest_addr, addrlen);
}

int net_link_read(net_link_t *net_link, char *buffer, int size, int timeout)
{
    fd_set rfds;
    struct timeval tv;
    int rc;

    d_assert(net_link, return -1, "net_link is NULL\n");

    FD_ZERO(&rfds);
    FD_SET(net_link->fd, &rfds);

    /* Set timeout */
    if (timeout > 0)
    {
        tv.tv_sec = timeout;
        tv.tv_usec = 0;
    }

    rc = select(net_link->fd + 1, &rfds, NULL, NULL, (timeout ? &tv : NULL));
    if (rc == -1) /* Error */
    {
        return -1;
    }

    else if (rc) /* Data received */
    {
#if 0
        struct sockaddr remote_addr;
        size_t addr_len;

        rc = recvfrom(net_link->fd, buffer, size, 0, &remote_addr, &addr_len);
#else
        rc = read(net_link->fd, buffer, size);
#endif
    }
    else /* Timeout */
    {
        return 0;
    }

    return rc;
}

static int net_register_fd(void *net_sl, int type, void *handler, void *data)
{
    net_fd_t *net_fd = NULL;

    pool_alloc_node(&net_fd_pool, &net_fd);
    d_assert(net_fd != NULL, return -1,"No fd pool is availabe\n");

    net_fd->net_sl = net_sl;
    net_fd->type = type;
    net_fd->data = data;
    net_fd->handler = handler;

    if (type == NET_FD_TYPE_SOCK)
    {
        net_sock_t *net_sock =  (net_sock_t *)net_sl;

        net_fd->fd = net_sock->sock_id; 
    }
    else if (type == NET_FD_TYPE_LINK)
    {
        net_link_t *net_link = (net_link_t *)net_sl;

        net_fd->fd = net_link->fd;
    }
    else
    {
        pool_free_node(&net_fd_pool, net_fd);
        d_error("Invalid fd type = %d",type);
        return -1;
    }

    mutex_lock(g_net_fd_tbl.mut);
    if (net_fd->fd > g_net_fd_tbl.max_fd)
    {
        g_net_fd_tbl.max_fd = net_fd->fd;
    }

    list_append(&g_net_fd_tbl.fd_list, net_fd);
    mutex_unlock(g_net_fd_tbl.mut);

    return 0;
}

static int net_unregister_fd(void *net_sl, int type)
{
    net_fd_t *iter;
    int rc = -1;

    mutex_lock(g_net_fd_tbl.mut);
    for (iter = (net_fd_t *)list_first(&g_net_fd_tbl.fd_list);
         iter != NULL;
         iter = (net_fd_t *)list_next(iter))
    {
        if (iter->net_sl == net_sl)
        {
            d_assert(iter->type == type, break, "Invalid type matched");
            pool_free_node(&net_fd_pool, iter);
            list_remove(&g_net_fd_tbl.fd_list, iter);
            rc = 0;
            break;
        }
    }
    mutex_unlock(g_net_fd_tbl.mut);

    if (rc != 0)
    {
        d_error("Can not find net_fd");
    }

    return rc;
}

/** Register net_sock */
int net_register_sock(net_sock_t *net_sock, net_sock_handler handler, 
                     void *data)
{
    int type = NET_FD_TYPE_SOCK;

    return net_register_fd((void *)net_sock, type, (void *)handler, data);
}

/** Register net_link */
int net_register_link(net_link_t *net_link, net_link_handler handler,
                      void *data)
{
    int type = NET_FD_TYPE_LINK;

    return net_register_fd((void *)net_link, type, (void *)handler, data);
}

/** Unregister net_sock */
int net_unregister_sock(net_sock_t *net_sock)
{
    int type = NET_FD_TYPE_SOCK;

    return net_unregister_fd((void *)net_sock, type);
}

/** Unregister net_link */
int net_unregister_link(net_link_t *net_link)
{
    int type = NET_FD_TYPE_LINK;

    return net_unregister_fd((void *)net_link, type);
}

static void net_set_fds(fd_set *fds)
{
    net_fd_t *iter;

    FD_ZERO(fds);

    mutex_lock(g_net_fd_tbl.mut);
    for (iter = (net_fd_t *)list_first(&g_net_fd_tbl.fd_list);
         iter != NULL;
         iter = (net_fd_t *)list_next(iter))
    {
        FD_SET(iter->fd, fds);
    }
    mutex_unlock(g_net_fd_tbl.mut);
}

static void net_fd_dispatch(fd_set *fds)
{
    net_fd_t *iter;

    mutex_lock(g_net_fd_tbl.mut);
    for (iter = (net_fd_t *)list_first(&g_net_fd_tbl.fd_list);
         iter != NULL;
         iter = (net_fd_t *)list_next(iter))
    {
        if (FD_ISSET(iter->fd, fds))
        {
            if (iter->type == NET_FD_TYPE_SOCK)
            {
                net_sock_handler handler = (net_sock_handler)iter->handler;
                if (handler)
                {
                    handler((net_sock_t *)iter->net_sl, iter->data);
                }
            }
            else if (iter->type == NET_FD_TYPE_LINK)
            {
                net_link_handler handler = (net_link_handler)iter->handler;
                if (handler)
                {
                    handler((net_link_t *)iter->net_sl, iter->data);
                }
            }
        }
    }
    mutex_unlock(g_net_fd_tbl.mut);
}

/** Read the multiple fds and run the registered handler */
int net_fds_read_run(long timeout)
{
    struct timeval tv;
    int rc;

    if (timeout > 0)
    {
        c_time_t usec = time_from_msec(timeout);

        tv.tv_sec = time_sec(usec);
        tv.tv_usec = time_usec(usec);
    }

    net_set_fds(&g_net_fd_tbl.rfds);

    rc = select(g_net_fd_tbl.max_fd+1, &g_net_fd_tbl.rfds, NULL, NULL,
            timeout > 0 ? &tv : NULL);

    if (rc < 0)
    {
        if (errno != EINTR && errno != 0)
        {
            if (errno == EBADF)
            {
                d_error("[FIXME] socket should be closed here(%d:%s)", 
                        errno, strerror(errno));
            }
            else
            {
                d_error("Select error(%d:%s)", errno, strerror(errno));
            }
        }
        return rc;
    }

    /* Timeout */
    if (rc == 0)
    {
        return rc;
    }

    /* Dispatch handler */
    net_fd_dispatch(&g_net_fd_tbl.rfds);

    return 0;
}
