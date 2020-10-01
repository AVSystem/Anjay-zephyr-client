/*
 * Copyright 2020 AVSystem <avsystem@avsystem.com>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <fcntl.h>
#include <unistd.h>

#ifdef CONFIG_POSIX_API
#    include <net/socket.h>
#    include <netdb.h>
#    include <poll.h>
#    include <sys/socket.h>
#endif // CONFIG_POSIX_API

typedef int sockfd_t;

#ifdef CONFIG_POSIX_API
static inline int
getsockname(int sock, struct sockaddr *addr, socklen_t *addrlen) {
    return zsock_getsockname(sock, addr, addrlen);
}
#endif // CONFIG_POSIX_API

static inline int
getpeername(int sock, struct sockaddr *addr, socklen_t *addrlen) {
    // Zephyr does not provide getpeername() in any form. Luckily, it's not
    // necessary for Anjay to work, so let's fake an always-failing one.
    (void) sock;
    (void) addr;
    (void) addrlen;
    return -1;
}
