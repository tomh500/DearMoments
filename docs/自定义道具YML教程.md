 # 🧩 自定义道具投掷使用说明  
  我们一直致力于为用户提供安全、便捷的项目服务。  
  现在，支持添加自定义道具投掷功能，使用方法非常简单！

 ## 📂 打开配置文件夹  
 - 在CFG监听器左上角，点击「文件菜单」中的「自定义道具投掷」即可快速打开配置文件夹。  
 - 也可以手动打开目录：`DearMoments/src/CS2/main/SQ/CustomItems`  

 ## 🎯 添加自定义道具步骤  
 1. 在本地房间里瞄准好点位后，打开游戏控制台输入命令：  
    ```bash
    getpos
    ```  
 2. 获取到的坐标数据放入配套的计算器。  
 3. 编辑 `Custom.yml` 文件，按照规范格式，轻松添加你的自定义道具。  

 ## 📝 Custom.yml 格式说明  

 ```yaml
  <道具唯一标识符ID>:
  filename: "道具文件名，是CFG可执行文件的名字"
  displayname: "道具对外显示的名字"
  map: "隶属地图，目前支持 Dust2、Inferno、Mirage、Ancient、Nuke、Anubis、Overpass、Vertigo"
  sensitivity: "瞄准时使用的灵敏度，使用配套计算器请填写示例：2.52，生成器强制要求 m_yaw 0.022"
  yaw: "X轴方向移动数值，通过 getpos 搭配计算器计算得出"
  pitch: "Y轴方向移动数值，通过 getpos 搭配计算器计算得出"
  type: "道具类型，可选值：smoke（烟雾弹）、molo（燃烧弹）、flash（闪光弹）、grenade（手榴弹）、decoy（诱饵弹）"
  throwmode: "投掷方式，支持 Normal（直接扔）、Jump（跳投）、ForwardJump（前跳投）、Custom（自定义，需在 extra 填写投掷命令）"
  extra: ["例如需要下蹲，在瞄准时填写 +duck","例如投掷后恢复站立，填写 -duck"]
  select:
   - page: "轮盘分页，可选 1、2、3 页"
   - slot: "轮盘槽位，可选 1、2、3、4、5 槽位"
   - command: "注册的用户空间命令，格式为 /items_<道具唯一标识符>_set"
   - bind: "绑定按键，可自动道具描点并送出正确设置的道具，也可以不绑定，等价于在控制台输入 items_<道具唯一标识符>_set"
  setpos:
   - x: "坐标1，getpos得到的setpos第1个参数，可留空，主要用于本地服务器传送坐标"
   - z: "坐标2，getpos得到的setpos第2个参数，可留空，主要用于本地服务器传送坐标"
   - y: "坐标3，getpos得到的setpos第3个参数，可留空，主要用于本地服务器传送坐标"
 ```

 ## ⚠️ 重要提示  
 - 请搭配最新版本的 **DearMoments-X** 使用本功能，确保兼容性与功能完整性。  
 - 通过配套计算器填写的灵敏度和坐标，能够保证道具投掷精准度。  
 - 投掷方式 `throwmode` 支持多种模式，灵活应对不同需求。  
 - `extra` 字段可以填写自定义指令（如投掷时下蹲），增强投掷动作的多样性。  
 - `select` 字段支持轮盘分页与槽位绑定，方便快速选择自定义道具。  
 - `setpos` 主要用于本地服务器传送，普通用户可留空。  

 ---  
  以上为自定义道具投掷的完整说明，祝你玩得开心！有任何疑问，欢迎随时咨询。
