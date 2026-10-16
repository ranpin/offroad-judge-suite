# 加密考题数据 2021.08.09
# 考题目录(以Raw-001为例)
# -Raw-001
#   -Raw-001-Calib (标定参数，不进行加密)
#   -Raw-001-Camera (图片，不进行加密)
#   -Raw-001-Groundtruth (真值文件，进行加密，此部分加密方式对车队不透明)
#   -Raw-001-Camera-Timestamp.csv (单目相机时间戳和图片文件对应，进行加密)
#   -Raw-001-Canbus-Gear.csv (CAN总线挡位，进行加密)
#   -Raw-001-Canbus-Odom.csv (CAN总线编码器，进行加密)
#   -Raw-001-Canbus-Steer.csv (CAN总线方向盘，进行加密)
#   -Raw-001-GPS.csv (GPS，进行加密)
#   -Raw-001-IMU.csv (GPS，进行加密)
#   -Raw-001-HDL32.pcap (多线激光雷达，进行加密)
#   -Raw-001-URG.lms (单线激光雷达，进行加密)
#   -Raw-001-infor.xlsx (不进行加密)
#   -Raw-001-Task.txt (不进行加密)

import os
import configparser
import shutil
import copy
import random
import struct
import time

CONFIG_PATH="config.ini"

def parse_config_file(config_path):
    cf = configparser.ConfigParser()
    cf.read(CONFIG_PATH)
    #secs=cf.sections()
    #print(secs)
    #options=cf.options("Paths")
    #print(options)
    try:
        raw_dir = cf.get("Paths","raw_data_dir")
        encrypt_dir = cf.get("Paths", "encrypt_data_dir")
        token_file_name = cf.get("Paths", "token_file_name")
        gt_file_name = cf.get("Paths", "groundtruth_file_name")
        data_name = cf.get("Names", "data_name")
        data_types = cf.options("DataFiles")
        data_files = {}
        for dt in data_types:
            data_files[dt] = cf.get("DataFiles", dt)
        tags = {}
        for dt in data_types:
            tags[dt] = cf.get("Tags", dt)
        sub_dirs_options = cf.options("SubDirs")
        sub_dirs = []
        for option in sub_dirs_options:
            sub_dirs.append(cf.get("SubDirs", option))
        
        token_min = int(cf.get("TokenRange", "min"))
        token_max = int(cf.get("TokenRange", "max"))
        gt_token = int(cf.get("TokenRange", "gt_token"))

    except:
        print("[ERROR]: Read config file %s failed, Please check if config file exists or the file's format is valid" % config_path)
        return None
    else:
        return raw_dir, encrypt_dir, token_file_name, gt_file_name, data_name, data_files, data_types, sub_dirs, tags, token_min, token_max, gt_token

def encrypt_string(s, token_min, token_max):
    s_ = ''
    token = random.randint(token_min, token_max)
    for i in range(len(s)):
        s_ += chr(ord(s[i]) - token) if token < 5 else chr(ord(s[i]) + token)
    return s_, token

def encrypt_string2(s, token):
    s_ = ''
    for i in range(len(s)):
        s_ += chr(ord(s[i]) - token) if token < 5 else chr(ord(s[i]) + token)
    return s_

def encrypt_shift_byte(byte, token):
    # return str(struct.pack('B', (byte + token) % 256))
    # return str( (byte+token) % 256)
    # return int(struct.pack('B', (byte + token) % 256))
    return (byte + token) % 256

def encrypt_data_csv(data_type, source_data_file, target_data_file, tags, token_min, token_max):
    print("[INFO]: encrypt %s data from source file %s to target file %s" % (data_type, source_data_file, target_data_file))
    target_csv = open(target_data_file, 'w')
    token_list = []
    with open(source_data_file, 'r', encoding='utf-8') as src_csv:
        listOfLines = src_csv.readlines()
        # always suppose first line is names of each column
        target_csv.write(listOfLines[0])
        for line in listOfLines[1:]:
            timestamp, data = line.split(',', 1)
            # discard ending '\n'
            data = data[:-1]
            s_, token = encrypt_string(data, token_min, token_max)
            token_list.append([timestamp, tags[data_type], str(token)])
            target_csv.write(timestamp +',' + s_ + '\n')
    target_csv.close()
    return token_list

def encrypt_gt_csv(source_gt_dir, target_gt_dir, token):
    print("[INFO]: encrypt gt from source dir %s to target dir %s" % (source_gt_dir, target_gt_dir))
    if not os.path.exists(target_gt_dir): os.mkdir(target_gt_dir)
    for file in os.listdir(source_gt_dir):
        if file[-4:]=='.csv':
            with open(os.path.join(source_gt_dir,file), 'r', encoding='utf-8') as f:
                s = f.read()
                s_ = encrypt_string2(s,token)
                if not os.path.exists(target_gt_dir): os.mkdir(target_gt_dir)
                with open(os.path.join(target_gt_dir,file), 'w', encoding='utf-8') as wf:
                    wf.write(s_)
    su_dir = os.path.join(source_gt_dir,'files')
    if os.path.exists(su_dir):
        su_target_dir = os.path.join(target_gt_dir,'files')
        os.mkdir(su_target_dir)
        for file in os.listdir(su_dir):
            if file[:-4] == '.csv':
                with open(os.path.join(su_dir, file), 'r', encoding='utf-8') as f:
                    s = f.read()
                    s_ = encrypt_string2(s,token)
                    with open(os.path.join(su_target_dir, file), 'w', encoding='utf-8') as wf:
                        wf.write(s_)
    return

def encrypt_data_urg(data_type, source_data_file, target_data_file, tags, token_min, token_max):
    print("[INFO]: encrypt %s data from source file %s to target file %s" % (data_type, source_data_file, target_data_file))
    target_lms = open(target_data_file, 'wb')
    token_list = []
    with open(source_data_file, 'rb') as src_lms:
        # header contains: 角度范围(float, 4字节), 角分辨率(float, 4字节), 长度单位(float, 4字节)
        header = src_lms.read(12)
        target_lms.write(header)
        rang, resolution, unit = struct.unpack('3f', header)
        # data struct: 时间戳(int, 4字节), 激光束长度(short, 2字节)* N, N = 角度范围/角分辨率 + 1
        length = int(rang/resolution) + 1
        while True:
            data_header = src_lms.read(4)
            target_lms.write(data_header)
            if not data_header:
                break
            t, = struct.unpack('i', data_header)
            data = src_lms.read(length * 2)
            token = random.randint(token_min, token_max)
            token_list.append([str(t), tags[data_type], str(token)])
            buffer = []    
            for byte in data:
                encrypted_byte = encrypt_shift_byte(byte, token)
                buffer.append(encrypted_byte)
            for item in buffer:
                assert item>=0 and item <= 255
            # target_lms.write(encrypted_byte)
            l = target_lms.write(bytearray(buffer))
            # print("Write %d bytes of %s data " % (l, data_type))
    target_lms.close()
    return token_list

def encrypt_data_hdl(data_type, source_data_file, target_data_file, tags, token_min, token_max):
    print("[INFO]: encrypt %s data from source file %s to target file %s" % (data_type, source_data_file, target_data_file))
    target_pcap = open(target_data_file, 'wb')
    token_list = []
    with open(source_data_file, 'rb') as src_pcap:
        pcap_header = src_pcap.read(24)
        target_pcap.write(pcap_header)
        token = random.randint(token_min, token_max)
        pred_ts = 0
        while True:
            data_header = src_pcap.read(16)
            target_pcap.write(data_header)
            if not data_header:
                break
            else:
                # print("One pack")
                pass
            length = struct.unpack('i', data_header[-4:])[0]
            sec = struct.unpack('i', data_header[:4])[0]
            msec = struct.unpack('i', data_header[4:8])[0]
            localt = time.localtime(sec)
            ts = (localt.tm_hour * 3600 + localt.tm_min * 60 + localt.tm_sec) * 1000 + msec // 1000
            if ts != pred_ts:
                token = random.randint(token_min, token_max)
                pred_ts = ts
                token_list.append([str(ts), tags[data_type], str(token)])    
            data = src_pcap.read(length)
            buffer = []
            for byte in data:
                encrypted_byte = encrypt_shift_byte(byte, token)
                buffer.append(encrypted_byte)
            for item in buffer:
                assert item>=0 and item <= 255
            l = target_pcap.write(bytearray(buffer))
            # print("Write %d bytes of %s data " % (l, data_type))
    target_pcap.close()
    return token_list


def write_token_file(token_list, file_name):
    with open(file_name, 'w') as f:
        f.write("timestamp,type,token\n")
        for item in token_list:
            f.write(item[0] + ',' + item[1] + ',' + item[2] + '\n')
    
if __name__ == "__main__":
    # read config file
    ret = parse_config_file(CONFIG_PATH)
    if ret is None:
        exit(1)
    else:
        raw_dir, encrypt_dir, token_file_name, gt_file_name, data_name, data_files, data_types, sub_dirs, tags, token_min, token_max, gt_token = ret
        print("[INFO] configuration:")
        print("Raw data directory: %s" % raw_dir)
        print("Encrypt data directory: %s" % encrypt_dir)
        print("Token file name: %s" % token_file_name)
        print("Data name: %s" % data_name)
        print("Token tags: ", tags)

    # check paths and files
    assert os.path.exists(raw_dir), "[ERROR]: Raw data directory %s does not exist !" % raw_dir
    assert os.path.exists(os.path.join(raw_dir,gt_file_name)), "[ERROR]: Groundtruth file %s does not exist !" % gt_file_name
    assert not os.path.exists(encrypt_dir), "[ERROR]: Encrypt data directory %s already exists, please check or remove it" % encrypt_dir
    sub_dirs = [data_name + '-' + item for item in sub_dirs]
    for item in sub_dirs:
        assert os.path.exists(os.path.join(raw_dir, item)), "[ERROR]: sub data directory %s does not exist !" % item    
    for key in data_files.keys():
        data_files[key] = data_name + '-' + data_files[key]
    for dt in data_types:
        assert os.path.exists(os.path.join(raw_dir, data_files[dt]))
    print("[INFO] data files to encrypt")
    for file in data_files.values():
        print("%s" % file)

    # create encrypt output directory,
    # copy some sub directories into this new directory
    os.mkdir(encrypt_dir)
    print("Copy files ...")
    for sub_dir in sub_dirs:
        shutil.copytree(os.path.join(raw_dir, sub_dir), os.path.join(encrypt_dir, sub_dir))
        
    # encrypt data files for each data type
    encrypt_gt_csv(os.path.join(raw_dir,gt_file_name),os.path.join(encrypt_dir,gt_file_name),gt_token)
    total_token_list = []
    for dt in data_types:
        if dt not in ['urg', 'hdl']:
            token_list = encrypt_data_csv(dt, 
                                        os.path.join(raw_dir, data_files[dt]), 
                                        os.path.join(encrypt_dir, data_files[dt]),
                                        tags,
                                        token_min,
                                        token_max)
        else:
            if dt == 'urg':
                token_list = encrypt_data_urg(dt, 
                                        os.path.join(raw_dir, data_files[dt]),
                                        os.path.join(encrypt_dir, data_files[dt]),
                                        tags,
                                        token_min,
                                        token_max)
            elif dt == 'hdl':
                token_list = encrypt_data_hdl(dt, 
                                        os.path.join(raw_dir, data_files[dt]),
                                        os.path.join(encrypt_dir, data_files[dt]),
                                        tags,
                                        token_min,
                                        token_max)
            else:
                pass
        total_token_list.extend(token_list)
    write_token_file(total_token_list, token_file_name)
    print("[INFO]: Done.")