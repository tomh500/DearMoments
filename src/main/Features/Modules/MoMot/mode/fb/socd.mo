//通过定义+socd_forward...为实际命令 让其能正确获取移动状态 并发送命令 否则发送错误命令 也就是不会snaptap

//用于定义按下另一个按键而松开另一个按键的内容
alias socd_switch_cancel_forward -forward
alias socd_switch_cancel_back -back


//用于定义发送的内容
alias socd_switch_forward +forward
alias socd_switch_back +back
