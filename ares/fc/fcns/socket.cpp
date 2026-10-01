//A minimal outbound TCP connection, so that what the Network System says can leave the emulator.
//
//ares has no networking anywhere else in the tree, and that is a deliberate policy rather than an
//oversight, so this is kept as small and as opt-in as it can be: nothing opens unless the user names
//a destination in the environment, ares only ever *connects* (it never listens, and never accepts),
//and a destination that is not answering is not an error -- the socket simply stays shut and the
//core behaves exactly as it did before, which is what keeps headless runs and CI unaffected.
//
//Two independent destinations, because they carry different things and must not be interleaved:
//
//  ARES_FCNS_LINK     the modem's byte stream, raw. This is the one a replacement server speaks,
//                     and it has to stay byte-exact.
//  ARES_FCNS_MONITOR  every line the "Network" tracer emits, as text, newline terminated. This is
//                     the traffic log, for watching from a script or a terminal.
//
//Both take "host:port", e.g. ARES_FCNS_LINK=127.0.0.1:2400.

#if defined(PLATFORM_WINDOWS)
  #include <winsock2.h>
  #include <ws2tcpip.h>
#else
  #include <sys/socket.h>
  #include <netinet/in.h>
  #include <netinet/tcp.h>
  #include <arpa/inet.h>
  #include <unistd.h>
  #include <fcntl.h>
  #include <errno.h>
#endif

auto FCNS::Socket::open(string target) -> bool {
  close();
  if(!target) return false;

  auto parts = nall::split(target, ":");
  if(parts.size() != 2) {
    return false;
  }
  auto host = parts[0].strip();
  u32  port = parts[1].strip().natural();
  if(!host || !port || port > 65535) {
    return false;
  }

  #if defined(PLATFORM_WINDOWS)
  //one-shot Winsock startup; there is no matching cleanup because the lifetime is the process
  static bool initialized = [] {
    WSADATA data;
    return WSAStartup(MAKEWORD(2, 2), &data) == 0;
  }();
  if(!initialized) return false;
  #endif

  sockaddr_in address{};
  address.sin_family = AF_INET;
  address.sin_port = htons(port);
  if(inet_pton(AF_INET, host, &address.sin_addr) != 1) {
    return false;
  }

  auto fd = (s64)::socket(AF_INET, SOCK_STREAM, 0);
  if(fd < 0) return false;

  //connect while still blocking: a refused connection should be reported now, not discovered later
  if(::connect((int)fd, (sockaddr*)&address, sizeof(address)) != 0) {
    #if defined(PLATFORM_WINDOWS)
    ::closesocket((SOCKET)fd);
    #else
    ::close((int)fd);
    #endif
    return false;
  }

  //from here on the core must never block: the far end is another process running at its own pace,
  //and a stalled read or write would stall the emulated machine.
  #if defined(PLATFORM_WINDOWS)
  u_long nonblocking = 1;
  ioctlsocket((SOCKET)fd, FIONBIO, &nonblocking);
  #else
  fcntl((int)fd, F_SETFL, fcntl((int)fd, F_GETFL, 0) | O_NONBLOCK);
  #endif
  int nodelay = 1;
  ::setsockopt((int)fd, IPPROTO_TCP, TCP_NODELAY, (const char*)&nodelay, sizeof(nodelay));

  handle = fd;
  return true;
}

auto FCNS::Socket::close() -> void {
  if(handle < 0) return;
  #if defined(PLATFORM_WINDOWS)
  ::closesocket((SOCKET)handle);
  #else
  ::close((int)handle);
  #endif
  handle = -1;
}

auto FCNS::Socket::write(const u8* data, u32 size) -> void {
  if(handle < 0 || !size) return;
  u32 offset = 0;
  while(offset < size) {
    auto sent = ::send((int)handle, (const char*)data + offset, size - offset, 0);
    if(sent > 0) { offset += sent; continue; }
    #if !defined(PLATFORM_WINDOWS)
    //the peer is simply not draining us yet; dropping the remainder is better than blocking
    if(sent < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) return;
    if(sent < 0 && errno == EINTR) continue;
    #endif
    close();  //peer hung up
    return;
  }
}

auto FCNS::Socket::read(u8* data, u32 size) -> u32 {
  if(handle < 0 || !size) return 0;
  auto received = ::recv((int)handle, (char*)data, size, 0);
  if(received > 0) return received;
  if(received == 0) return close(), 0;  //orderly shutdown by the peer
  #if !defined(PLATFORM_WINDOWS)
  if(errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) return 0;
  #endif
  close();
  return 0;
}
