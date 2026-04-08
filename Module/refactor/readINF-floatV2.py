import math
import struct
import os
import matplotlib.pyplot as plt


def read_dat_file(dat_path, addr, length, attrib_dt):
    with open(dat_path, 'rb') as f:
        dat_size = os.path.getsize(dat_path)
        if attrib_dt == 2:
            need_bytes = 2 * length
            fmt = '<' + 'h' * length  # S16
        elif attrib_dt == 3:
            need_bytes = 4 * length
            fmt = '<' + 'f' * length  # float
        else:
            raise ValueError("未知的attrib_dt")
        if addr + need_bytes > dat_size:
            print(f"请求读取范围超出DAT文件实际大小: 需要{need_bytes}字节, addr={addr}, DAT文件总大小={dat_size}")
            return []
        f.seek(addr)
        buf = f.read(need_bytes)
        if len(buf) < need_bytes:
            print("数据不足，无法解包")
            return []
        data = struct.unpack(fmt, buf)
    return data


def print_first_three_raw_data(dat_path, addr, attrib_dt):
    """
    打印指定地址开始的前三个数据点的原始十六进制和十进制值
    """
    with open(dat_path, 'rb') as f:
        f.seek(addr)
        print("前三个数据点的原始值:")
        for i in range(3):
            if attrib_dt == 2:  # S16 (2字节)
                raw_bytes = f.read(2)
                if len(raw_bytes) < 2:
                    print(f"  点 {i+1}: 数据不足")
                    break
                hex_value = raw_bytes.hex()
                dec_value = struct.unpack('<h', raw_bytes)[0]
                print(f"  点 {i+1}: 十六进制=0x{hex_value.upper()}, 十进制={dec_value}")
            elif attrib_dt == 3:  # float (4字节)
                raw_bytes = f.read(4)
                if len(raw_bytes) < 4:
                    print(f"  点 {i+1}: 数据不足")
                    break
                hex_value = raw_bytes.hex()
                dec_value = struct.unpack('<f', raw_bytes)[0]
                print(f"  点 {i+1}: 十六进制=0x{hex_value.upper()}, 十进制={dec_value}")
        print()

def plot_data(data, chnl_id):
    plt.figure()
    plt.plot(data)
    plt.title(f'chnl_id: {chnl_id}')
    plt.xlabel('Index')
    plt.ylabel('Value')
    #plt.show()


def plot_ddr_data(data, chnl_id, sampling_rate=12.5e6):
    """
    绘制DDR原始数据图表
    :param data: 数据列表
    :param chnl_id: 通道ID
    :param sampling_rate: 采样率，默认12.5MHz
    """

    # 将原始数据转换为角度（单位：度）
    converted_data = [(value / 8192.0) * (180 / math.pi) for value in data]

    # 计算时间轴，单位为毫秒
    time_axis_ms = [i / sampling_rate * 1000 for i in range(len(data))]

    plt.figure()
    plt.plot(time_axis_ms, data)
    plt.title(f'DDR-dat - chnl_id: {chnl_id}')
    plt.xlabel('Time (ms)')
    plt.ylabel('Angle (degrees)')
    #plt.show()
def read_inf_file(file_path):
    # 获取inf文件的基本名称（不含路径和扩展名）
    base_name = os.path.basename(file_path).rsplit('.', 1)[0]

    # 构造dat文件路径：上级目录的DATA文件夹下
    inf_dir = os.path.dirname(file_path)
    parent_dir = os.path.dirname(inf_dir)

    # 根据文件名特征判断DAT文件名
    if "DDR3_DATA" in base_name:
        # 如果文件名包含DDR3_DATA，则DAT文件名需要添加_CH1后缀
        dat_path = os.path.join(parent_dir, 'DATA', base_name + '_CH1.DAT')
    else:
        # 否则使用原始文件名
        dat_path = os.path.join(parent_dir, 'DATA', base_name + '.DAT')

    if not os.path.exists(dat_path):
        print(f"对应的DAT文件不存在: {dat_path}")
        return
    with open(file_path, 'rb') as f:
        file_size = os.path.getsize(file_path)
        record_size = 122  # InfPara结构体大小
        num_records = file_size // record_size

        print(f"文件: {file_path}")
        print(f"大小: {file_size}字节")
        print(f"记录数: {num_records}\n")

        figures_created = False  # 标记是否创建了图表

        for i in range(num_records):
            # 读取 InfPara 结构体
            filetype = f.read(10).decode('utf-8', errors='ignore').strip('\x00')
            chnl_id = struct.unpack('<h', f.read(2))[0]
            chnl = f.read(12).decode('utf-8', errors='ignore').strip('\x00')
            addr = struct.unpack('<i', f.read(4))[0]
            freq = struct.unpack('<f', f.read(4))[0]  # float freq (4字节)
            length = struct.unpack('<i', f.read(4))[0]
            post = struct.unpack('<i', f.read(4))[0]
            max_dat = struct.unpack('<H', f.read(2))[0]
            low_rang = struct.unpack('<f', f.read(4))[0]
            high_rang = struct.unpack('<f', f.read(4))[0]
            factor = struct.unpack('<f', f.read(4))[0]
            offset = struct.unpack('<f', f.read(4))[0]
            unit = f.read(8).decode('utf-8', errors='ignore').strip('\x00')
            dly = struct.unpack('<f', f.read(4))[0]
            attrib_dt = struct.unpack('<h', f.read(2))[0]
            dat_wth = struct.unpack('<h', f.read(2))[0]
            spar_i1 = struct.unpack('<h', f.read(2))[0]
            spar_i2 = struct.unpack('<h', f.read(2))[0]
            spar_i3 = struct.unpack('<h', f.read(2))[0]
            spar_f1 = struct.unpack('<f', f.read(4))[0]
            spar_f2 = struct.unpack('<f', f.read(4))[0]
            spar_c1 = f.read(8).decode('utf-8', errors='ignore').strip('\x00')
            spar_c2 = f.read(16).decode('utf-8', errors='ignore').strip('\x00')
            spar_c3 = f.read(10).decode('utf-8', errors='ignore').strip('\x00')

            # 显示记录内容
            print(f"记录 #{i + 1}:")
            print(f"filetype: {filetype}")
            print(f"chnl_id: {chnl_id}")
            print(f"chnl: {chnl}")
            print(f"addr: {addr}")
            print(f"freq: {freq}")
            print(f"len: {length}")
            print(f"post: {post}")
            print(f"maxDat: {max_dat}")
            print(f"lowRang: {low_rang}")
            print(f"highRang: {high_rang}")
            print(f"factor: {factor}")
            print(f"Offset: {offset}")
            print(f"unit: {unit}")
            print(f"Dly: {dly}")
            print(f"attribDt: {attrib_dt}")
            print(f"datWth: {dat_wth}")
            print(f"sparI1: {spar_i1}")
            print(f"sparI2: {spar_i2}")
            print(f"sparI3: {spar_i3}")
            print(f"sparF1: {spar_f1}")
            print(f"sparF2: {spar_f2}")
            print(f"sparC1: {spar_c1}")
            print(f"sparC2: {spar_c2}")
            print(f"sparC3: {spar_c3}")
            print()

            # 读取DAT数据并单独画图（不show）
            try:
                # 打印前三个点的原始数据
                print_first_three_raw_data(dat_path, addr, attrib_dt)

                data = read_dat_file(dat_path, addr, length, attrib_dt)
                if attrib_dt == 2:  # DDR数据
                    plot_ddr_data(data, chnl_id)
                else:
                    plot_data(data, chnl_id)
                figures_created = True
                # 不在这里show
            except Exception as e:
                print(f"读取DAT或画图出错: {e}")

    # 循环外统一show，所有图一起弹出
    # 循环结束后，一次性显示所有图表
    if figures_created:
        plt.show()

# 主程序
while True:
    print("合并文件解析工具 v250814")
    print("- 1. 支持原始数据目录结构：自动识别INF目录、DATA目录")
    print("- 2. 支持DDR原始数据解析：针对单频模式优化（仅保留三类参数 三频模式仍然是五类），推荐使用适配版本的采集软件")
    print("- 使用方法：将INF目录内的合并后的INF文件拖拽到本窗口（如：90033ng1.INF），或输入文件路径")
    print("- 注意事项：确保文件路径不包含中文、空格等特殊字符")
    path = input("输入INF文件路径(q退出): ")
    if path.lower() == 'q':
        break

    if os.path.exists(path):
        try:
            read_inf_file(path)
        except Exception as e:
            print(f"读取错误: {e}")
    else:
        print("文件不存在")