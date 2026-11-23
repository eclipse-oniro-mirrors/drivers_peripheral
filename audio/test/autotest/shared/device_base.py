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
设备基础操作 — 屏幕解锁/唤醒/灭屏/超时设置/清理后台等通用操作

用法:
    from shared.device_base import DeviceBase
    dev = DeviceBase(driver)
    dev.unlock()
    dev.set_screen_timeout(86400000)
    dev.clear_recent_tasks()
"""

from hypium import BY, KeyCode


class DeviceBase:
    def __init__(self, driver):
        self.driver = driver

    def unlock(self):
        """解锁屏幕(唤醒+上滑)"""
        self.driver.unlock()

    def wake_up(self):
        """唤醒屏幕"""
        self.driver.wake_up_display()

    def close_display(self):
        """灭屏"""
        self.driver.close_display()

    def is_display_on(self):
        """检查屏幕是否亮着"""
        return self.driver.is_display_on()

    def set_screen_timeout(self, timeout_ms=86400000):
        """设置屏幕超时时间(毫秒),默认24小时不灭屏"""
        self.driver.shell(f"power-shell timeout -o {timeout_ms}")

    def clear_recent_tasks(self):
        """清理后台任务"""
        self.driver.press_recent_task()
        clear_btn = self.driver.find_component(BY.key("RecentClearAllView_Stack_deleteFull"))
        if clear_btn:
            clear_btn.click()
        else:
            self.driver.press_key(KeyCode.HOME)

    def go_home(self):
        """回到桌面"""
        self.driver.press_key(KeyCode.HOME)

    def take_screenshot(self, filename):
        """截图(保存到设备端,后续可hdc file recv拉取)"""
        self.driver.capture_screen(filename)

    def shell(self, cmd):
        """执行shell命令"""
        return self.driver.shell(cmd)

    def start_app(self, package_name, page_name=None):
        """启动APP"""
        if page_name:
            self.driver.start_app(package_name=package_name, page_name=page_name)
        else:
            self.driver.start_app(package_name=package_name)

    def find_and_click(self, by_selector, wait=1, screenshot_on_fail=None):
        """查找元素并点击,找不到返回False

        参数:
            by_selector: BY条件(如BY.text(“同意”))
            wait: 点击后等待时间(秒)
            screenshot_on_fail: 找不到时截图文件名(可选)

        返回: True=点击成功, False=未找到
        """
        comp = self.driver.find_component(by_selector)
        if comp:
            comp.click()
            self.driver.wait(wait)
            return True
        if screenshot_on_fail:
            self.driver.capture_screen(screenshot_on_fail)
        return False

    def find_and_click_by_text(self, text, wait=1, screenshot_on_fail=None):
        """按文字查找并点击"""
        return self.find_and_click(BY.text(text), wait, screenshot_on_fail)

    def find_and_click_by_key(self, key, wait=1, screenshot_on_fail=None):
        """按key查找并点击"""
        return self.find_and_click(BY.key(key), wait, screenshot_on_fail)

    def dismiss_usb_popup(self, log_func=None):
        """关闭USB连接方式弹窗(reboot后常见)
        
        依次探测并点击“仅充电”/“取消”/“确定”等按钮关闭弹窗。
        无弹窗时静默返回。
        """
        def log(msg):
            if log_func:
                log_func(msg)

        usb_buttons = ["仅充电", "取消", "确定", "不传输", "关闭"]
        for btn_text in usb_buttons:
            btn = self.driver.find_component(BY.text(btn_text))
            if btn:
                log(f"[USB弹窗] 找到按钮'{btn_text}', 点击关闭")
                btn.click()
                self.driver.wait(1)
                return True

        log("[USB弹窗] 未检测到USB连接方式弹窗")
        return False
