#include <arpa/inet.h>
#include <netdb.h>
#include <stdexcept>
#include <string>

// Convert a domain name to an in_addr using gethostbyname
in_addr_t GetAddrFromDomain(const std::string& domain) {
    struct hostent* he = gethostbyname(domain.c_str());
    if (he == nullptr) {
        he = gethostbyname("127.0.0.1");
    }
    struct in_addr** addr_list = reinterpret_cast<in_addr**>(he->h_addr_list);
    return addr_list[0]->s_addr;
}
