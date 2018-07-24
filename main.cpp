#include <iostream>
#include "tcp_server.h"

int main() {
    TCPServer server(3, 8881);

    server.Start();

    while(true) {
        sleep(1);
    }

    return 0;
}