import time
import numpy as np
import uhd

def add_pulse(data):
    new_value = np.complex64(1 + 1j)
    new_values = np.concatenate(([new_value], data))
    return new_values

if __name__ == "__main__":
    args = "addr=127.0.0.1"
    usrp = uhd.usrp.MultiUSRP(args)


    # 加载数据
    data = np.load('/home/zhj/data/complex_data.npy')

    # 应用add_pulse函数到每一行
    data = np.apply_along_axis(add_pulse, axis=1, arr=data)

    # 将数据展平成一维数组
    data = data.flatten()

    rate = 1e6
    # 通道
    channels = [0]
    # 增益
    gain = 60

    usrp.send_waveform(data, data.size/rate, 1.5e9, rate, channels, gain)


* text=auto eol=lf
