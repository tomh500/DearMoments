import os
import subprocess

def b2():
    print("请选择转换方向：")
    print("1. ogg → wav")
    print("2. wav → ogg")
    print("3. 所有音频转换为 wav 格式")
    x = input("请输入数字（1、2 或 3）：").strip()
    if x == '1':
        return '.ogg', '.wav'
    elif x == '2':
        return '.wav', '.ogg'
    elif x == '3':
        return None, None
    else:
        print("无效选择，程序退出。")
        exit(1)

def d3(c1, c2):
    if c1 is None and c2 is None:
        d4()
    else:
        for c5, _, c6 in os.walk('.'):
            for c7 in c6:
                if c7.lower().endswith(c1):
                    c8 = os.path.join(c5, c7)
                    c9 = c7[:-len(c1)] + c2
                    c10 = os.path.join(c5, c9)

                    if os.path.exists(c10):
                        print(f"[跳过] 目标已存在: {c10}")
                        continue

                    try:
                        subprocess.run([
                            'ffmpeg', '-y',
                            '-i', c8,
                            c10
                        ], check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
                        print(f"[完成] {c8} → {c10}")
                    except subprocess.CalledProcessError:
                        print(f"[错误] 转换失败: {c8}")

def d4():
    c11 = ['.m4a', '.mp3', '.mp4', '.flac', '.ogg']
    for c5, _, c6 in os.walk('.'):
        for c7 in c6:
            if any(c7.lower().endswith(c) for c in c11):
                c8 = os.path.join(c5, c7)
                wav_path = os.path.join(c5, os.path.splitext(c7)[0] + '.wav')

                if os.path.exists(wav_path):
                    print(f"[跳过] 目标已存在: {wav_path}")
                    continue

                try:
                    subprocess.run([
                        'ffmpeg', '-y',
                        '-i', c8,
                        wav_path
                    ], check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
                    print(f"[完成] {c8} → {wav_path}")

                    # 删除原文件
                    os.remove(c8)

                except subprocess.CalledProcessError:
                    print(f"[错误] 转换失败: {c8}")


if __name__ == "__main__":
    c1, c2 = b2()
    d3(c1, c2)
