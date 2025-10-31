#pragma once
#define MAX_NAME_LEN 255
enum { NUM_DIRECTIONS = 4 }; ///< Number of valid directions

// These definitions helps to write cross-platform code
#if !defined(_WIN32)
// In UNIX, sockets are file descriptors (int)
// In Winsock, sockets are of type SOCKET (unsigned int)
typedef int SOCKET;
// In UNIX, the value of an invalid socket is -1, while Winsock uses a value
// called INVALID_SOCKET
#define ISVALIDSOCKET(s) ((s) >= 0)
// In UNIX, the function to close a socket is called close(), while in Winsock
// it's called closesocket()
#define CLOSESOCKET(s) close(s)
// Error handling is also different between UNIX and Winsock. UNIX uses errno
#define GETSOCKETERRNO() (errno)
#else
#define ISVALIDSOCKET(s) ((s) != INVALID_SOCKET)
#define CLOSESOCKET(s) closesocket(s)
#define GETSOCKETERRNO() (WSAGetLastError())
#endif
