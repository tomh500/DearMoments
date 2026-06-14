import os, re, shutil, argparse, tkinter as tk, tkinter.font as tkfont
from tkinter import filedialog, messagebox
from asulparser import parse_asul, Directive
from asulwriter import write_cfg, save_asul
from checker import check_conflicts
from asulparser      import parse_asul, Directive
from asuljsonparser  import parse_asuljson
from asuljsonwriter import write_json
from checker         import check_conflicts
from asuljsonwriter import write_json, save_asuljson
from asulyml import parse_asulyml, write_yml, save_asulyml
import sys



KEYMAP = {
    'Left': 'leftarrow',
    'Right': 'rightarrow',
    'Up': 'uparrow',
    'Down': 'downarrow',
    'Pause': 'pause',
    'period': '.',
    'slash': '/',
    'bracketleft': '[',
    'bracketright': ']',
    'KP_Decimal': 'kp_del',
    'Caps_Lock': 'capslock',
    'Control_L': 'ctrl',
    'Control_R': 'rctrl',
    'Shift_R': 'rshift'
}


SKIP_CONFLICT = {'key','none'}

def resource_path(relative_path):
    # PyInstaller 中加载资源文件的兼容方式
    if hasattr(sys, '_MEIPASS'):
        return os.path.join(sys._MEIPASS, relative_path)
    return os.path.join(os.path.abspath("."), relative_path)


class AsulUI(tk.Tk):
    def __init__(self, queue=None, cli=False, link=False):
        super().__init__()
        self.title("Ausl_Editor@Luotiany1_Mar")
        self.set_safe_icon("Asul_Editor.ico", preferred_dir="cache")
        self.geometry("1070x1010")    #窗口大小
        
        # —— 核心修复：开启高分屏 DPI 意识，防止字体模糊
        try:
            from ctypes import windll
            windll.shcore.SetProcessDpiAwareness(1)
        except:
            pass

        self.cli_mode  = bool(cli)
        self.link_mode = bool(link)
        self.yml_mode  = False
        self.json_mode = False

        # —— 字体探测与优先级设置 (等线 > 雅黑 > 默认)
        fam = set(tkfont.families())
        # 获取系统默认字号并 +2，确保清晰
        self.base_size = abs(tkfont.nametofont("TkDefaultFont").cget("size")) + 5
        
        self.default_font_name = "TkDefaultFont" 
        # 探测优先级
        for fn in ("DengXian", "等线", "Microsoft YaHei", "微软雅黑"):
            if fn in fam:
                self.default_font_name = fn
                break
        
        # 设置全局 option（作为兜底）
        self.option_add("*Font", (self.default_font_name, self.base_size, "bold"))

        # —— 状态
        self.queue      = queue or []
        self.cur        = None
        self.output_dir = None
        self.directives = []
        self.ui_widgets = []
        self._waiting   = None 
        self.custom     = None 

        # —— 菜单
        m = tk.Menu(self); fm = tk.Menu(m, tearoff=False)
        fm.add_command(label="打开 .asul",     command=self.menu_open_asul)
        fm.add_command(label="打开 .asuljson", command=self.menu_open_json) 
        fm.add_command(label="打开 .asulyml",  command=self.menu_open_yml)
        fm.add_command(label="打开 .asulink",  command=self.menu_open_link)
        fm.add_separator(); fm.add_command(label="退出", command=self.quit)
        m.add_cascade(label="文件", menu=fm); self.config(menu=m)

        # —— 滚动画布
        
        self.canvas = tk.Canvas(self, bg='#f0f0f0'); sb = tk.Scrollbar(self, command=self.canvas.yview)
        self.canvas.configure(yscrollcommand=sb.set)
        sb.pack(side="right", fill="y"); self.canvas.pack(side="left", fill="both", expand=True)
        self.frm = tk.Frame(self.canvas, bg='#ffffff') # 内部背景颜色设为白，好看点
        self.canvas_window = self.canvas.create_window((0,0), window=self.frm, anchor="nw")
        
        self.canvas.bind('<Configure>', lambda e: self.canvas.itemconfigure(self.canvas_window, width=e.width))
        self.frm.bind("<Configure>",lambda e:self.canvas.configure(scrollregion=self.canvas.bbox("all")))
        
        # —— 水印
        self.watermark_text = "完全免费"
        self._draw_watermark()
        self.bind("<Configure>", lambda e: self._draw_watermark())

        for ev in ("<MouseWheel>","<Button-4>","<Button-5>"):
            self.canvas.bind_all(ev, self._on_mousewheel)

        # —— 生成按钮 (显式绑定字体)
        self.btn = tk.Button(self, text="生成 CFG & 更新 Asul", 
                            font=(self.default_font_name, self.base_size, "bold"),
                            state="disabled", command=self.on_generate)
        self.btn.pack(side="bottom", fill="x", pady=5)

        self.bind_all("<Button-1>", self._on_click, add=True)
        self.bind_all("<Key>", self._on_keypress)

        if self.queue: self.next_file()

    def set_safe_icon(self, icon_name, preferred_dir="cache"):
        """
        优先级：
        1. 尝试 preferred_dir (cache)
        2. 尝试当前目录
        3. 递归搜索子目录
        """
        possible_paths = [
            os.path.join(preferred_dir, icon_name), # 1. 优先去 cache 找
            icon_name                               # 2. 尝试根目录
        ]
        
        target_path = None
        
        # 先在已知路径里找
        for p in possible_paths:
            if os.path.exists(p):
                target_path = p
                break
        
        # 如果还没找到，开启全盘搜索模式（你要求的遍历子目录）
        if not target_path:
            for root, dirs, files in os.walk('.'):
                if icon_name in files:
                    target_path = os.path.join(root, icon_name)
                    break
        
        # 尝试设置，不成功也别闪退
        if target_path:
            try:
                self.iconbitmap(target_path)
            except:
                pass

    def _draw_watermark(self):
        # 清理旧水印
        self.canvas.delete("watermark")
        
        w = self.canvas.winfo_width()
        h = self.canvas.winfo_height()
        
        if w < 100 or h < 100:
            return  # 太小不画

        step_x = 200   # 水平间隔
        step_y = 150   # 垂直间隔
        font_size = 28 # 字号

        for x in range(0, w, step_x):
            for y in range(0, h, step_y):
                self.canvas.create_text(
                    x, y,
                    text=self.watermark_text,
                    font=("Microsoft YaHei", font_size, "bold"),
                    fill="#d0d0d0",
                    angle=30,   # 倾斜
                    tags="watermark"
                )

        # 确保水印始终在最上层
        self.canvas.tag_raise("watermark")


    def _on_mousewheel(self, ev):
        d = -1 if getattr(ev,'delta',0)>0 or ev.num==4 else 1
        self.canvas.yview_scroll(d,"units")

    def _on_click(self, ev):
        # 1) 退出自定义输入
        if self.custom:
            ent = self.custom
            x1,y1 = ent.winfo_rootx(), ent.winfo_rooty()
            x2,y2 = x1+ent.winfo_width(), y1+ent.winfo_height()
            if not (x1<=ev.x_root<=x2 and y1<=ev.y_root<=y2):
                # 将用户输入保存到 directive.selected
                for d in self.directives:
                    print(f"DEBUG: Rendering {d.kind} - {getattr(d, 'title', 'no-title')}") # 加这一行
                    if getattr(d,'entry',None) is ent:
                        d.selected = ent.get().strip() or d.selected
                        break
                ent.config(state="readonly")
                self.custom = None
                return

        # 2) 取消按键监听
        if self._waiting:
            _, ent = self._waiting
            if ev.widget is not ent:
                self._cancel_listen()

    def _cancel_listen(self):
        d, ent = self._waiting
        ent.config(state="readonly"); ent.delete(0,tk.END)
        ent.insert(0, d.selected); self._waiting = None

    def _on_keypress(self, ev):
        if not self._waiting:
            return

        d, ent = self._waiting
        ks = KEYMAP.get(ev.keysym, ev.keysym)

    # 忽略大小写修饰键并标准化
        if ks.lower() in ("shift_l", "control_l", "alt_l"):
            ks = ks.split("_")[0]
        elif ks.lower() in ("shift_r", "control_r", "alt_r"):
            ks = ks.split("_")[0] + "r"

    # 针对非英文字符直接保留（可自行扩展）
        valid_symbols = {'.', '/', '[', ']', ';', '-', '=', '\\'}
        if ks in valid_symbols:
            pass
        else:
            ks = ks.lower()

        # 如果不是字母数字下划线，统一为 "key"
            if not re.match(r'^[a-z0-9_]+$', ks):
                ks = "key"

        d.selected = ks
        ent.delete(0, tk.END)
        ent.insert(0, ks)
        ent.config(state="readonly")
        self._waiting = None


    def menu_open_yml(self):
        p = filedialog.askopenfilename(filetypes=[("Asulyml 文件", "*.asulyml")])
        if not p: return
        self.queue = [(p, os.path.dirname(p), "yml")] # 第三个参数改为标识
        self.next_file()

    def menu_open_json(self):
        p = filedialog.askopenfilename(filetypes=[("Asuljson 文件", "*.asuljson")])
        if not p: return
        # 修改：确保第三个参数是 "json" 字符串，而不是 True
        self.queue = [(p, os.path.dirname(p), "json")] 
        self.json_mode = True
        self.link_mode = False
        self.next_file()

    def menu_open_asul(self):
        p = filedialog.askopenfilename(filetypes=[("Asul", "*.asul")])
        if not p:
            return
        
        d = os.path.dirname(p)
        fs = sorted(f for f in os.listdir(d) if f.lower().endswith(".asul"))
        idx = fs.index(os.path.basename(p))
        
        # 为每个文件生成完整的输出路径
        self.queue = [
            (os.path.join(d, f), 
             os.path.join(d, os.path.splitext(f)[0] + ".cfg"),
             False)
            for f in fs[idx:]
        ]
        
        self.link_mode = False
        self.next_file()

    def menu_open_link(self):
        p = filedialog.askopenfilename(filetypes=[("Asulink", "*.asulink")])
        if not p:
            return

        base = os.path.dirname(p)
        lines = [l.strip() for l in open(p, encoding='utf-8') if l.strip()]
        self.queue = []
        self.link_mode = True
        default_dir = None

        i = 0
        while i < len(lines):
            parts = re.findall(r'"(.*?)"', lines[i])

            if len(parts) == 1 and default_dir is None:
                default_dir = os.path.normpath(os.path.join(base, parts[0]))
                i += 1
                continue

            if len(parts) == 2:
                src = os.path.normpath(os.path.join(base, parts[0]))
                out = os.path.normpath(os.path.join(base, parts[1]))
            elif len(parts) == 1:
                src = os.path.normpath(os.path.join(base, parts[0]))
                out = default_dir or base
            else:
                i += 1
                continue

            if not os.path.isdir(os.path.dirname(out)):
                out = os.path.join(default_dir or base, os.path.basename(out))

            # 判断输出后缀
            if os.path.isdir(out) or out.endswith(os.sep):
                fname = os.path.splitext(os.path.basename(src))[0]
                if src.endswith(".json") or src.endswith(".asuljson"):
                    ext = ".json"
                elif src.endswith(".yml") or src.endswith(".asulyml"):
                    ext = ".yml"
                else:
                    ext = ".cfg"
                full_out = os.path.join(out, fname + ext)
            else:
                full_out = out

            # 判定模式存入队列
            lname = src.lower()
            if lname.endswith(".asuljson") or lname.endswith(".json"):
                mode = "json"
            elif lname.endswith(".asulyml") or lname.endswith(".yml"):
                mode = "yml"
            else:
                mode = "asul"
            
            self.queue.append((src, full_out, mode))
            i += 1

        if not self.queue:
            messagebox.showerror("错误", ".asulink 文件无有效路径")
            return

        self.next_file()

    def next_file(self):
        # 1. 彻底清空当前 UI
        self.canvas.yview_moveto(0)
        for w in self.ui_widgets:
            w.destroy()
        self.ui_widgets.clear()
        
        if not self.queue:
            messagebox.showinfo("完成", "所有文件已配置完毕。")
            self.quit()
            return

        # 2. 弹出新任务
        self.cur, self.output_dir, mode = self.queue.pop(0) 
        
        
        # 3. 核心：强制根据后缀名和传入的 mode 重新判定
        # 这样能防止上一个文件的 json_mode 影响这一个
        self.json_mode = (mode == "json")
        self.yml_mode  = (mode == "yml")

        # 4. 执行解析
        try:
            if self.json_mode:
                self.directives = parse_asuljson(self.cur)
            elif self.yml_mode:
                self.directives = parse_asulyml(self.cur)
            else:
                self.directives = parse_asul(self.cur)
        except Exception as e:
            messagebox.showerror("解析失败", f"文件 {self.cur} 解析出错：\n{e}")
            self.next_file()
            return

        # 5. 初始化变量
        for d in self.directives:
            if d.kind == 'bool':
                d.var = tk.BooleanVar(value=bool(getattr(d, 'default', False)))
            elif d.kind == 'int':
                d.var = tk.IntVar(value=int(getattr(d, 'default', 0)))
            elif d.kind == 'float':
                d.var = tk.DoubleVar(value=float(getattr(d, 'default', 0.0)))
            elif d.kind == 'line':
                d.var = tk.StringVar(value=str(getattr(d, 'default', "")))

        self.build_ui()
        self.btn.config(state="normal")
        self.title(f"配置：{os.path.basename(self.cur)} ({mode})")


    def build_ui(self):
        """完全优化版：自动适配窗口宽度，填满右侧空间"""
        # 清理旧插件
        for w in self.ui_widgets:
            w.destroy()
        self.ui_widgets.clear()

        fname = self.default_font_name
        fsize = self.base_size

        for d in self.directives:
            if d.kind in ('raw', 'text'):
                continue

            d_title = getattr(d, 'title', getattr(d, 'name', "未知配置"))
            d_detail = getattr(d, 'detail', getattr(d, 'comment', ""))

            # --- 1. 处理 func 类型 (单选题) ---
            if d.kind == 'func':
                fr = tk.Frame(self.frm, pady=12, bg='#ffffff')
                fr.pack(fill='x', expand=True) # 允许横向铺满
                
                tk.Label(fr, text=str(d_title), font=(fname, fsize + 1, 'bold'), bg='#ffffff').pack(anchor="w", padx=15)
                if d_detail:
                    tk.Label(fr, text=str(d_detail), font=(fname, fsize - 1), fg="#666666", 
                             bg='#ffffff', wraplength=800, justify="left").pack(anchor="w", padx=15)
                
                default_val = str(getattr(d, 'default_key', ""))
                if not hasattr(d, 'var'): d.var = tk.StringVar(value=default_val)

                opt_container = tk.Frame(fr, bg='#ffffff')
                opt_container.pack(fill='x', padx=30, pady=5)

                for k, desc in getattr(d, 'options', []):
                    rb = tk.Radiobutton(opt_container, text=desc, value=k, variable=d.var, 
                                        font=(fname, fsize), bg='#ffffff', activebackground='#ffffff')
                    rb.pack(side='top', anchor='w', pady=2)
                
                tk.Frame(self.frm, height=1, bg="#E0E0E0").pack(fill='x', padx=20)
                self.ui_widgets.append(fr)

            # --- 2. 处理 int / float 类型 (滑块) ---
            elif d.kind in ('int', 'float'):
                fr = tk.Frame(self.frm, pady=12, bg='#ffffff')
                fr.pack(fill='x', expand=True)
                
                tk.Label(fr, text=f"{d_title}:", font=(fname, fsize + 1, 'bold'), bg='#ffffff').pack(anchor='w', padx=15)
                if d_detail:
                    tk.Label(fr, text=d_detail, font=(fname, fsize - 1), fg="#666666", bg='#ffffff').pack(anchor='w', padx=15)
                
                if not hasattr(d, 'var'):
                    df = getattr(d, 'default', 0)
                    d.var = tk.IntVar(value=int(df)) if d.kind == 'int' else tk.DoubleVar(value=float(df))
                
                mn, mx = getattr(d, 'minimum', 0), getattr(d, 'maximum', 100)
                step_val = getattr(d, 'step', 1) if d.kind == 'int' else getattr(d, 'step', 0.01)
                
                # 核心改进：去掉 length=600，改用 fill='x' 让滑块自适应
                slider = tk.Scale(fr, from_=mn, to=mx, orient='horizontal', 
                                  font=(fname, fsize - 2), bg='#ffffff', highlightthickness=0,
                                  resolution=step_val, variable=d.var)
                slider.pack(fill='x', padx=30, pady=5, expand=True) # 这里的 fill='x' 是关键
                
                tk.Frame(self.frm, height=1, bg="#E0E0E0").pack(fill='x', padx=20)
                self.ui_widgets.append(fr)

            # --- 3. 处理 bool 类型 ---
            elif d.kind == 'bool':
                fr = tk.Frame(self.frm, pady=12, bg='#ffffff')
                fr.pack(fill='x', expand=True)
                
                tk.Label(fr, text=f"{d_title}", font=(fname, fsize + 1, 'bold'), bg='#ffffff').pack(anchor='w', padx=15)
                if d_detail:
                    tk.Label(fr, text=d_detail, font=(fname, fsize - 1), fg="#666666", bg='#ffffff').pack(anchor='w', padx=15)
                
                if not hasattr(d, 'var'): d.var = tk.BooleanVar(value=bool(getattr(d, 'default', False)))
                
                btn_fr = tk.Frame(fr, bg='#ffffff')
                btn_fr.pack(anchor='w', padx=25, pady=5)
                tk.Radiobutton(btn_fr, text='启用', value=True, variable=d.var, font=(fname, fsize), bg='#ffffff').pack(side='left', padx=10)
                tk.Radiobutton(btn_fr, text='禁用', value=False, variable=d.var, font=(fname, fsize), bg='#ffffff').pack(side='left', padx=10)
                
                tk.Frame(self.frm, height=1, bg="#E0E0E0").pack(fill='x', padx=20)
                self.ui_widgets.append(fr)

            # --- 4. 处理 key 类型 ---
            elif d.kind == 'key':
                fr = tk.Frame(self.frm, pady=12, bg='#ffffff')
                fr.pack(fill='x', expand=True)
                
                tk.Label(fr, text=f"{d_title}", font=(fname, fsize + 1, 'bold'), bg='#ffffff').pack(anchor="w", padx=15)
                if d_detail:
                    tk.Label(fr, text=d_detail, font=(fname, fsize - 1), fg="#666666", bg='#ffffff').pack(anchor='w', padx=15)
                
                val = getattr(d, 'default_key', 'key')
                d.selected = val
                
                btn_fr = tk.Frame(fr, bg='#ffffff')
                btn_fr.pack(fill='x', padx=25, pady=5)
                
                ent = tk.Entry(btn_fr, width=20, font=(fname, fsize), state="readonly")
                d.entry = ent
                ent.config(state="normal"); ent.delete(0, tk.END); ent.insert(0, val); ent.config(state="readonly")
                ent.pack(side="left", padx=5, ipady=3) # 增加高度
                
                for txt, cmd in [("监听", lambda di=d, en=ent: self.start_listen(di, en)),
                                 ("默认", lambda di=d, en=ent: self.reset_default(di, en)),
                                 ("清空", lambda di=d, en=ent: self.clear_bind(di, en))]:
                    tk.Button(btn_fr, text=txt, font=(fname, fsize-1), command=cmd).pack(side="left", padx=3)
                
                tk.Frame(self.frm, height=1, bg="#E0E0E0").pack(fill='x', padx=20)
                self.ui_widgets.append(fr)

            # --- 5. 处理 line 类型 ---
            elif d.kind == 'line':
                fr = tk.Frame(self.frm, pady=12, bg='#ffffff')
                fr.pack(fill='x', expand=True)
                
                tk.Label(fr, text=f"{d_title}", font=(fname, fsize + 1, 'bold'), bg='#ffffff').pack(anchor="w", padx=15)
                if d_detail:
                    tk.Label(fr, text=d_detail, font=(fname, fsize - 1), fg="#666666", bg='#ffffff').pack(anchor="w", padx=15)
                
                if not hasattr(d, 'var'): d.var = tk.StringVar(value=str(getattr(d, 'default', "")))
                
                # 核心改进：fill='x' 让输入框拉长，ipady 让输入框变厚
                ent = tk.Entry(fr, textvariable=d.var, font=(fname, fsize))
                ent.pack(fill="x", padx=30, pady=8, ipady=5) 
                
                tk.Frame(self.frm, height=1, bg="#E0E0E0").pack(fill='x', padx=20)
                self.ui_widgets.append(fr)

        # 刷新滚动区
        self.update_idletasks()
        self.canvas.config(scrollregion=self.canvas.bbox("all"))


    def start_custom(self, entry, directive):
        # 进入自定义输入
        self.custom = entry
        entry.config(state="normal")

    def start_listen(self, directive, entry):
        # 进入按键监听
        self._waiting = (directive, entry)
        entry.config(state="normal"); entry.delete(0,tk.END); entry.insert(0,"…")

    def reset_default(self, directive, entry):
        directive.selected = directive.default_key or "key"
        entry.config(state="normal")
        entry.delete(0,tk.END); entry.insert(0,directive.selected)
        entry.config(state="readonly")

    def _after_generation(self):
        if self.queue:
            if self.link_mode:  # 仅asulink模式自动继续
                self.next_file()
            else:  # asul和asuljson模式询问用户
                if messagebox.askyesno("继续？", f"还有 {len(self.queue)} 个文件待处理，是否继续？"):
                    self.next_file()
                else:
                    self.queue = []  # 清空队列但不退出
        else:
            if self.link_mode:  # asulink模式处理完退出
                self.quit()
            else:  # asul和asuljson模式不自动退出
                self.title(f"{self.title()} [已完成]")
                self.btn.config(state="disabled")


    def clear_bind(self, directive, entry):
        directive.selected = "key"
        entry.config(state="normal")
        entry.delete(0,tk.END); entry.insert(0,"key")
        entry.config(state="readonly")

    def on_generate(self):
        # 1. 首先同步所有 UI 变量到指令对象的 default 属性中
        # 这样无论哪种 mode，后端的 write 函数都能直接读取 d.default
        for d in self.directives:
            if hasattr(d, 'var'):
                try:
                    d.default = d.var.get()
                except Exception:
                    pass # 防止某些非数值输入导致 get() 崩溃
            # 针对 key 类型的特殊处理
            if d.kind == 'key' and hasattr(d, 'selected'):
                d.default_key = d.selected

        # --- 分模式处理生成 ---

        # A. YAML 模式 (强制排他，防止进入标准 ASUL 逻辑)
        if self.yml_mode:
            yml_out = self.output_dir
            if os.path.isdir(yml_out) or yml_out.endswith(os.sep):
                fname = os.path.splitext(os.path.basename(self.cur))[0]
                yml_out = os.path.join(yml_out, fname + ".yml")
            
            try:
                write_yml(yml_out, self.directives)
                save_asulyml(self.cur, self.directives)
                messagebox.showinfo("完成", f"已生成 YAML：\n{yml_out}")
            except Exception as e:
                messagebox.showerror("写入失败", f"YAML 写入出错：{e}")

        # B. JSON 模式
        elif self.json_mode:
            json_out = self.output_dir
            # 备份
            bak_dir = os.path.join(os.path.dirname(self.cur), "backup")
            os.makedirs(bak_dir, exist_ok=True)
            if os.path.isfile(self.cur):
                shutil.copy2(self.cur, bak_dir)
            
            # 保存 .asuljson 源码更新
            save_asuljson(self.cur, self.directives)
            # 导出正式 .json
            write_json(json_out, self.directives)
            messagebox.showinfo("完成", f"已生成 JSON：\n{json_out}")

        # C. 标准 ASUL (.cfg) 模式
        else:
            if os.path.isdir(self.output_dir) or self.output_dir.endswith(os.sep):
                base_name = os.path.splitext(os.path.basename(self.cur))[0]
                tgt = os.path.join(self.output_dir, base_name + ".cfg")
            else:
                tgt = self.output_dir
            
            # 检查冲突
            conf = check_conflicts(self.directives)
            if conf:
                msg = "检测到冲突：\n" + "\n".join(f"{k}→{','.join(v)}" for k, v in conf.items())
                messagebox.showerror("冲突", msg)
                return

            # 注意：如果 write_cfg 报错，请检查 asulwriter.py
            # 建议将 asulwriter.py 里的 v = d.entry.get() 改为 v = str(d.default)
            try:
                write_cfg(tgt, self.directives)
                save_asul(self.cur, self.directives)
                messagebox.showinfo("完成", f"已生成：\n{tgt}")
            except Exception as e:
                messagebox.showerror("错误", f"CFG 生成失败，请检查 asulwriter.py 是否兼容新版数据结构。\n错误信息：{e}")

        # 统一收尾（进入下一个文件或退出）
        self._after_generation()


if __name__=="__main__":
    p = argparse.ArgumentParser()
    p.add_argument("-asul")
    p.add_argument("-asulink")
    p.add_argument("-asuljson") 
    p.add_argument("-asulyml")
    a = p.parse_args()
    


    queue=[]; cli=False; link=False
    if a.asul:
        base=os.path.dirname(a.asul)
        fs=sorted(f for f in os.listdir(base) if f.lower().endswith(".asul"))
        idx=fs.index(os.path.basename(a.asul))
        queue = [(os.path.join(base, f), base, False) for f in fs[idx:]]  # 这里添加第三个参数 False
        cli=True
    elif a.asulink:
        base = os.path.dirname(a.asulink)
        lines = [l.strip() for l in open(a.asulink, encoding='utf-8') if l.strip()]
        queue = []
        link = True
        default_dir = None
        for line in lines:
            parts = re.findall(r'"(.*?)"', line)
            if len(parts) == 1 and default_dir is None:
                default_dir = os.path.normpath(os.path.join(base, parts[0]))
                continue
            if len(parts) == 2:
                src = os.path.normpath(os.path.join(base, parts[0]))
                out = os.path.normpath(os.path.join(base, parts[1]))
            elif len(parts) == 1:
                src = os.path.normpath(os.path.join(base, parts[0]))
                out = default_dir or base
            else:
                continue
            if not os.path.isdir(os.path.dirname(out)):
                out = os.path.join(default_dir or base, os.path.basename(out))
            if os.path.isdir(out) or out.endswith(os.sep):
                fname = os.path.splitext(os.path.basename(src))[0]
                # --- 修正识别逻辑 ---
                if src.endswith(".json"):
                    ext = ".json"
                elif src.endswith(".yml") or src.endswith(".asulyml"):
                    ext = ".yml"
                else:
                    ext = ".cfg"
                full_out = os.path.join(out, fname + ext)
            else:
                full_out = out
            
            # --- 关键：使用 mode 字符串而不是 is_json 布尔值 ---
            lname = src.lower()
            if lname.endswith(".asuljson") or lname.endswith(".json"):
                mode = "json"
            elif lname.endswith(".asulyml") or lname.endswith(".yml"):
                mode = "yml"
            else:
                mode = "asul"
            queue.append((src, full_out, mode))

    elif a.asuljson:
        queue = [(a.asuljson, os.path.dirname(a.asuljson), True)]
        cli = True
        json_mode = True

    app=AsulUI(queue=queue,cli=cli,link=link)
    app.mainloop()
