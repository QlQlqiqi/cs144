### cs144

代码 init 参考 [blog](https://github.com/PKUFlyingPig/CS144-Computer-Network)

##### lab0
弄了很久，主要是环境问题：[blog](https://blog.csdn.net/J__M__C/article/details/131713326)

还有一个点是，不要在 write/read 还未执行完之前，用 shutdown 关闭 socket。

用 std::deque<std::string> 存储 push 的结果会比用 std::deque<char> 能快点，因为 push 和 pop 操作，前者可以一次操作多个 char。
