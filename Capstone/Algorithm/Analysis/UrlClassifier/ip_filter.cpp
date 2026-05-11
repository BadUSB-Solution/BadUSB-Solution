#include "ip_filter.h"
#include <winsock2.h>
#include <ws2tcpip.h>

#pragma comment(lib, "ws2_32.lib")

namespace UrlClassifier::IpFilter
{
    uint32_t Check(const ParsedUrl& p)
    {
        if (p.host.empty()) return 0;

        std::string narrow = WideToNarrow(p.host);
        struct in_addr addr{};
        if (inet_pton(AF_INET, narrow.c_str(), &addr) != 1)
            return 0;  // IPv4 아님

        uint32_t flags = URL_FLAG_IP_ADDRESS;
        uint32_t ip = ntohl(addr.s_addr);

        // 127.0.0.0/8 - 로컬호스트
        if ((ip & 0xFF000000u) == 0x7F000000u) {
            flags |= URL_FLAG_IP_LOOPBACK;
        }
        // 사설 IP (10/8, 172.16/12, 192.168/16)
        else if ((ip & 0xFF000000u) == 0x0A000000u ||
                 (ip & 0xFFF00000u) == 0xAC100000u ||
                 (ip & 0xFFFF0000u) == 0xC0A80000u) {
            flags |= URL_FLAG_IP_PRIVATE;
        }
        // 링크 로컬 169.254/16
        else if ((ip & 0xFFFF0000u) == 0xA9FE0000u) {
            flags |= URL_FLAG_IP_LINKLOCAL;
        }

        return flags;
    }
}
