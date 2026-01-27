const VK_MAP = { 32: "空格", 65: "A", 66: "B", 1: "左键", 2: "右键", 16: "Shift", 17: "Ctrl" };
const dropZone = document.getElementById('dropZone');
const cppPreview = document.getElementById('cppPreview');

// 拖拽添加积木
document.querySelectorAll('.item').forEach(item => {
    item.onclick = () => addBlock(item.dataset.type, item.dataset.val);
});

function addBlock(type, val) {
    const div = document.createElement('div');
    div.className = `block ${type}`;
    div.dataset.type = type;
    
    let content = `<span class="del" onclick="this.parentElement.remove();update();">×</span>`;
    
    if (type === 'SCROLL') {
        content += `滚轮下滚 强度: <input type="number" class="v" value="-120"> 延迟: <input type="number" class="d" value="20">`;
    } else if (type === 'KEY') {
        content += `点击按键: <select class="v">${Object.entries(VK_MAP).map(([k,v])=>`<option value="${k}">${v}</option>`).join('')}</select> 延迟: <input type="number" class="d" value="20">`;
    } else if (type === 'IF') {
        content += `如果按下: <select class="v">${Object.entries(VK_MAP).map(([k,v])=>`<option value="${k}">${v}</option>`).join('')}</select> { ... }`;
    } else if (type === 'LOOP') {
        content += `循环执行: <input type="number" class="v" value="5"> 次 { ... }`;
    } else if (type === 'CODE') {
        content += `原生代码: <input type="text" class="v" style="width:70%" value="std::cout << 'Hello';" >`;
    }

    div.innerHTML = content;
    div.oninput = update;
    dropZone.appendChild(div);
    update();
}

function update() {
    const code = generateFullCpp();
    cppPreview.textContent = code;
}

function generateFullCpp() {
    const blocks = Array.from(dropZone.children);
    let mySteps = [];
    let customLines = [];

    blocks.forEach(b => {
        const type = b.dataset.type;
        const v = b.querySelector('.v').value;
        const d = b.querySelector('.d')?.value || 0;

        if (type === 'SCROLL') {
            mySteps.push(`        { ACTION_MOUSE_SCROLL, ${v}, ${parseFloat(d).toFixed(1)} }`);
        } else if (type === 'KEY') {
            mySteps.push(`        { ACTION_KEY_TAP, ${v}, ${parseFloat(d).toFixed(1)} }`);
        } else if (type === 'IF') {
            customLines.push(`        if (GetAsyncKeyState(${v}) & 0x8000) { /* 逻辑 */ }`);
        } else if (type === 'LOOP') {
            customLines.push(`        for(int i=0; i<${v}; ++i) { /* 逻辑 */ }`);
        } else if (type === 'CODE') {
            customLines.push(`        ${v}`);
        }
    });

    return `namespace UserMacro {
    int TRIGGER_KEY = VK_SPACE;
    
    // [自动化数组]
    MacroStep mySteps[] = {
${mySteps.join(',\n')}
    };
    int stepCount = sizeof(mySteps) / sizeof(MacroStep);

    // [逻辑钩子]
    void Custom() {
${customLines.join('\n')}
    }
}`;
}

document.getElementById('exportBtn').onclick = () => {
    const blob = new Blob([generateFullCpp()], {type: 'text/plain'});
    const a = document.createElement('a');
    a.href = URL.createObjectURL(blob);
    a.download = 'UserMacro.cpp';
    a.click();
};