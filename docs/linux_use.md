### [1. 查看linux端口被占用的方法](#)
**lsof(list open files)** 命令可以列出当前系统中打开的所有文件，包括网络端口。可以使用lsof命令查看某个端口被哪个进程占用。
```shell
sudo lsof -i:15000
```
**netstat** 命令可以显示网络连接、路由表和网络接口信息等。可以使用netstat命令查看某个端口被哪个进程占用.
```shell
sudo netstat -tlnp | grep 15000
```
**ss** 命令可以列出当前系统中打开的套接字(socket)信息，包括网络端口，可以使用ss命令查看某个端口被哪个进程占用。
```shell
sudo ss -tlnp | grep 15000
```
**fuser** fuser命令可以查看某个文件或目录被哪个进程占用。对于网络端口，也可以使用fuser命令进行查询，
```shell
sudo fuser 15000/tcp
sudo fuser 15000/udp
```
**ps命令** ps命令可以列出当前系统中正在运行的进程信息。可以使用ps命令结合grep命令来查找某个进程，然后再查看该进程打开的网络端口。
```shell
sudo ps -ef | grep program_name
```
其中 program_name 为需要查询的进程名

### [2. scp命令](#)
scp 命令是用于通过 SSH 协议安全地将文件复制到远程系统和从远程系统复制文件到本地的命令。
使用 SSH 意味着它享有与 SSH 相同级别的数据加密，因此被认为是跨两个远程主机传输文件的安全方式。

```c++
 scp [option] /path/to/source/file user@server-ip:/path/to/destination/directory
```
* `/path/to/source/file` – 这是打算复制到远程主机的源文件。
* `user@server-IP` – 这是远程系统的用户名和 IP 地址。请注意 IP 地址后面加冒号。
* `/path/to/destination/directory` – 这是文件将复制到的远程系统上的目标目录。

以下是scp命令常用的几个选项：
* `-C` - 这会在复制过程中压缩文件或目录。
* `-P` - 如果默认 SSH 端口不是 22，则使用此选项指定 SSH 端口。
* `-r` - 此选项递归复制目录及其内容。
* `-p` - 保留文件的访问和修改时间。

```c++
scp ./muse-rpc.zip root@125.91.127.142:/home
```

### [3. tcpdump 命令](#)


### [4. valgrind](#)

```shell
valgrind --tool=memcheck --leak-check=full --show-leak-kinds=all --undef-value-errors=no --log-file=log
```