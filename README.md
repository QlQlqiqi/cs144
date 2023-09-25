1. 每当接收到 ARP 时，记录 sender IP 和 mac，并记录 30s 过期时间；
2. 每当向一个 IP 发送 ARP 广播时，查看过去 5s 内是否已经发送过了；
3. eth frame 中的 src 和 dst 会经常变，因为其是 frame 经过多个节点时携带的信息，而 payload 中的地址信息才是我们需要的；
