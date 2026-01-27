(function() {

document.addEventListener('click', (e) => {
    if (e.target.classList.contains('warning-macro')) {
        alert("🚨 风险告知：\n\n该功能在 DearMacro 中并非原生指令实现，而是通过宏脚本完成。\n虽然它能达到相同效果，但极易被服务器主办方定性为作弊违规。\n\n建议咨询服务器主办方相关信息，同时，我们也并不提倡您在竞技比赛服务器上使用本功能。");
    }
});

    const list = document.querySelector('.full-list ul');
    if (!list) return;

    // 获取所有列表项并转为数组
    const items = Array.from(list.children);

    items.sort((a, b) => {
        // 定义权重：普通(0) < 警告(1) < 错误(2)
        const getWeight = (el) => {
            if (el.classList.contains('confirmed-invalid')) return 2;
            if (el.classList.contains('warning-macro')) return 1;
            return 0;
        };

        return getWeight(a) - getWeight(b);
    });

    // 清空列表并按新顺序重新插入
    // 增加淡入动画效果
    list.innerHTML = '';
    items.forEach((item, index) => {
        item.style.animationDelay = `${index * 0.05}s`;
        list.appendChild(item);
    });
})();