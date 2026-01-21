import tkinter as tk
from tkinter import filedialog, messagebox
import threading
import subprocess
import json
import os
import sys

try:
    import ffmpeg
except ImportError:
    import pip
    pip.main(['install', 'ffmpeg-python'])
    import ffmpeg

# 获取音频总时长（秒）
def get_duration(path):
    try:
        result = subprocess.run(
            ["ffprobe", "-v", "error", "-show_entries",
             "format=duration", "-of", "json", path],
            stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True
        )
        data = json.loads(result.stdout)
        return float(data["format"]["duration"])
    except Exception as e:
        messagebox.showerror("错误", f"获取音频时长失败:\n{e}")
        return 0.0

# 构建淡入淡出音量表达式
def build_fade_volume_expr(duration, fade_in, fade_out, base_volume):
    base = base_volume / 100.0
    expr = f"""
        if(lt(t,{fade_in}),
            {base}*t/{fade_in},
            if(gt(t,{duration - fade_out}),
                {base}*(1 - (t - {duration - fade_out})/{fade_out}),
                {base}
            )
        )
    """
    return expr.replace('\n', '').replace('  ', '').strip()

# 处理音频函数（截取+音量+淡入淡出）
def process_audio(infile, outfile, volume, start, end, fade_in, fade_out):
    try:
        real_duration = end - start
        expr = build_fade_volume_expr(real_duration, fade_in, fade_out, volume)

        stream = ffmpeg.input(infile, ss=start, to=end)
        audio = stream.audio.filter('volume', volume=expr)
        out_stream = ffmpeg.output(audio, outfile, vn=None)
        out, err = ffmpeg.run(out_stream, capture_stdout=True, capture_stderr=True)

        os.remove(infile)
        os.rename(outfile, infile)
        messagebox.showinfo("成功", f"处理完成！\n音量：{volume}%\n淡入：{fade_in}s\n淡出：{fade_out}s\n截取：{start:.2f}s - {end:.2f}s")
    except ffmpeg.Error as e:
        stderr = e.stderr.decode().strip()
        print(stderr, file=sys.stderr)
        messagebox.showerror("错误", f"FFmpeg 处理失败:\n{stderr}")

def format_seconds_to_hms(seconds: float) -> str:
    seconds = int(seconds)
    m, s = divmod(seconds, 60)
    return f"{m:02}:{s:02}"


# 滑块+输入框组件工厂
def make_slider_entry(parent, label_text, var, frm, to, resolution):
    frame = tk.Frame(parent)
    frame.pack(fill='x', padx=10, pady=5)
    tk.Label(frame, text=label_text, width=12).pack(side='left')

    slider = tk.Scale(frame, from_=frm, to=to,
                      resolution=resolution,
                      orient='horizontal', variable=var,
                      showvalue=False, length=200)
    slider.pack(side='left', padx=5)

    # 显示秒数输入框
    entry = tk.Entry(frame, width=6, justify='center', state='readonly')
    entry.pack(side='left', padx=5)
    entry_var = tk.StringVar(value=f"{var.get():.2f}")
    entry.config(textvariable=entry_var)

    # 显示 mm:ss 标签
    time_label = tk.Label(frame, text=format_seconds_to_hms(var.get()), width=6)
    time_label.pack(side='left', padx=5)

    def on_slider_change(val):
        entry_var.set(f"{float(val):.2f}")
        time_label.config(text=format_seconds_to_hms(float(val)))

    var.trace_add('write', lambda *args: on_slider_change(var.get()))

    def apply_entry(_=None):
        try:
            raw = entry.get()
            v = float(raw)
        except ValueError:
            v = var.get()
        v = max(frm, min(to, v))
        var.set(v)
        entry_var.set(f"{v:.2f}")
        time_label.config(text=format_seconds_to_hms(v))
        entry.config(state='readonly')

    def on_entry_focus_in(event):
        entry.config(state='normal')
        entry.select_range(0, 'end')

    entry.bind('<FocusIn>', on_entry_focus_in)
    entry.bind('<Return>', apply_entry)
    entry.bind('<FocusOut>', apply_entry)

    return slider, entry
    def on_slider_change(val):
        entry_var.set(f"{float(val):.2f}")

    var.trace_add('write', lambda *args: on_slider_change(var.get()))

    def apply_entry(_=None):
        try:
            raw = entry.get()
            v = float(raw)
        except ValueError:
            v = var.get()
        v = max(frm, min(to, v))
        var.set(v)
        entry_var.set(f"{v:.2f}")
        entry.config(state='readonly')

    def on_entry_focus_in(event):
        entry.config(state='normal')
        entry.select_range(0, 'end')

    entry.bind('<FocusIn>', on_entry_focus_in)
    entry.bind('<Return>', apply_entry)
    entry.bind('<FocusOut>', apply_entry)

    return slider, entry

# 主窗口
root = tk.Tk()
root.title("音频音量调节 & 截取 & 淡入淡出")
root.geometry("520x450")

file_path = tk.StringVar()
duration = 0.0

def choose_file():
    global duration
    path = filedialog.askopenfilename(
        filetypes=[("音频文件", "*.mp3;*.wav;*.flac;*.aac")]
    )
    if not path:
        return
    file_path.set(path)
    duration = get_duration(path)
    start_var.set(0.0)
    end_var.set(duration)
    start_slider.config(to=duration)
    end_slider.config(to=duration)
    file_label.config(text=os.path.basename(path))

tk.Button(root, text="选择音频文件", command=choose_file).pack(pady=10)
file_label = tk.Label(root, textvariable=file_path, wraplength=480)
file_label.pack()

# 变量初始化
volume_var = tk.DoubleVar(value=100.0)
start_var = tk.DoubleVar(value=0.0)
end_var = tk.DoubleVar(value=0.0)
fade_in_var = tk.DoubleVar(value=0.5)
fade_out_var = tk.DoubleVar(value=0.5)

# 生成滑块+输入框
make_slider_entry(root, "音量 (%)", volume_var, 0, 200, 1.0)
start_slider, _ = make_slider_entry(root, "开始时间 (s)", start_var, 0, duration, 0.1)
end_slider, _ = make_slider_entry(root, "结束时间 (s)", end_var, 0, duration, 0.1)
make_slider_entry(root, "淡入时间 (s)", fade_in_var, 0, 10, 0.1)
make_slider_entry(root, "淡出时间 (s)", fade_out_var, 0, 10, 0.1)

def on_process():
    infile = file_path.get()
    if not infile:
        messagebox.showwarning("警告", "请先选择音频文件")
        return
    if start_var.get() >= end_var.get():
        messagebox.showwarning("警告", "开始时间必须小于结束时间")
        return
    if fade_in_var.get() + fade_out_var.get() > (end_var.get() - start_var.get()):
        messagebox.showwarning("警告", "淡入和淡出总时间不能超过截取时长")
        return

    base, ext = os.path.splitext(infile)
    tmpfile = base + "_tmp" + ext

    threading.Thread(
        target=process_audio,
        args=(infile, tmpfile,
              volume_var.get(),
              start_var.get(),
              end_var.get(),
              fade_in_var.get(),
              fade_out_var.get()),
        daemon=True
    ).start()

tk.Button(root, text="开始处理", command=on_process).pack(pady=20)

root.mainloop()
