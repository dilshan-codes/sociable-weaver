#include <winsock2.h> // Windows Socket API version 2.
// This gives us: socket(), bind(), listen(), accept(), send(), recv(), closesocket().
#include <stdio.h>
#include <string.h> 
#include "server.h"
#include "router.h"

void server_start(int port) {
// runs in an infinite loop accepting browser connections.
// Parameter: port = the port number to listen on. Pass 8080 from main.c.
    
    WSADATA wsa;
    // Loads the Winsock library (ws2_32.dll) into memory.
    // WSADATA is a struct (a group of variables) that Windows fills in with details about the Winsock version it loaded.
    // We declare it here but we never read it — Windows needs it as a storage location to write into.

    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        printf("ERROR: Winsock failed to start\n");
        return;
    }
    // WSAStartup() takes two arguments:
    // MAKEWORD(2, 2) = we want Winsock version 2.2 (the modern version) &wsa = the address of our WSADATA struct for Windows to fill in.
    // means "give me the memory address of this variable".
    // WSAStartup returns 0 if it succeeded, non-zero if it failed.

    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == INVALID_SOCKET) {
        printf("ERROR: Could not create socket\n");
        return;
    }
    // socket() creates a new socket and returns an integer called a file descriptor (fd). In C, almost everything — files, sockets, pipes — is represented as an integer fd.
    // socket() takes three arguments:
    // AF_INET = Address Family Internet = we want IPv4 (e.g. 127.0.0.1)
    // SOCK_STREAM = we want TCP — reliable, ordered, connection-based
    // 0 = use the default protocol for this type, which is TCP
    //
    // If socket() fails it returns INVALID_SOCKET (a special error value).

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, (char*)&opt, sizeof(opt));
    // If the program crashes and you restart it immediately, Windows holds onto the port for ~60 seconds before releasing it.
    // SO_REUSEADDR tells Windows to release the port immediately so we can bind to it again right away.

    struct sockaddr_in address;
    // Binding attaches the socket to a specific IP address and port number. Without binding, the socket exists but is not connected to any address.
    // sockaddr_in is a struct that holds an IP address + port number. It stands for "socket address internet".

    memset(&address, 0, sizeof(address));
    // memset(&address, 0, sizeof(address)) fills the entire address struct with zeros before we start filling in the fields we care about.
    address.sin_family = AF_INET;
    // sin_family = socket address family. AF_INET = IPv4. Must match what we passed to socket() above.
    address.sin_addr.s_addr = INADDR_ANY;
    // sin_addr = the IP address to bind to.
    // INADDR_ANY = bind to ALL available network interfaces on this machine.
    // This means the server accepts connections from 127.0.0.1 (localhost)
    address.sin_port = htons(port);
    // sin_port = the port number.
    // htons() = "host to network short".
    // Your CPU stores numbers in little-endian byte order (small byte first). Network protocols require big-endian byte order (big byte first).
    // htons() swaps the bytes so the network understands the port number correctly. Without htons(), port 8080 would be sent as the wrong number.

    bind(server_fd, (struct sockaddr*)&address, sizeof(address));
    // Now actually bind the socket to this address. bind() takes:
    // server_fd = the socket to bind
    // (struct sockaddr*)&address = pointer to our address struct
    // The cast (struct sockaddr*) is needed because bind() accepts the generic sockaddr type, but we have the more specific sockaddr_in.
    // They are the same size in memory — the cast just changes the type label. sizeof(address) = how large our address struct is
    listen(server_fd, 10);
    // listen() tells the operating system to start accepting incoming connections and queue them up while we are busy processing one.
    // Arguments:
    // server_fd = the socket to listen on
    // 10 = the backlog — how many connections can wait in the queue before the OS starts refusing new ones.

    printf("Sociable Weaver running at http://localhost:%d\n", port);

    while (1) {
    // while(1) is an infinite loop — it runs forever until the program closes.

        int client_fd = accept(server_fd, NULL, NULL);
        // server_fd = the listening socket — stays open forever, never used to send data
        // client_fd = the connection to THIS specific browser tab right now
        // This separation is important: server_fd keeps listening for more browsers while client_fd handles the current one.
        // The two NULL arguments are for storing the client's IP address. We do not need that information so we pass NULL.
        
        if (client_fd == INVALID_SOCKET) continue;
        // If accept() failed for any reason, skip and try again

        router_handle(client_fd);
        closesocket(client_fd);
        // router_handle() reads what the browser asked for and sends back the right response.
        // router_handle() is defined in router.c
        // When the router is done, close this connection.
        // server_fd stays open — only client_fd is closed here.

        // Loop back to accept() — wait for the next browser request
    }

    WSACleanup();
    // WSACleanup() releases Winsock resources.
    // In practice this line is never reached because the while(1) loop above runs forever.
}