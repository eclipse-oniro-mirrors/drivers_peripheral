# !/usr/bin/env python
# -*- coding: utf-8 -*-
 
#
# Copyright (c) 2026 Huawei Device Co., Ltd.
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#

"""
初始化态管理器 — 设备端标志文件实现APP首次初始化弹窗自适应跳过

用法:
    from shared.init_manager import InitManager
    init_mgr = InitManager(driver)
    init_mgr.dismiss_init_dialogs("camera", ["同意", "跳过", "我知道了", "下一步", "允许"])

原理:
    - 标志文件目录: /data/local/tmp/hdf_auto_init_flags/<app_name>.done
    - 首次运行: 处理弹窗 -> 写标志文件
    - 二次执行: 检测标志文件存在 -> 跳过所有弹窗处理
    - 烧录时 /data/local/tmp/ 清空,重启不清空
"""

from hypium import BY

FLAG_DIR = "/data/local/tmp/hdf_auto_init_flags"


class InitManager:
    def __init__(self, driver):
        self.driver = driver
        self.driver.shell(f"mkdir -p {FLAG_DIR}")

    def is_app_initialized(self, app_name):
        """检查APP是否已完成首次初始化(标志文件存在=已初始化)"""
        out = self.driver.shell(
            f"ls {FLAG_DIR}/{app_name}.done 2>/dev/null && echo EXISTS || echo NOT_EXISTS"
        )
        return "EXISTS" in out

    def mark_app_initialized(self, app_name, log_func=None):
        """标记APP已完成首次初始化,写入标志文件"""
        self.driver.shell(f"touch {FLAG_DIR}/{app_name}.done")
        if log_func:
            log_func(f"[初始化] 已标记 {app_name} 完成首次初始化")

    def dismiss_init_dialogs(self, app_name, button_texts, log_func=None):
        """简化的初始化弹窗处理(自适应跳过)

        参数:
            app_name: APP名称,用于标志文件(如"camera")
            button_texts: 按钮文字列表,按优先级依次尝试
            log_func: 日志输出函数(如MESSAGE),可选
        """
        def log(msg):
            if log_func:
                log_func(msg)

        if self.is_app_initialized(app_name):
            log(f"[初始化] {app_name} 标志文件存在,快速探测弹窗是否实际出现")
            # 标志文件存在,但仍快速探测弹窗是否实际出现
            # 防止新烧录设备标志文件残留导致跳过必要的弹窗处理
            probe_btn = self.driver.find_component(BY.text(button_texts[0]))
            if probe_btn:
                log(f"[初始化] {app_name} 弹窗实际存在(标志文件可能残留),处理弹窗")
            else:
                log(f"[初始化] {app_name} 无弹窗,跳过处理")
                return

        log(f"[初始化] {app_name} 处理初始化弹窗")
        clicked_any = False
        for btn_text in button_texts:
            btn = self.driver.find_component(BY.text(btn_text))
            if btn:
                log(f"[初始化] 找到按钮'{btn_text}',点击")
                btn.click()
                self.driver.wait(1)
                clicked_any = True

        if clicked_any:
            log(f"[初始化] {app_name} 弹窗已处理")
        else:
            log(f"[初始化] {app_name} 未检测到初始化弹窗,可能已预置完成")

        # 无论是否点击了按钮,都标记为已初始化
        # 避免二次执行时误点界面上同文字的其他元素
        self.mark_app_initialized(app_name, log_func)

    def handle_init_dialog(self, app_name, dialog_configs, log_func=None):
        """详细的初始化弹窗处理(带弹窗容器双重确认)
        
        参数:
            app_name: APP名称
            dialog_configs: 初始化弹窗配置列表,每项含:
                - container_key: 弹窗容器的key/id(可选)
                - container_text: 弹窗容器的text(备选,可选)
                - button_text: 需点击的按钮文字
                - wait_after: 点击后等待时间(秒,默认1)
                - optional: 是否可选(找不到也不报错,默认True)
        """
        if self.is_app_initialized(app_name):
            if log_func:
                log_func(f"[初始化] {app_name} 已初始化过,跳过弹窗处理")
            return True

        if log_func:
            log_func(f"[初始化] {app_name} 首次运行,处理初始化弹窗")
        all_handled = True
        for cfg in dialog_configs:
            button_text = cfg.get("button_text", "")
            wait_after = cfg.get("wait_after", 1)
            optional = cfg.get("optional", True)
            
            container = self._find_dialog_container(cfg)
            if container:
                if self._click_dialog_button(button_text, wait_after, log_func):
                    continue
                if log_func:
                    log_func(f"[初始化] 找到弹窗容器但未找到按钮'{button_text}'")
                if not optional:
                    all_handled = False
            else:
                if log_func:
                    log_func(f"[初始化] 未找到弹窗容器,尝试直接找按钮")
                self._click_dialog_button(button_text, wait_after, log_func)

        self.mark_app_initialized(app_name, log_func)
        return all_handled

    def reset_all_flags(self):
        """清除所有初始化标志文件(用于重新触发首次初始化测试)"""
        self.driver.shell(f"rm -f {FLAG_DIR}/*.done")

    def _find_dialog_container(self, cfg):
        """根据配置查找弹窗容器(先key后text)"""
        container_key = cfg.get("container_key", "")
        container_text = cfg.get("container_text", "")
        if container_key:
            container = self.driver.find_component(BY.key(container_key))
            if container:
                return container
        if container_text:
            return self.driver.find_component(BY.text(container_text))
        return None

    def _click_dialog_button(self, button_text, wait_after, log_func):
        """查找并点击弹窗按钮,返回是否成功"""
        btn = self.driver.find_component(BY.text(button_text))
        if btn:
            if log_func:
                log_func(f"[初始化] 找到按钮'{button_text}',点击")
            btn.click()
            self.driver.wait(wait_after)
            return True
        return False
