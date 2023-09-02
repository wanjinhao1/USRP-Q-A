import socket
import threading
import struct
import math
import os

stop_all_threads = False

def read_input(server_sock):
    global stop_all_threads
    input("按 Enter 停止服务器")
    stop_all_threads = True
    server_sock.close()
    print("停止接收，关闭套接字...")
    os._exit(0)  # 正常退出程序

def recv_and_save(client_sock, last_octet):
    received_data = []  # 存储接收到的数据
    start_saving = False  # 初始时不保存数据
    save_count = 0  # 初始化保存计数器

    # 循环接收数据
    while not stop_all_threads:
        data = client_sock.recv(4096)
        if not data:
            break

        if len(received_data) < 1024:
            received_data.extend(struct.unpack('1024f', data))

        if len(received_data) >= 1024:
            for data in received_data:
                if not start_saving and abs(data.real) > 0.10:
                    start_saving = True

                if start_saving:
                    with open(f"data_{last_octet}.bin", "ab") as myfile:
                        myfile.write(struct.pack('f', data))
                    save_count += 1
                    if save_count >= 2000000:
                        start_saving = False
                        save_count = 0
            received_data.clear()

    client_sock.close()
    print("finished recv")

def main():
    global stop_all_threads

    server_sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    server_sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    server_addr = ('', 12345)

    try:
        server_sock.bind(server_addr)
        server_sock.listen(1)
        print("\n------------------------------服务器正在监听连接-----------------------------")
    except Exception as e:
        print("无法绑定套接字")
        server_sock.close()
        return

    input_thread = threading.Thread(target=read_input, args=(server_sock,))
    input_thread.start()

    client_count = 0
    while not stop_all_threads:
        try:
            client_sock, client_addr = server_sock.accept()
        except Exception as e:
            print("无法接受客户端连接")
            server_sock.close()
            return

        client_count += 1
        print(f"\n创建一个新线程处理连接...")
        client_ip = client_addr[0]
        print(f"客户端已连接。IP地址: {client_ip}")

        last_octet = int(client_ip.split('.')[-1])
        print(f"接收并检测 节点 {last_octet} 数据中...")

        client_thread = threading.Thread(target=recv_and_save, args=(client_sock, last_octet))
        client_thread.start()

        if client_count >= 6:
            print("\n------------------------------六节点连接成功！------------------------------")
            break

    input_thread.join()
    print("finished")

if __name__ == "__main__":
    main()
* text=auto eol=lf
