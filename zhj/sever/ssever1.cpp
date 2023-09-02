#include <iostream>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <vector>
#include <complex>
#include <thread>
#include <fstream>
#include <atomic>
#include <cstdlib>


std::atomic_bool stop_all_threads(false);

void read_input(int server_fd) {
    std::cout << "\n--------------------------Press enter to stop server-------------------------" << std::endl;
    std::cin.get();
    stop_all_threads.store(true);
    close(server_fd);  // 关闭套接字
    std::cout << "停止接收，关闭套接字..." << std::endl;
    std::exit(0);  // 正常退出程序
}

void recvandsave(int client_fd,int a) //接收数据并进行保存 client_fd，代表一个客户端的文件描述符。
{
    std::vector<std::complex<float>> received_data(1024);  // 创建一个2048大小的复数向量，存储接收到的数据
    // 打开一个文件用于写入接收到的数据，文件以追加模式打开（app模式）
    std::ostringstream filename;
    filename << "/home/zhj/20230807/data/data_" << a << ".bin";
    std::ofstream myfile(filename.str(), std::ios::binary);

    // 循环接收数据
    while (!stop_all_threads.load())
    {
        // 用于记录已接收数据的字节数
        ssize_t total_bytes_received = 0;
        // 当接收的字节数小于需要接收的总字节数时，继续接收
        while (total_bytes_received < received_data.size() * sizeof(std::complex<float>)) {
            ssize_t num_bytes_received = recv(client_fd, received_data.data() + total_bytes_received / sizeof(std::complex<float>),
                                            (received_data.size() * sizeof(std::complex<float>)) - total_bytes_received, 0);
            if (num_bytes_received < 0) {
                std::cerr << "无法接收数据" << std::endl;
                close(client_fd);
                return;
            }
            total_bytes_received += num_bytes_received;
        }
        // 将接收到的数据写入文件
        for (const auto& data : received_data) {
            myfile.write(reinterpret_cast<const char*>(&data), sizeof(data));
        }
    }
    myfile.close();
    close(client_fd);
    std::cout << "finished recv" << std::endl;
}

int main()
{
    int server_fd, client_fd;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_addr_len = sizeof(client_addr);
    
    // 创建套接字
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        std::cerr << "无法创建套接字" << std::endl;
        return 1;
    }

    // 创建线程时，使用 std::ref 来传递引用
    std::thread input_thread(read_input, std::ref(server_fd));
    // 设置服务端地址和端口
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(12345);
    
    // 将套接字绑定到服务端地址和端口
    if (bind(server_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        std::cerr << "无法绑定套接字" << std::endl;
        close(server_fd);
        return 1;
    }
    
    // 监听连接请求
    if (listen(server_fd, 1) < 0) {
        std::cerr << "无法监听连接请求" << std::endl;
        close(server_fd);
        return 1;
    }
    
    std::cout << "\n------------------------------服务器正在监听连接-----------------------------" << std::endl;

    int client_count = 0; // 定义一个计数器
    // 接受客户端连接并在单独线程中处理
    while (!stop_all_threads.load()) {
        // 接受客户端连接
        if ((client_fd = accept(server_fd, (struct sockaddr*)&client_addr, &client_addr_len)) < 0) {
            std::cerr << "无法接受客户端连接" << std::endl;
            close(server_fd);
            return 1;
        }
            // 计数器加一
        client_count++;
        std::cout << "\n创建一个新线程处理连接..." << std::endl;

        // 将客户端的 IP 地址转换为字符串形式
        char client_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &(client_addr.sin_addr), client_ip, INET_ADDRSTRLEN);

        std::cout << "客户端已连接。IP地址: " << client_ip << std::endl;
        
        std::string ip_str(client_ip);
        size_t last_dot_position = ip_str.rfind('.');
        std::string last_octet_str = ip_str.substr(last_dot_position + 1);
        int last_octet = std::stoi(last_octet_str);

        // 创建一个新线程来处理客户端连接
        std::thread client_thread(recvandsave, client_fd, last_octet);
        std::cout << "保存" << "节点"<< last_octet << "数据中..." << std::endl;

        // 分离线程，使其能够独立运行
        client_thread.detach();

        if (client_count >= 6) {
        std::cout << "\n------------------------------六节点连接成功！------------------------------" << std::endl;
        break;}
    }

    // 等待输入线程结束
    input_thread.join();    
    std::cout << "finished" << std::endl;
    return 0;
}
* text=auto eol=lf
