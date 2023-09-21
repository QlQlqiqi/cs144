1. rev_win_size 记录 window size，out_segs_num_ 记录待回复（“内核”中排队的和已发送未回复的）的 segments 的总 seqno number，rev_win_size - out_segs_num_ 作为可发送的 seqno number；
2. 记录 receive message 中的 window size，如果为 0，则需要在  push 认为是 1，保证 sender 可以发送 segment，使得 receiver 告诉 sender 最新的 window size；
3. 如果 window size 不为 0，则需要增加重传次数，并 double 重传间隔时间，保证 receiver 有足够时间处理接收到的数据。如果不 double 重传间隔时间，可能会导致重传频率过高；
5. receive 可能会接收到部分确认的 message，这时候需要用 window size 减去剩余的 seqno number，仅仅保留完全确认的部分；
6. receive 可能接受到 message of the invalid message，比如旧的 message，或者异常的 message；
7. TCPConfig::MAX_PAYLOAD_SIZE 仅仅是限制 payload 的大小，不限制 syn 或者 fin；
