HAVE_PCAP := 1
USE_ICMP := 1

# Currently the only reason for PCAP is ICMP
ifeq ($(HAVE_PCAP),1)
	LIBPCAP := -lpcap
endif
ifneq ($(USE_ICMP),1)
	override HAVE_PCAP := 0
endif

#CXXFLAGS = -ggdb -fsanitize=address -fno-omit-frame-pointer -std=c++14 -Wall -Wextra -Ofast -DHAVE_PCAP=$(HAVE_PCAP) -DUSE_ICMP=$(USE_ICMP) -I/usr/local/opt/openssl/include -L/usr/local/opt/openssl/lib
CXXFLAGS = -std=c++14 -Wall -Wextra -Ofast -DHAVE_PCAP=$(HAVE_PCAP) -DUSE_ICMP=$(USE_ICMP)
LIBS = $(LIBPCAP) -lpthread -lssl -lcrypto

tunnel: main.cpp shared.cpp factory.cpp forwarders.cpp tls_helpers.cpp transport_base.cpp obfuscate_base.cpp simple_obfuscator.cpp xor_obfuscator.cpp mocker_base.cpp dns_mocker.cpp http_ws_mocker.cpp socks5_proxy.cpp udp_base.cpp udp_client.cpp udp_server.cpp dtls_server.cpp tcp_base.cpp tcp_client.cpp tcp_server.cpp icmp_base.cpp icmp_client.cpp icmp_server.cpp icmp6_base.cpp icmp6_client.cpp icmp6_server.cpp
	$(CXX) $(CXXFLAGS) main.cpp -o $@ $(LIBS)

clean:
	rm tunnel
.phony: clean
