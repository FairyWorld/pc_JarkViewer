#include "jarkUtils.h"




// 界面字符串表，暂时仅支持中文和英文
// 不要随意增减条目，索引号对应 stringID 使用
// 条目所在行号减10即为 stringID
const char* const UIStringTable[][2] = {
    {"NULL", "NULL"},
    {"设置", "Settings"},
    {"常规", "General"},
    {"文件关联", "Association"},
    {"帮助", "Help"},
    {"关于", "About"},
    {"常见格式", "Common Formats"},  
    {"选择常用", "Select Common"},
    {"全选", "Select All"},
    {"全不选", "Deselect All"},
    {"立即关联", "Apply Association"},
    {"本软件原生绿色单文件，请把软件放置到合适位置再关联文件格式，若软件位置变化则需重新关联。\n若不再使用本软件，请点击【全不选】再点击【立即关联】即可移除所有关联关系。", "This software is a portable single file. Please place the software in an appropriate location before associating file formats." },
    {"旋转动画", "Rotate Animation"},
    {"缩放动画", "Zoom Animation"},
    {"平移图像加速 (拖动图像时优化渲染速度，图像会微微失真)", "Optimize Slide"},
    {"删除前提示", "Confirm Before Delete"},
    {"切换动画模式", "Switch Animation Mode"},
    {"幻灯片顺序", "Slideshow Order"},
    {"幻灯片间隔(秒)", "Slideshow Interval (seconds)"},
    {"xx", "xx"},
    {"切图动画", "SwitchAnim"},
    {"无动画", "None"},
    {"上下滑动", "Vertical"},
    {"左右滑动", "Horizontal"},
    {"界面主题", "Theme"},
    {"跟随系统", "System"},
    {"浅色", "Light"},
    {"深色", "Dark"},
    {"语言Lang", "语言Lang"},
    {"自动Auto", "自动Auto"},
    {"中文", "中文"},
    {"English", "English"},
};

const char* const getUIString(int stringidx) {
    int langIdx = GlobalVar::settingParameter.UI_LANG == 0 ?
        (jarkUtils::isSystemUILanguageChinese() ? 0 : 1) :
        (GlobalVar::settingParameter.UI_LANG - 1);
    if (langIdx < 0 || langIdx > 1) // 目前仅中英，索引范围0~1
        langIdx = 0;
    return UIStringTable[stringidx][langIdx];
}