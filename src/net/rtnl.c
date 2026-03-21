#include "errno.h"
#include "net/netlink.h"
#include "net/rtnl.h"

static struct net_device net_devices[MAX_NET_DEVICES];
static int32_t next_ifindex = 1;
static spin_t net_dev_lock = SPIN_INIT;

static struct rt_entry route_table[MAX_ROUTES];
static spin_t route_lock = SPIN_INIT;

static struct rtnl_link rtnl_msg_handlers[RTM_NR_MSGTYPES];
static spin_t rtnl_lock = SPIN_INIT;

static int rtnl_link_doit(struct nlmsghdr *nlh, uint32_t sender_pid,
                          struct netlink_sock *sender_sock);
static int rtnl_link_dumpit(struct nlmsghdr *nlh, uint32_t sender_pid,
                            struct netlink_sock *sender_sock);
static int rtnl_addr_doit(struct nlmsghdr *nlh, uint32_t sender_pid,
                          struct netlink_sock *sender_sock);
static int rtnl_addr_dumpit(struct nlmsghdr *nlh, uint32_t sender_pid,
                            struct netlink_sock *sender_sock);
static int rtnl_route_doit(struct nlmsghdr *nlh, uint32_t sender_pid,
                           struct netlink_sock *sender_sock);
static int rtnl_route_dumpit(struct nlmsghdr *nlh, uint32_t sender_pid,
                             struct netlink_sock *sender_sock);

void nla_builder_init(struct nla_builder *b, char *buf, size_t capacity) {
    b->buf = buf;
    b->capacity = capacity;
    b->offset = 0;
}

int nla_put(struct nla_builder *b, uint16_t type, const void *data,
            size_t len) {
    size_t total = NLA_TOTAL_LEN(len);

    if (b->offset + total > b->capacity) {
        return -ENOSPC;
    }

    struct nlattr *nla = (struct nlattr *)(b->buf + b->offset);
    nla->nla_type = type;
    nla->nla_len = NLA_LEN(len);

    if (data && len > 0) {
        memcpy(NLA_DATA(nla), data, len);
    }

    size_t padlen = total - NLA_LEN(len);
    if (padlen > 0) {
        memset((char *)NLA_DATA(nla) + len, 0, padlen);
    }

    b->offset += total;
    return 0;
}

int nla_put_u8(struct nla_builder *b, uint16_t type, uint8_t val) {
    return nla_put(b, type, &val, sizeof(val));
}

int nla_put_u16(struct nla_builder *b, uint16_t type, uint16_t val) {
    return nla_put(b, type, &val, sizeof(val));
}

int nla_put_u32(struct nla_builder *b, uint16_t type, uint32_t val) {
    return nla_put(b, type, &val, sizeof(val));
}

int nla_put_u64(struct nla_builder *b, uint16_t type, uint64_t val) {
    return nla_put(b, type, &val, sizeof(val));
}

int nla_put_string(struct nla_builder *b, uint16_t type, const char *str) {
    return nla_put(b, type, str, strlen(str) + 1);
}

struct nlattr *nla_find(void *head, int len, uint16_t type) {
    struct nlattr *nla = (struct nlattr *)head;
    int remaining = len;

    while (NLA_OK(nla, remaining)) {
        if ((nla->nla_type & NLA_TYPE_MASK) == type) {
            return nla;
        }
        nla = NLA_NEXT(nla, remaining);
    }

    return NULL;
}

void *nla_data(const struct nlattr *nla) {
    return NLA_DATA((struct nlattr *)nla);
}

int nla_len(const struct nlattr *nla) { return (int)nla->nla_len - NLA_HDRLEN; }

uint32_t nla_get_u32(const struct nlattr *nla) {
    return *(uint32_t *)NLA_DATA((struct nlattr *)nla);
}

uint16_t nla_get_u16(const struct nlattr *nla) {
    return *(uint16_t *)NLA_DATA((struct nlattr *)nla);
}

uint8_t nla_get_u8(const struct nlattr *nla) {
    return *(uint8_t *)NLA_DATA((struct nlattr *)nla);
}

const char *nla_get_string(const struct nlattr *nla) {
    return (const char *)NLA_DATA((struct nlattr *)nla);
}

static int rtnl_send_error(struct netlink_sock *sender_sock,
                           struct nlmsghdr *orig_nlh, uint32_t sender_pid,
                           int error_code) {
    UNUSED(sender_pid);

    char reply_buf[NLMSG_SPACE(sizeof(struct nlmsgerr))];
    memset(reply_buf, 0, sizeof(reply_buf));

    struct nlmsghdr *nlh = (struct nlmsghdr *)reply_buf;
    nlh->nlmsg_len = NLMSG_LENGTH(sizeof(struct nlmsgerr));
    nlh->nlmsg_type = NLMSG_ERROR;
    nlh->nlmsg_flags = 0;
    nlh->nlmsg_seq = orig_nlh->nlmsg_seq;
    nlh->nlmsg_pid = 0;

    struct nlmsgerr *errmsg = NLMSG_DATA(nlh);
    errmsg->error = error_code;
    memcpy(&errmsg->msg, orig_nlh, sizeof(struct nlmsghdr));

    netlink_buffer_write_packet(sender_sock, reply_buf, nlh->nlmsg_len, 0, 0);
    return 0;
}

static int rtnl_send_done(struct netlink_sock *sender_sock,
                          struct nlmsghdr *orig_nlh, uint32_t sender_pid) {
    UNUSED(sender_pid);

    char reply_buf[NLMSG_SPACE(sizeof(int))];
    memset(reply_buf, 0, sizeof(reply_buf));

    struct nlmsghdr *nlh = (struct nlmsghdr *)reply_buf;
    nlh->nlmsg_len = NLMSG_LENGTH(sizeof(int));
    nlh->nlmsg_type = NLMSG_DONE;
    nlh->nlmsg_flags = NLM_F_MULTI;
    nlh->nlmsg_seq = orig_nlh->nlmsg_seq;
    nlh->nlmsg_pid = 0;

    int *errcode = NLMSG_DATA(nlh);
    *errcode = 0;

    netlink_buffer_write_packet(sender_sock, reply_buf, nlh->nlmsg_len, 0, 0);
    return 0;
}

static int rtnl_fill_link(char *buf, size_t buflen, struct net_device *dev,
                          uint16_t msg_type, uint16_t flags, uint32_t seq,
                          uint32_t pid) {
    size_t ifinfo_len = NLMSG_ALIGN(sizeof(struct ifinfomsg));
    struct nlmsghdr *nlh = (struct nlmsghdr *)buf;

    char *attr_start = buf + NLMSG_HDRLEN + ifinfo_len;
    size_t attr_capacity = buflen - NLMSG_HDRLEN - ifinfo_len;

    struct nla_builder nla_b;
    nla_builder_init(&nla_b, attr_start, attr_capacity);

    nla_put_string(&nla_b, IFLA_IFNAME, dev->name);
    nla_put_u32(&nla_b, IFLA_MTU, dev->mtu);
    nla_put_u32(&nla_b, IFLA_TXQLEN, dev->txqlen);
    nla_put_u8(&nla_b, IFLA_OPERSTATE, dev->operstate);

    if (dev->addr_len > 0) {
        nla_put(&nla_b, IFLA_ADDRESS, dev->addr, dev->addr_len);
        nla_put(&nla_b, IFLA_BROADCAST, dev->broadcast, dev->addr_len);
    }

    nla_put_string(&nla_b, IFLA_QDISC, dev->qdisc);
    nla_put(&nla_b, IFLA_STATS64, &dev->stats, sizeof(dev->stats));

    nlh->nlmsg_len = NLMSG_HDRLEN + ifinfo_len + nla_b.offset;
    nlh->nlmsg_type = msg_type;
    nlh->nlmsg_flags = flags;
    nlh->nlmsg_seq = seq;
    nlh->nlmsg_pid = pid;

    struct ifinfomsg *ifi = NLMSG_DATA(nlh);
    memset(ifi, 0, sizeof(struct ifinfomsg));
    ifi->ifi_family = AF_UNSPEC;
    ifi->ifi_type = dev->type;
    ifi->ifi_index = dev->ifindex;
    ifi->ifi_flags = dev->flags;
    ifi->ifi_change = 0xFFFFFFFF;

    return (int)nlh->nlmsg_len;
}

static int rtnl_fill_addr(char *buf, size_t buflen, struct net_device *dev,
                          struct net_device_addr *addr, uint16_t msg_type,
                          uint16_t flags, uint32_t seq, uint32_t pid) {
    size_t ifa_len = NLMSG_ALIGN(sizeof(struct ifaddrmsg));
    struct nlmsghdr *nlh = (struct nlmsghdr *)buf;

    char *attr_start = buf + NLMSG_HDRLEN + ifa_len;
    size_t attr_capacity = buflen - NLMSG_HDRLEN - ifa_len;

    struct nla_builder nla_b;
    nla_builder_init(&nla_b, attr_start, attr_capacity);

    if (addr->family == AF_INET) {
        nla_put(&nla_b, IFA_ADDRESS, &addr->addr.ipv4, 4);
        nla_put(&nla_b, IFA_LOCAL, &addr->local.ipv4, 4);
        if (addr->broadcast.ipv4 != 0) {
            nla_put(&nla_b, IFA_BROADCAST, &addr->broadcast.ipv4, 4);
        }
    } else if (addr->family == AF_INET6) {
        nla_put(&nla_b, IFA_ADDRESS, addr->addr.ipv6, 16);
        nla_put(&nla_b, IFA_LOCAL, addr->local.ipv6, 16);
    }

    if (addr->label[0] != '\0') {
        nla_put_string(&nla_b, IFA_LABEL, addr->label);
    }

    nla_put(&nla_b, IFA_CACHEINFO, &addr->cacheinfo, sizeof(addr->cacheinfo));

    nlh->nlmsg_len = NLMSG_HDRLEN + ifa_len + nla_b.offset;
    nlh->nlmsg_type = msg_type;
    nlh->nlmsg_flags = flags;
    nlh->nlmsg_seq = seq;
    nlh->nlmsg_pid = pid;

    struct ifaddrmsg *ifa = NLMSG_DATA(nlh);
    memset(ifa, 0, sizeof(struct ifaddrmsg));
    ifa->ifa_family = addr->family;
    ifa->ifa_prefixlen = addr->prefixlen;
    ifa->ifa_flags = addr->flags;
    ifa->ifa_scope = addr->scope;
    ifa->ifa_index = dev->ifindex;

    return (int)nlh->nlmsg_len;
}

static int rtnl_fill_route(char *buf, size_t buflen, struct rt_entry *rt,
                           uint16_t msg_type, uint16_t flags, uint32_t seq,
                           uint32_t pid) {
    size_t rtm_len = NLMSG_ALIGN(sizeof(struct rtmsg));
    struct nlmsghdr *nlh = (struct nlmsghdr *)buf;

    char *attr_start = buf + NLMSG_HDRLEN + rtm_len;
    size_t attr_capacity = buflen - NLMSG_HDRLEN - rtm_len;

    struct nla_builder nla_b;
    nla_builder_init(&nla_b, attr_start, attr_capacity);

    size_t addr_size = (rt->family == AF_INET) ? 4 : 16;

    if (rt->dst_len > 0) {
        if (rt->family == AF_INET) {
            nla_put(&nla_b, RTA_DST, &rt->dst.ipv4, addr_size);
        } else {
            nla_put(&nla_b, RTA_DST, rt->dst.ipv6, addr_size);
        }
    }

    bool has_gateway = false;
    if (rt->family == AF_INET && rt->gateway.ipv4 != 0) {
        has_gateway = true;
    } else if (rt->family == AF_INET6) {
        for (int i = 0; i < 16; i++) {
            if (rt->gateway.ipv6[i] != 0) {
                has_gateway = true;
                break;
            }
        }
    }

    if (has_gateway) {
        if (rt->family == AF_INET) {
            nla_put(&nla_b, RTA_GATEWAY, &rt->gateway.ipv4, addr_size);
        } else {
            nla_put(&nla_b, RTA_GATEWAY, rt->gateway.ipv6, addr_size);
        }
    }

    if (rt->oif > 0) {
        nla_put_u32(&nla_b, RTA_OIF, rt->oif);
    }
    if (rt->priority > 0) {
        nla_put_u32(&nla_b, RTA_PRIORITY, rt->priority);
    }

    bool has_prefsrc = false;
    if (rt->family == AF_INET && rt->prefsrc.ipv4 != 0) {
        has_prefsrc = true;
    }
    if (has_prefsrc) {
        nla_put(&nla_b, RTA_PREFSRC, &rt->prefsrc.ipv4, addr_size);
    }

    nla_put_u32(&nla_b, RTA_TABLE, rt->table);

    nlh->nlmsg_len = NLMSG_HDRLEN + rtm_len + nla_b.offset;
    nlh->nlmsg_type = msg_type;
    nlh->nlmsg_flags = flags;
    nlh->nlmsg_seq = seq;
    nlh->nlmsg_pid = pid;

    struct rtmsg *rtm = NLMSG_DATA(nlh);
    memset(rtm, 0, sizeof(struct rtmsg));
    rtm->rtm_family = rt->family;
    rtm->rtm_dst_len = rt->dst_len;
    rtm->rtm_src_len = rt->src_len;
    rtm->rtm_tos = rt->tos;
    rtm->rtm_table = rt->table;
    rtm->rtm_protocol = rt->protocol;
    rtm->rtm_scope = rt->scope;
    rtm->rtm_type = rt->type;
    rtm->rtm_flags = rt->flags;

    return (int)nlh->nlmsg_len;
}

static int rtnl_link_doit(struct nlmsghdr *nlh, uint32_t sender_pid,
                          struct netlink_sock *sender_sock) {
    struct ifinfomsg *ifi = NLMSG_DATA(nlh);

    switch (nlh->nlmsg_type) {
    case RTM_GETLINK: {
        struct net_device *dev = NULL;

        if (ifi->ifi_index > 0) {
            dev = rtnl_dev_get_by_index(ifi->ifi_index);
        }

        if (dev == NULL) {
            int attr_len = (int)nlh->nlmsg_len - NLMSG_HDRLEN -
                           NLMSG_ALIGN(sizeof(struct ifinfomsg));
            if (attr_len > 0) {
                void *attr_data =
                    (char *)ifi + NLMSG_ALIGN(sizeof(struct ifinfomsg));
                struct nlattr *name_attr =
                    nla_find(attr_data, attr_len, IFLA_IFNAME);
                if (name_attr) {
                    dev = rtnl_dev_get_by_name(nla_get_string(name_attr));
                }
            }
        }

        if (dev == NULL) {
            rtnl_send_error(sender_sock, nlh, sender_pid, -ENODEV);
            return -ENODEV;
        }

        char reply[4096];
        int len = rtnl_fill_link(reply, sizeof(reply), dev, RTM_NEWLINK, 0,
                                 nlh->nlmsg_seq, sender_pid);
        if (len > 0) {
            netlink_buffer_write_packet(sender_sock, reply, (size_t)len, 0, 0);
        }
        return 0;
    }

    case RTM_NEWLINK: {
        int attr_len = (int)nlh->nlmsg_len - NLMSG_HDRLEN -
                       NLMSG_ALIGN(sizeof(struct ifinfomsg));
        void *attr_data = (char *)ifi + NLMSG_ALIGN(sizeof(struct ifinfomsg));

        struct net_device *dev = NULL;
        if (ifi->ifi_index > 0) {
            dev = rtnl_dev_get_by_index(ifi->ifi_index);
        }
        if (dev == NULL && attr_len > 0) {
            struct nlattr *name_attr =
                nla_find(attr_data, attr_len, IFLA_IFNAME);
            if (name_attr) {
                dev = rtnl_dev_get_by_name(nla_get_string(name_attr));
            }
        }

        if (dev != NULL) {
            spin_lock(dev->lock);

            if (ifi->ifi_change & IFF_UP) {
                if (ifi->ifi_flags & IFF_UP) {
                    dev->flags |= IFF_UP;
                } else {
                    dev->flags &= ~IFF_UP;
                }
            }

            uint32_t change = ifi->ifi_change;
            dev->flags = (dev->flags & ~change) | (ifi->ifi_flags & change);

            if (attr_len > 0) {
                struct nlattr *mtu_attr =
                    nla_find(attr_data, attr_len, IFLA_MTU);
                if (mtu_attr) {
                    dev->mtu = nla_get_u32(mtu_attr);
                }

                struct nlattr *txq_attr =
                    nla_find(attr_data, attr_len, IFLA_TXQLEN);
                if (txq_attr) {
                    dev->txqlen = nla_get_u32(txq_attr);
                }
            }

            if (dev->flags & IFF_UP) {
                dev->operstate = IF_OPER_UP;
                dev->flags |= IFF_RUNNING | IFF_LOWER_UP;
            } else {
                dev->operstate = IF_OPER_DOWN;
                dev->flags &= ~(IFF_RUNNING | IFF_LOWER_UP);
            }

            spin_unlock(dev->lock);

            if (attr_len > 0) {
                struct nlattr *wireless_attr =
                    nla_find(attr_data, attr_len, IFLA_WIRELESS);
                if (wireless_attr && dev->wireless_cmd) {
                    int ret =
                        dev->wireless_cmd(dev, nla_data(wireless_attr),
                                          (uint32_t)nla_len(wireless_attr));
                    if (ret < 0) {
                        rtnl_send_error(sender_sock, nlh, sender_pid, ret);
                        return ret;
                    }
                }
            }

            rtnl_notify_link(dev, RTM_NEWLINK);

            if (nlh->nlmsg_flags & NLM_F_ACK) {
                rtnl_send_error(sender_sock, nlh, sender_pid, 0);
            }
            return 0;
        }

        if (nlh->nlmsg_flags & NLM_F_CREATE) {
            char name[IFNAMSIZ] = {0};
            if (attr_len > 0) {
                struct nlattr *name_attr =
                    nla_find(attr_data, attr_len, IFLA_IFNAME);
                if (name_attr) {
                    strncpy(name, nla_get_string(name_attr), IFNAMSIZ - 1);
                }
            }

            if (name[0] == '\0') {
                rtnl_send_error(sender_sock, nlh, sender_pid, -EINVAL);
                return -EINVAL;
            }

            dev = rtnl_dev_alloc(name,
                                 ifi->ifi_type ? ifi->ifi_type : ARPHRD_ETHER);
            if (dev == NULL) {
                rtnl_send_error(sender_sock, nlh, sender_pid, -ENOMEM);
                return -ENOMEM;
            }

            if (attr_len > 0) {
                struct nlattr *mtu_attr =
                    nla_find(attr_data, attr_len, IFLA_MTU);
                if (mtu_attr) {
                    dev->mtu = nla_get_u32(mtu_attr);
                }
            }

            int ret = rtnl_dev_register(dev);
            if (ret < 0) {
                rtnl_send_error(sender_sock, nlh, sender_pid, ret);
                return ret;
            }

            rtnl_notify_link(dev, RTM_NEWLINK);

            if (nlh->nlmsg_flags & NLM_F_ACK) {
                rtnl_send_error(sender_sock, nlh, sender_pid, 0);
            }
            return 0;
        }

        rtnl_send_error(sender_sock, nlh, sender_pid, -ENODEV);
        return -ENODEV;
    }

    case RTM_SETLINK: {
        struct net_device *dev = NULL;
        struct nlattr *wireless_attr = NULL;

        if (ifi->ifi_index > 0) {
            dev = rtnl_dev_get_by_index(ifi->ifi_index);
        }
        if (dev == NULL) {
            rtnl_send_error(sender_sock, nlh, sender_pid, -ENODEV);
            return -ENODEV;
        }

        spin_lock(dev->lock);

        uint32_t change = ifi->ifi_change;
        if (change) {
            dev->flags = (dev->flags & ~change) | (ifi->ifi_flags & change);
        }

        int attr_len = (int)nlh->nlmsg_len - NLMSG_HDRLEN -
                       NLMSG_ALIGN(sizeof(struct ifinfomsg));
        if (attr_len > 0) {
            void *attr_data =
                (char *)ifi + NLMSG_ALIGN(sizeof(struct ifinfomsg));

            struct nlattr *mtu_attr = nla_find(attr_data, attr_len, IFLA_MTU);
            if (mtu_attr) {
                dev->mtu = nla_get_u32(mtu_attr);
            }

            wireless_attr = nla_find(attr_data, attr_len, IFLA_WIRELESS);
        }

        if (dev->flags & IFF_UP) {
            dev->operstate = IF_OPER_UP;
            dev->flags |= IFF_RUNNING | IFF_LOWER_UP;
        } else {
            dev->operstate = IF_OPER_DOWN;
            dev->flags &= ~(IFF_RUNNING | IFF_LOWER_UP);
        }

        spin_unlock(dev->lock);

        if (wireless_attr && dev->wireless_cmd) {
            int ret = dev->wireless_cmd(dev, nla_data(wireless_attr),
                                        (uint32_t)nla_len(wireless_attr));
            if (ret < 0) {
                rtnl_send_error(sender_sock, nlh, sender_pid, ret);
                return ret;
            }
        }

        rtnl_notify_link(dev, RTM_NEWLINK);

        if (nlh->nlmsg_flags & NLM_F_ACK) {
            rtnl_send_error(sender_sock, nlh, sender_pid, 0);
        }
        return 0;
    }

    case RTM_DELLINK: {
        struct net_device *dev = NULL;

        if (ifi->ifi_index > 0) {
            dev = rtnl_dev_get_by_index(ifi->ifi_index);
        }
        if (dev == NULL) {
            rtnl_send_error(sender_sock, nlh, sender_pid, -ENODEV);
            return -ENODEV;
        }
        if (dev->flags & IFF_LOOPBACK) {
            rtnl_send_error(sender_sock, nlh, sender_pid, -EPERM);
            return -EPERM;
        }

        rtnl_notify_link(dev, RTM_DELLINK);
        rtnl_dev_unregister(dev);

        if (nlh->nlmsg_flags & NLM_F_ACK) {
            rtnl_send_error(sender_sock, nlh, sender_pid, 0);
        }
        return 0;
    }

    default:
        rtnl_send_error(sender_sock, nlh, sender_pid, -EOPNOTSUPP);
        return -EOPNOTSUPP;
    }
}

static int rtnl_link_dumpit(struct nlmsghdr *nlh, uint32_t sender_pid,
                            struct netlink_sock *sender_sock) {
    spin_lock(net_dev_lock);

    for (int i = 0; i < MAX_NET_DEVICES; i++) {
        if (!net_devices[i].active) {
            continue;
        }

        struct net_device *dev = &net_devices[i];
        char reply[4096];
        int len = rtnl_fill_link(reply, sizeof(reply), dev, RTM_NEWLINK,
                                 NLM_F_MULTI, nlh->nlmsg_seq, sender_pid);
        if (len > 0) {
            netlink_buffer_write_packet(sender_sock, reply, (size_t)len, 0, 0);
        }
    }

    spin_unlock(net_dev_lock);
    rtnl_send_done(sender_sock, nlh, sender_pid);
    return 0;
}

static int rtnl_addr_doit(struct nlmsghdr *nlh, uint32_t sender_pid,
                          struct netlink_sock *sender_sock) {
    struct ifaddrmsg *ifa = NLMSG_DATA(nlh);
    int attr_len =
        (int)nlh->nlmsg_len - NLMSG_HDRLEN - NLMSG_ALIGN(sizeof(struct ifaddrmsg));
    void *attr_data = (char *)ifa + NLMSG_ALIGN(sizeof(struct ifaddrmsg));

    switch (nlh->nlmsg_type) {
    case RTM_NEWADDR: {
        struct net_device *dev = rtnl_dev_get_by_index((int32_t)ifa->ifa_index);
        if (dev == NULL) {
            rtnl_send_error(sender_sock, nlh, sender_pid, -ENODEV);
            return -ENODEV;
        }

        struct nlattr *addr_attr = NULL;
        struct nlattr *local_attr = NULL;
        struct nlattr *brd_attr = NULL;
        struct nlattr *label_attr = NULL;

        if (attr_len > 0) {
            addr_attr = nla_find(attr_data, attr_len, IFA_ADDRESS);
            local_attr = nla_find(attr_data, attr_len, IFA_LOCAL);
            brd_attr = nla_find(attr_data, attr_len, IFA_BROADCAST);
            label_attr = nla_find(attr_data, attr_len, IFA_LABEL);
        }

        if (addr_attr == NULL && local_attr == NULL) {
            rtnl_send_error(sender_sock, nlh, sender_pid, -EINVAL);
            return -EINVAL;
        }

        spin_lock(dev->lock);

        if (dev->num_addrs >= MAX_IF_ADDRS) {
            spin_unlock(dev->lock);
            rtnl_send_error(sender_sock, nlh, sender_pid, -ENOSPC);
            return -ENOSPC;
        }

        struct net_device_addr *new_addr = &dev->addrs[dev->num_addrs];
        memset(new_addr, 0, sizeof(struct net_device_addr));

        new_addr->family = ifa->ifa_family;
        new_addr->prefixlen = ifa->ifa_prefixlen;
        new_addr->flags = ifa->ifa_flags;
        new_addr->scope = ifa->ifa_scope;

        size_t addr_size = (ifa->ifa_family == AF_INET) ? 4 : 16;

        if (addr_attr) {
            memcpy(&new_addr->addr, nla_data(addr_attr),
                   MIN((size_t)nla_len(addr_attr), addr_size));
        }
        if (local_attr) {
            memcpy(&new_addr->local, nla_data(local_attr),
                   MIN((size_t)nla_len(local_attr), addr_size));
        } else if (addr_attr) {
            memcpy(&new_addr->local, nla_data(addr_attr),
                   MIN((size_t)nla_len(addr_attr), addr_size));
        }
        if (brd_attr) {
            memcpy(&new_addr->broadcast, nla_data(brd_attr),
                   MIN((size_t)nla_len(brd_attr), addr_size));
        }
        if (label_attr) {
            strncpy(new_addr->label, nla_get_string(label_attr), IFNAMSIZ - 1);
        } else {
            strncpy(new_addr->label, dev->name, IFNAMSIZ - 1);
        }

        new_addr->cacheinfo.ifa_prefered = 0xFFFFFFFF;
        new_addr->cacheinfo.ifa_valid = 0xFFFFFFFF;
        new_addr->cacheinfo.cstamp = 0;
        new_addr->cacheinfo.tstamp = 0;

        dev->num_addrs++;
        spin_unlock(dev->lock);

        rtnl_notify_addr(dev, new_addr, RTM_NEWADDR);

        struct rt_entry local_rt;
        memset(&local_rt, 0, sizeof(local_rt));
        local_rt.family = ifa->ifa_family;
        local_rt.dst_len = (ifa->ifa_family == AF_INET) ? 32 : 128;
        local_rt.table = RT_TABLE_LOCAL;
        local_rt.protocol = RTPROT_KERNEL;
        local_rt.scope = RT_SCOPE_HOST;
        local_rt.type = RTN_LOCAL;
        local_rt.oif = dev->ifindex;
        if (ifa->ifa_family == AF_INET) {
            local_rt.dst.ipv4 = new_addr->local.ipv4;
            local_rt.prefsrc.ipv4 = new_addr->local.ipv4;
        }
        rtnl_route_add(&local_rt);

        if (ifa->ifa_prefixlen > 0 &&
            ifa->ifa_prefixlen < ((ifa->ifa_family == AF_INET) ? 32 : 128)) {
            struct rt_entry subnet_rt;
            memset(&subnet_rt, 0, sizeof(subnet_rt));
            subnet_rt.family = ifa->ifa_family;
            subnet_rt.dst_len = ifa->ifa_prefixlen;
            subnet_rt.table = RT_TABLE_MAIN;
            subnet_rt.protocol = RTPROT_KERNEL;
            subnet_rt.scope = RT_SCOPE_LINK;
            subnet_rt.type = RTN_UNICAST;
            subnet_rt.oif = dev->ifindex;

            if (ifa->ifa_family == AF_INET) {
                uint32_t mask = 0;
                if (ifa->ifa_prefixlen > 0) {
                    mask = ~((1U << (32 - ifa->ifa_prefixlen)) - 1);
                    mask = __builtin_bswap32(mask);
                }
                subnet_rt.dst.ipv4 = new_addr->addr.ipv4 & mask;
                subnet_rt.prefsrc.ipv4 = new_addr->local.ipv4;
            }

            rtnl_route_add(&subnet_rt);
        }

        if (nlh->nlmsg_flags & NLM_F_ACK) {
            rtnl_send_error(sender_sock, nlh, sender_pid, 0);
        }
        return 0;
    }

    case RTM_DELADDR: {
        struct net_device *dev = rtnl_dev_get_by_index((int32_t)ifa->ifa_index);
        if (dev == NULL) {
            rtnl_send_error(sender_sock, nlh, sender_pid, -ENODEV);
            return -ENODEV;
        }

        struct nlattr *addr_attr = NULL;
        if (attr_len > 0) {
            addr_attr = nla_find(attr_data, attr_len, IFA_ADDRESS);
            if (!addr_attr) {
                addr_attr = nla_find(attr_data, attr_len, IFA_LOCAL);
            }
        }

        if (addr_attr == NULL) {
            rtnl_send_error(sender_sock, nlh, sender_pid, -EINVAL);
            return -EINVAL;
        }

        size_t addr_size = (ifa->ifa_family == AF_INET) ? 4 : 16;

        spin_lock(dev->lock);

        bool found = false;
        for (int i = 0; i < dev->num_addrs; i++) {
            struct net_device_addr *da = &dev->addrs[i];

            if (da->family != ifa->ifa_family || da->prefixlen != ifa->ifa_prefixlen) {
                continue;
            }
            if (memcmp(&da->addr, nla_data(addr_attr), addr_size) == 0) {
                rtnl_notify_addr(dev, da, RTM_DELADDR);

                for (int j = i; j < dev->num_addrs - 1; j++) {
                    memcpy(&dev->addrs[j], &dev->addrs[j + 1],
                           sizeof(struct net_device_addr));
                }
                dev->num_addrs--;
                found = true;
                break;
            }
        }

        spin_unlock(dev->lock);

        if (!found) {
            rtnl_send_error(sender_sock, nlh, sender_pid, -EADDRNOTAVAIL);
            return -EADDRNOTAVAIL;
        }

        if (nlh->nlmsg_flags & NLM_F_ACK) {
            rtnl_send_error(sender_sock, nlh, sender_pid, 0);
        }
        return 0;
    }

    case RTM_GETADDR:
        rtnl_send_error(sender_sock, nlh, sender_pid, -EOPNOTSUPP);
        return -EOPNOTSUPP;

    default:
        rtnl_send_error(sender_sock, nlh, sender_pid, -EOPNOTSUPP);
        return -EOPNOTSUPP;
    }
}

static int rtnl_addr_dumpit(struct nlmsghdr *nlh, uint32_t sender_pid,
                            struct netlink_sock *sender_sock) {
    struct ifaddrmsg *ifa = NLMSG_DATA(nlh);
    uint8_t family_filter = ifa->ifa_family;

    spin_lock(net_dev_lock);

    for (int i = 0; i < MAX_NET_DEVICES; i++) {
        if (!net_devices[i].active) {
            continue;
        }

        struct net_device *dev = &net_devices[i];
        spin_lock(dev->lock);

        for (int j = 0; j < dev->num_addrs; j++) {
            struct net_device_addr *addr = &dev->addrs[j];

            if (family_filter != 0 && addr->family != family_filter) {
                continue;
            }
            if (ifa->ifa_index != 0 &&
                (uint32_t)dev->ifindex != ifa->ifa_index) {
                continue;
            }

            char reply[4096];
            int len =
                rtnl_fill_addr(reply, sizeof(reply), dev, addr, RTM_NEWADDR,
                               NLM_F_MULTI, nlh->nlmsg_seq, sender_pid);
            if (len > 0) {
                netlink_buffer_write_packet(sender_sock, reply, (size_t)len, 0,
                                            0);
            }
        }

        spin_unlock(dev->lock);
    }

    spin_unlock(net_dev_lock);
    rtnl_send_done(sender_sock, nlh, sender_pid);
    return 0;
}

static int rtnl_route_doit(struct nlmsghdr *nlh, uint32_t sender_pid,
                           struct netlink_sock *sender_sock) {
    struct rtmsg *rtm = NLMSG_DATA(nlh);
    int attr_len =
        (int)nlh->nlmsg_len - NLMSG_HDRLEN - NLMSG_ALIGN(sizeof(struct rtmsg));
    void *attr_data = (char *)rtm + NLMSG_ALIGN(sizeof(struct rtmsg));

    switch (nlh->nlmsg_type) {
    case RTM_NEWROUTE: {
        struct rt_entry new_route;
        memset(&new_route, 0, sizeof(new_route));

        new_route.family = rtm->rtm_family;
        new_route.dst_len = rtm->rtm_dst_len;
        new_route.src_len = rtm->rtm_src_len;
        new_route.tos = rtm->rtm_tos;
        new_route.table = rtm->rtm_table;
        new_route.protocol = rtm->rtm_protocol;
        new_route.scope = rtm->rtm_scope;
        new_route.type = rtm->rtm_type;
        new_route.flags = rtm->rtm_flags;

        if (new_route.type == RTN_UNSPEC) {
            new_route.type = RTN_UNICAST;
        }
        if (new_route.table == RT_TABLE_UNSPEC) {
            new_route.table = RT_TABLE_MAIN;
        }
        if (new_route.protocol == RTPROT_UNSPEC) {
            new_route.protocol = RTPROT_BOOT;
        }

        size_t addr_size = (new_route.family == AF_INET) ? 4 : 16;

        if (attr_len > 0) {
            struct nlattr *dst_attr = nla_find(attr_data, attr_len, RTA_DST);
            if (dst_attr) {
                memcpy(&new_route.dst, nla_data(dst_attr),
                       MIN((size_t)nla_len(dst_attr), addr_size));
            }

            struct nlattr *gw_attr = nla_find(attr_data, attr_len, RTA_GATEWAY);
            if (gw_attr) {
                memcpy(&new_route.gateway, nla_data(gw_attr),
                       MIN((size_t)nla_len(gw_attr), addr_size));
                if (new_route.scope == RT_SCOPE_NOWHERE) {
                    new_route.scope = RT_SCOPE_UNIVERSE;
                }
            }

            struct nlattr *oif_attr = nla_find(attr_data, attr_len, RTA_OIF);
            if (oif_attr) {
                new_route.oif = (int32_t)nla_get_u32(oif_attr);
            }

            struct nlattr *prio_attr =
                nla_find(attr_data, attr_len, RTA_PRIORITY);
            if (prio_attr) {
                new_route.priority = nla_get_u32(prio_attr);
            }

            struct nlattr *prefsrc_attr =
                nla_find(attr_data, attr_len, RTA_PREFSRC);
            if (prefsrc_attr) {
                memcpy(&new_route.prefsrc, nla_data(prefsrc_attr),
                       MIN((size_t)nla_len(prefsrc_attr), addr_size));
            }

            struct nlattr *table_attr =
                nla_find(attr_data, attr_len, RTA_TABLE);
            if (table_attr) {
                new_route.table = (uint8_t)nla_get_u32(table_attr);
            }
        }

        if (new_route.scope == 0) {
            bool has_gw = false;
            if (new_route.family == AF_INET && new_route.gateway.ipv4 != 0) {
                has_gw = true;
            }
            new_route.scope = has_gw ? RT_SCOPE_UNIVERSE : RT_SCOPE_LINK;
        }

        int ret = rtnl_route_add(&new_route);
        if (ret < 0) {
            rtnl_send_error(sender_sock, nlh, sender_pid, ret);
            return ret;
        }

        rtnl_notify_route(&new_route, RTM_NEWROUTE);

        if (nlh->nlmsg_flags & NLM_F_ACK) {
            rtnl_send_error(sender_sock, nlh, sender_pid, 0);
        }
        return 0;
    }

    case RTM_DELROUTE: {
        struct rt_entry del_route;
        memset(&del_route, 0, sizeof(del_route));

        del_route.family = rtm->rtm_family;
        del_route.dst_len = rtm->rtm_dst_len;
        del_route.table = rtm->rtm_table;

        size_t addr_size = (del_route.family == AF_INET) ? 4 : 16;

        if (attr_len > 0) {
            struct nlattr *dst_attr = nla_find(attr_data, attr_len, RTA_DST);
            if (dst_attr) {
                memcpy(&del_route.dst, nla_data(dst_attr),
                       MIN((size_t)nla_len(dst_attr), addr_size));
            }

            struct nlattr *gw_attr = nla_find(attr_data, attr_len, RTA_GATEWAY);
            if (gw_attr) {
                memcpy(&del_route.gateway, nla_data(gw_attr),
                       MIN((size_t)nla_len(gw_attr), addr_size));
            }

            struct nlattr *table_attr =
                nla_find(attr_data, attr_len, RTA_TABLE);
            if (table_attr) {
                del_route.table = (uint8_t)nla_get_u32(table_attr);
            }
        }

        int ret = rtnl_route_del(&del_route);
        if (ret < 0) {
            rtnl_send_error(sender_sock, nlh, sender_pid, ret);
            return ret;
        }

        rtnl_notify_route(&del_route, RTM_DELROUTE);

        if (nlh->nlmsg_flags & NLM_F_ACK) {
            rtnl_send_error(sender_sock, nlh, sender_pid, 0);
        }
        return 0;
    }

    case RTM_GETROUTE:
        rtnl_send_error(sender_sock, nlh, sender_pid, -EOPNOTSUPP);
        return -EOPNOTSUPP;

    default:
        rtnl_send_error(sender_sock, nlh, sender_pid, -EOPNOTSUPP);
        return -EOPNOTSUPP;
    }
}

static int rtnl_route_dumpit(struct nlmsghdr *nlh, uint32_t sender_pid,
                             struct netlink_sock *sender_sock) {
    struct rtmsg *rtm = NLMSG_DATA(nlh);
    uint8_t family_filter = rtm->rtm_family;
    uint8_t table_filter = rtm->rtm_table;

    spin_lock(route_lock);

    for (int i = 0; i < MAX_ROUTES; i++) {
        if (!route_table[i].active) {
            continue;
        }

        struct rt_entry *rt = &route_table[i];

        if (family_filter != 0 && rt->family != family_filter) {
            continue;
        }
        if (table_filter != RT_TABLE_UNSPEC && rt->table != table_filter) {
            continue;
        }

        char reply[4096];
        int len = rtnl_fill_route(reply, sizeof(reply), rt, RTM_NEWROUTE,
                                  NLM_F_MULTI, nlh->nlmsg_seq, sender_pid);
        if (len > 0) {
            netlink_buffer_write_packet(sender_sock, reply, (size_t)len, 0, 0);
        }
    }

    spin_unlock(route_lock);
    rtnl_send_done(sender_sock, nlh, sender_pid);
    return 0;
}

struct net_device *rtnl_dev_alloc(const char *name, uint16_t type) {
    spin_lock(net_dev_lock);

    struct net_device *dev = NULL;
    for (int i = 0; i < MAX_NET_DEVICES; i++) {
        if (!net_devices[i].active) {
            dev = &net_devices[i];
            break;
        }
    }

    if (dev == NULL) {
        spin_unlock(net_dev_lock);
        return NULL;
    }

    memset(dev, 0, sizeof(struct net_device));
    strncpy(dev->name, name, IFNAMSIZ - 1);
    dev->type = type;
    dev->ifindex = next_ifindex++;
    dev->mtu = 1500;
    dev->txqlen = 1000;
    dev->operstate = IF_OPER_DOWN;
    dev->lock = SPIN_INIT;
    strncpy(dev->qdisc, "noop", IFNAMSIZ - 1);

    if (type == ARPHRD_ETHER) {
        dev->addr_len = 6;
        dev->flags = IFF_BROADCAST | IFF_MULTICAST;
    } else if (type == ARPHRD_LOOPBACK) {
        dev->addr_len = 6;
        dev->flags = IFF_LOOPBACK | IFF_UP | IFF_RUNNING | IFF_LOWER_UP;
        dev->operstate = IF_OPER_UP;
        dev->mtu = 65536;
        dev->txqlen = 1000;
        strncpy(dev->qdisc, "noqueue", IFNAMSIZ - 1);
    } else {
        dev->addr_len = 0;
    }

    spin_unlock(net_dev_lock);
    return dev;
}

int rtnl_dev_register(struct net_device *dev) {
    if (dev == NULL) {
        return -EINVAL;
    }

    spin_lock(net_dev_lock);

    for (int i = 0; i < MAX_NET_DEVICES; i++) {
        if (net_devices[i].active &&
            strcmp(net_devices[i].name, dev->name) == 0 &&
            &net_devices[i] != dev) {
            spin_unlock(net_dev_lock);
            return -EEXIST;
        }
    }

    dev->active = true;
    spin_unlock(net_dev_lock);
    return 0;
}

void rtnl_dev_unregister(struct net_device *dev) {
    if (dev == NULL) {
        return;
    }

    spin_lock(net_dev_lock);
    dev->active = false;
    spin_unlock(net_dev_lock);
}

void rtnl_dev_set_wireless_handler(struct net_device *dev, void *priv,
                                   rtnl_wireless_cmd_t handler) {
    if (!dev) {
        return;
    }

    spin_lock(dev->lock);
    dev->wireless_priv = priv;
    dev->wireless_cmd = handler;
    spin_unlock(dev->lock);
}

struct net_device *rtnl_dev_get_by_index(int32_t ifindex) {
    spin_lock(net_dev_lock);

    for (int i = 0; i < MAX_NET_DEVICES; i++) {
        if (net_devices[i].active && net_devices[i].ifindex == ifindex) {
            spin_unlock(net_dev_lock);
            return &net_devices[i];
        }
    }

    spin_unlock(net_dev_lock);
    return NULL;
}

struct net_device *rtnl_dev_get_by_name(const char *name) {
    spin_lock(net_dev_lock);

    for (int i = 0; i < MAX_NET_DEVICES; i++) {
        if (net_devices[i].active && strcmp(net_devices[i].name, name) == 0) {
            spin_unlock(net_dev_lock);
            return &net_devices[i];
        }
    }

    spin_unlock(net_dev_lock);
    return NULL;
}

int rtnl_route_add(struct rt_entry *route) {
    if (route == NULL) {
        return -EINVAL;
    }

    spin_lock(route_lock);

    size_t addr_size = (route->family == AF_INET) ? 4 : 16;
    for (int i = 0; i < MAX_ROUTES; i++) {
        if (!route_table[i].active) {
            continue;
        }

        struct rt_entry *existing = &route_table[i];
        if (existing->family == route->family &&
            existing->dst_len == route->dst_len &&
            existing->table == route->table &&
            memcmp(&existing->dst, &route->dst, addr_size) == 0) {
            memcpy(existing, route, sizeof(struct rt_entry));
            existing->active = true;
            spin_unlock(route_lock);
            return 0;
        }
    }

    for (int i = 0; i < MAX_ROUTES; i++) {
        if (!route_table[i].active) {
            memcpy(&route_table[i], route, sizeof(struct rt_entry));
            route_table[i].active = true;
            spin_unlock(route_lock);
            return 0;
        }
    }

    spin_unlock(route_lock);
    return -ENOSPC;
}

int rtnl_route_del(struct rt_entry *route) {
    if (route == NULL) {
        return -EINVAL;
    }

    spin_lock(route_lock);

    size_t addr_size = (route->family == AF_INET) ? 4 : 16;
    for (int i = 0; i < MAX_ROUTES; i++) {
        if (!route_table[i].active) {
            continue;
        }

        struct rt_entry *existing = &route_table[i];
        if (existing->family == route->family &&
            existing->dst_len == route->dst_len &&
            memcmp(&existing->dst, &route->dst, addr_size) == 0) {

            if (route->table != RT_TABLE_UNSPEC &&
                existing->table != route->table) {
                continue;
            }

            bool gw_specified = false;
            if (route->family == AF_INET && route->gateway.ipv4 != 0) {
                gw_specified = true;
            }
            if (gw_specified &&
                memcmp(&existing->gateway, &route->gateway, addr_size) != 0) {
                continue;
            }

            existing->active = false;
            spin_unlock(route_lock);
            return 0;
        }
    }

    spin_unlock(route_lock);
    return -ESRCH;
}

int rtnl_get_primary_ipv4_config(int32_t *ifindex, uint32_t *addr,
                                 uint8_t *prefixlen, uint32_t *gateway) {
    struct net_device *chosen_dev = NULL;
    struct net_device_addr *chosen_addr = NULL;
    uint32_t chosen_gw = 0;

    if (!ifindex || !addr || !prefixlen || !gateway) {
        return -EINVAL;
    }

    spin_lock(net_dev_lock);
    for (int i = 0; i < MAX_NET_DEVICES; i++) {
        struct net_device *dev = &net_devices[i];
        if (!dev->active || !(dev->flags & IFF_UP) ||
            dev->type == ARPHRD_LOOPBACK) {
            continue;
        }

        for (int j = 0; j < dev->num_addrs; j++) {
            if (dev->addrs[j].family != AF_INET) {
                continue;
            }
            chosen_dev = dev;
            chosen_addr = &dev->addrs[j];
            break;
        }

        if (chosen_dev) {
            break;
        }
    }
    spin_unlock(net_dev_lock);

    if (!chosen_dev || !chosen_addr) {
        return -ENOENT;
    }

    spin_lock(route_lock);
    for (int i = 0; i < MAX_ROUTES; i++) {
        struct rt_entry *rt = &route_table[i];
        if (!rt->active || rt->family != AF_INET) {
            continue;
        }
        if (rt->dst_len != 0 || rt->oif != chosen_dev->ifindex) {
            continue;
        }
        chosen_gw = rt->gateway.ipv4;
        break;
    }
    spin_unlock(route_lock);

    *ifindex = chosen_dev->ifindex;
    *addr = chosen_addr->local.ipv4;
    *prefixlen = chosen_addr->prefixlen;
    *gateway = chosen_gw;
    return 0;
}

int rtnl_addr_add(int32_t ifindex, uint8_t family, const void *addr,
                  uint8_t prefixlen, uint8_t scope, uint8_t flags) {
    struct net_device *dev = rtnl_dev_get_by_index(ifindex);
    if (dev == NULL) {
        return -ENODEV;
    }

    spin_lock(dev->lock);

    if (dev->num_addrs >= MAX_IF_ADDRS) {
        spin_unlock(dev->lock);
        return -ENOSPC;
    }

    struct net_device_addr *new_addr = &dev->addrs[dev->num_addrs];
    memset(new_addr, 0, sizeof(struct net_device_addr));

    new_addr->family = family;
    new_addr->prefixlen = prefixlen;
    new_addr->scope = scope;
    new_addr->flags = flags;
    strncpy(new_addr->label, dev->name, IFNAMSIZ - 1);

    size_t addr_size = (family == AF_INET) ? 4 : 16;
    memcpy(&new_addr->addr, addr, addr_size);
    memcpy(&new_addr->local, addr, addr_size);

    if (family == AF_INET && prefixlen < 32) {
        uint32_t ip = *(const uint32_t *)addr;
        uint32_t mask = 0;
        if (prefixlen > 0) {
            mask = __builtin_bswap32(~((1U << (32 - prefixlen)) - 1));
        }
        new_addr->broadcast.ipv4 = ip | ~mask;
    }

    new_addr->cacheinfo.ifa_prefered = 0xFFFFFFFF;
    new_addr->cacheinfo.ifa_valid = 0xFFFFFFFF;

    dev->num_addrs++;
    spin_unlock(dev->lock);

    rtnl_notify_addr(dev, new_addr, RTM_NEWADDR);
    return 0;
}

int rtnl_addr_del(int32_t ifindex, uint8_t family, const void *addr,
                  uint8_t prefixlen) {
    struct net_device *dev = rtnl_dev_get_by_index(ifindex);
    if (dev == NULL) {
        return -ENODEV;
    }

    size_t addr_size = (family == AF_INET) ? 4 : 16;

    spin_lock(dev->lock);

    for (int i = 0; i < dev->num_addrs; i++) {
        if (dev->addrs[i].family == family &&
            dev->addrs[i].prefixlen == prefixlen &&
            memcmp(&dev->addrs[i].addr, addr, addr_size) == 0) {

            rtnl_notify_addr(dev, &dev->addrs[i], RTM_DELADDR);

            for (int j = i; j < dev->num_addrs - 1; j++) {
                memcpy(&dev->addrs[j], &dev->addrs[j + 1],
                       sizeof(struct net_device_addr));
            }
            dev->num_addrs--;

            spin_unlock(dev->lock);
            return 0;
        }
    }

    spin_unlock(dev->lock);
    return -EADDRNOTAVAIL;
}

void rtnl_notify_link(struct net_device *dev, uint16_t event_type) {
    char buf[4096];
    int len = rtnl_fill_link(buf, sizeof(buf), dev, event_type, 0, 0, 0);

    if (len > 0) {
        netlink_broadcast_to_group(buf, (size_t)len, 0,
                                   RTNLGRP_TO_MASK(RTNLGRP_LINK),
                                   NETLINK_ROUTE, 0, NULL);
    }
}

void rtnl_notify_addr(struct net_device *dev, struct net_device_addr *addr,
                      uint16_t event_type) {
    char buf[4096];
    int len = rtnl_fill_addr(buf, sizeof(buf), dev, addr, event_type, 0, 0, 0);

    if (len > 0) {
        uint32_t groups = 0;
        netlink_broadcast_to_group(buf, (size_t)len, 0, groups, NETLINK_ROUTE,
                                   0, NULL);
    }
}

void rtnl_notify_route(struct rt_entry *route, uint16_t event_type) {
    char buf[4096];
    int len = rtnl_fill_route(buf, sizeof(buf), route, event_type, 0, 0, 0);

    if (len > 0) {
        uint32_t groups = 0;
        if (route->family == AF_INET) {
            groups = RTNLGRP_TO_MASK(RTNLGRP_IPV4_ROUTE);
        } else if (route->family == AF_INET6) {
            groups = RTNLGRP_TO_MASK(RTNLGRP_IPV6_ROUTE);
        }

        if (groups) {
            netlink_broadcast_to_group(buf, (size_t)len, 0, groups,
                                       NETLINK_ROUTE, 0, NULL);
        }
    }
}

int rtnl_register(uint16_t msgtype, rtnl_doit_func doit,
                  rtnl_dumpit_func dumpit) {
    if (msgtype < RTM_BASE || msgtype >= RTM_BASE + RTM_NR_MSGTYPES) {
        return -EINVAL;
    }

    int idx = msgtype - RTM_BASE;

    spin_lock(rtnl_lock);
    if (doit) {
        rtnl_msg_handlers[idx].doit = doit;
    }
    if (dumpit) {
        rtnl_msg_handlers[idx].dumpit = dumpit;
    }
    spin_unlock(rtnl_lock);

    return 0;
}

int rtnl_process_msg(struct netlink_sock *sender_sock, const char *data,
                     size_t len, uint32_t sender_pid) {
    if (data == NULL || len < sizeof(struct nlmsghdr)) {
        return -EINVAL;
    }

    const char *ptr = data;
    int remaining = (int)len;

    while (NLMSG_OK((struct nlmsghdr *)ptr, remaining)) {
        struct nlmsghdr *nlh = (struct nlmsghdr *)ptr;

        if (nlh->nlmsg_len < NLMSG_HDRLEN) {
            break;
        }

        if (nlh->nlmsg_type == NLMSG_NOOP) {
            nlh = NLMSG_NEXT(nlh, remaining);
            ptr = (const char *)nlh;
            continue;
        }

        if (nlh->nlmsg_type == NLMSG_DONE) {
            break;
        }

        if (nlh->nlmsg_type == NLMSG_ERROR) {
            nlh = NLMSG_NEXT(nlh, remaining);
            ptr = (const char *)nlh;
            continue;
        }

        if (nlh->nlmsg_type >= RTM_BASE &&
            nlh->nlmsg_type < RTM_BASE + RTM_NR_MSGTYPES) {
            int idx = nlh->nlmsg_type - RTM_BASE;
            struct rtnl_link *link = &rtnl_msg_handlers[idx];
            bool is_dump = (nlh->nlmsg_flags & NLM_F_DUMP) == NLM_F_DUMP;

            int ret;
            if (is_dump && link->dumpit) {
                ret = link->dumpit(nlh, sender_pid, sender_sock);
            } else if (link->doit) {
                ret = link->doit(nlh, sender_pid, sender_sock);
            } else {
                rtnl_send_error(sender_sock, nlh, sender_pid, -EOPNOTSUPP);
                ret = -EOPNOTSUPP;
            }

            UNUSED(ret);
        } else {
            rtnl_send_error(sender_sock, nlh, sender_pid, -EOPNOTSUPP);
        }

        nlh = NLMSG_NEXT(nlh, remaining);
        ptr = (const char *)nlh;
    }

    return 0;
}

void rtnl_init() {
    memset(rtnl_msg_handlers, 0, sizeof(rtnl_msg_handlers));

    spin_lock(net_dev_lock);
    memset(net_devices, 0, sizeof(net_devices));
    next_ifindex = 1;
    spin_unlock(net_dev_lock);

    spin_lock(route_lock);
    memset(route_table, 0, sizeof(route_table));
    spin_unlock(route_lock);

    rtnl_register(RTM_NEWLINK, rtnl_link_doit, NULL);
    rtnl_register(RTM_DELLINK, rtnl_link_doit, NULL);
    rtnl_register(RTM_GETLINK, rtnl_link_doit, rtnl_link_dumpit);
    rtnl_register(RTM_SETLINK, rtnl_link_doit, NULL);

    rtnl_register(RTM_NEWADDR, rtnl_addr_doit, NULL);
    rtnl_register(RTM_DELADDR, rtnl_addr_doit, NULL);
    rtnl_register(RTM_GETADDR, rtnl_addr_doit, rtnl_addr_dumpit);

    rtnl_register(RTM_NEWROUTE, rtnl_route_doit, NULL);
    rtnl_register(RTM_DELROUTE, rtnl_route_doit, NULL);
    rtnl_register(RTM_GETROUTE, rtnl_route_doit, rtnl_route_dumpit);
}
