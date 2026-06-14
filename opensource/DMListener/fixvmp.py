from PIL import Image
import os

def fix_bmp_format(file_path):
    """
    使用 Pillow 库修复 BMP 文件的格式。
    读取文件，然后以标准的 BMP 格式重新保存，覆盖原文件。
    """
    if not os.path.exists(file_path):
        print(f"错误：文件不存在：{file_path}")
        return

    try:
        # 打开图像文件
        img = Image.open(file_path)

        # 检查文件是否为 BMP 格式
        if img.format.upper() != 'BMP':
            print(f"警告：文件不是 BMP 格式，而是 {img.format}。尝试转换为 BMP。")

        # 将图像转换为 RGB 模式，这是标准的 BMP 格式所要求的
        img = img.convert("RGB")

        # 以 BMP 格式保存，这将确保它是一个标准的、可被大多数程序识别的文件
        # 参数 'BMP' 是 Pillow 用于识别 BMP 格式的字符串
        img.save(file_path, 'BMP')
        print(f"成功修复并重新保存文件：{file_path}")

    except Exception as e:
        print(f"修复文件时发生错误：{file_path}")
        print(f"错误信息：{e}")

# 请将此处的文件路径替换为你的实际文件路径
bmp_file_path = r"D:\SteamLibrary\steamapps\common\Counter-Strike Global Offensive\game\csgo\cfg\Summer\cpp\DM监听器\background.bmp"
fix_bmp_format(bmp_file_path)